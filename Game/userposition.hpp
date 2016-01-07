#ifndef USERPOSITION
#define USERPOSITION

#include <boost/asio.hpp>


//#include "transfer.hpp"
#include "postgresqlstruct.h"
#include "./../server.h"
#include "session.hpp"
#include "./../memManage/diskdbmanager.h"
#include "./../utils/stringbuilder.hpp"
#include "./../Monster/monster.h"
#include "./../NetWork/tcpconnection.h"
#include "./../GameAttack/gameattack.h"
#include "./../GameTask/gametask.h"
#include "./../TeamFriend/teamfriend.h"
#include "./../PlayerLogin/playerlogin.h"

using boost::asio::ip::tcp;

class UserPosition
{
public:
    static STR_PackPlayerPosition GetUserPos( hf_uint32 usr )
    {
        STR_PackPlayerPosition pos;
        StringBuilder  sbd;
        //std::ostringstream os;
        sbd<<"select pos_x,pos_y,pos_z,direct,mapid,actid from t_playerposition where roleid="<<usr<<";";
        Server::GetInstance()->getDiskDB()->GetPlayerInitPos(&pos, sbd.str());
        return pos;
    }

    //用户登陆成功后推送用户的位置
    static void Position_push( TCPConnection::Pointer conn, hf_uint32 roleid)
    {
        STR_PackPlayerPosition pos = GetUserPos( roleid );
        char buf[512] = {0};
        memcpy(buf,&pos,sizeof(pos));
        conn->Write_all(buf,sizeof(pos));
        //保存玩家位置
        SessionMgr::Instance()->SaveSession(conn, &pos);        
    }

    //
    //用户位置更新
    //
    static void UserPositionUpdate( TCPConnection::Pointer sk , void *pack)
    {
        STR_PackPlayerPosition *pos = (STR_PackPlayerPosition*)pack;

        SessionMgr::Instance()->SaveSession( sk,pos);
        BroadCastUserPosition(sk,pos);
        Server::GetInstance()->free(pack);
    }


    //用户位置移动
    static void PlayerMove(TCPConnection::Pointer conn, STR_PackPlayerPosition* pos)
    {
        SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
        memcpy(&((*smap)[conn].m_position), pos, sizeof(STR_PackPlayerPosition));
//        conn->Write_all(pos, sizeof(STR_PackPlayerPosition));
        //给可视范围内的玩家发送位置
        Session* sess = &(*smap)[conn];
        sess->SendPositionToViewRole(pos);
//        BroadCastUserPosition(conn, pos);
        STR_PlayerStartPos* startPos = &(*smap)[conn].m_StartPos;
        hf_float dx = startPos->Pos_x - pos->Pos_x,
            dy = startPos->Pos_y - pos->Pos_y,
            dz = startPos->Pos_z - pos->Pos_z;
        if(dx*dx + dy*dy + dz*dz >= RefreshDistance*RefreshDistance)
        {
            startPos->Pos_x = pos->Pos_x;
            startPos->Pos_x = pos->Pos_x;
            startPos->Pos_x = pos->Pos_x;
            //刷新新看到的怪和离开可视范围的怪
            Server::GetInstance()->GetMonster()->PushViewMonsters(conn);
            //刷新新看到的玩家和离开可视范围的的玩家
            Server::GetInstance()->GetPlayerLogin()->SendViewRole(conn);
        }
        Server::GetInstance()->free(pos);
    }

    static void PlayerDirectChange(TCPConnection::Pointer conn, hf_float direct)
    {       
        SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
        (*smap)[conn].m_position.Direct = direct;
        BroadCastUserDirect(conn, direct);
    }

    static void PlayerActionChange(TCPConnection::Pointer conn, hf_uint8 action)
    {
        SessionMgr::SessionPointer smap = SessionMgr::Instance ()->GetSession ();
        (*smap)[conn].m_position.ActID = action;
        BroadCastUserAction(conn, action);
    }

    //用户位置移动
    static void PlayerPositionMove(TCPConnection::Pointer conn, STR_PlayerMove* Move)
    {
        SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();

        STR_PackPlayerPosition* pos = &((*smap)[conn].m_position);
        pos->Direct = Move->Direct;
        pos->ActID = Move->ActID;
        hf_uint16 move_speed = (*smap)[conn].m_roleInfo.Move_Speed;
        switch(Move->Opt)
        {
        case 0:
        {
            conn->Write_all(pos, sizeof(STR_PackPlayerPosition));
            //给可视范围内的玩家发送位置
            BroadCastUserPosition(conn, pos);
            Server::GetInstance()->free(Move);
            return;
        }
        case 1:   //向前移动
        {
            pos->Pos_x = pos->Pos_x + PlayerMoveDistance * move_speed/100 * cos(pos->Direct);
            pos->Pos_z = pos->Pos_z + PlayerMoveDistance * move_speed/100 * sin(pos->Direct);
            break;
        }
        case 2: //向后移动
        {
            pos->Pos_x = pos->Pos_x + PlayerMoveDistance * move_speed/100 * cos(pos->Direct + PI);
            pos->Pos_z = pos->Pos_z + PlayerMoveDistance * move_speed/100 * sin(pos->Direct + PI);
            break;
        }
        case 3://向左移动
        {
            pos->Pos_x = pos->Pos_x + PlayerMoveDistance * move_speed/100 * cos(pos->Direct + PI/2);
            pos->Pos_z = pos->Pos_z + PlayerMoveDistance * move_speed/100 * sin(pos->Direct + PI/2);
            break;
        }
        case 4://向右移动
        {
            pos->Pos_x = pos->Pos_x + PlayerMoveDistance * move_speed/100 * cos(pos->Direct + PI*3/2);
            pos->Pos_z = pos->Pos_z + PlayerMoveDistance * move_speed/100 * sin(pos->Direct + PI*3/2);
            break;
        }
        default:
            Logger::GetLogger()->Info("Unkown derect pack");
            Server::GetInstance()->free(Move);
            return;
        }

//        conn->Write_all(pos, sizeof(STR_PackPlayerPosition));
        //给可视范围内的玩家发送位置       
        BroadCastUserPosition(conn, pos);

        STR_PlayerStartPos* startPos = &(*smap)[conn].m_StartPos;
        hf_float dx = startPos->Pos_x - pos->Pos_x,
                 dy = startPos->Pos_y - pos->Pos_y,
                 dz = startPos->Pos_z - pos->Pos_z;
        if(dx*dx + dy*dy + dz*dz >= RefreshDistance*RefreshDistance)
        {
            startPos->Pos_x = pos->Pos_x;
            startPos->Pos_x = pos->Pos_x;
            startPos->Pos_x = pos->Pos_x;
            //刷新新看到的怪和离开可视范围的怪
            Server::GetInstance()->GetMonster()->PushViewMonsters(conn);
            //刷新新看到的玩家和离开可视范围的的玩家
            Server::GetInstance()->GetPlayerLogin()->SendViewRole(conn);
        }
        Server::GetInstance()->free(Move);
    }

    //
    //向其他在线玩家(可是范围内)广播当前玩家的位置
    //
    static void BroadCastUserPosition( TCPConnection::Pointer conn, STR_PackPlayerPosition *pos)
    {
        SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();
        STR_PackOtherPlayerPosition OtherPos((*smap)[conn].m_roleid, pos);

        umap_roleSock  viewRole = (*smap)[conn].m_viewRole;
        for(_umap_roleSock::iterator it = viewRole->begin(); it != viewRole->end(); it++)
        {
            it->second->Write_all(&OtherPos, sizeof(STR_PackOtherPlayerPosition));
        }
    }

    static void BroadCastUserDirect(TCPConnection::Pointer conn, hf_float direct)
    {
        SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();

        hf_uint32 roleid = (*smap)[conn].m_roleid;
        STR_PackOtherPlayerDirect playerDirect(roleid, direct);
        umap_roleSock  viewRole = (*smap)[conn].m_viewRole;
//        printf("roleid:%u, direct:%f\n", playerDirect.roleid, playerDirect.direct);
//        cout << "roleid:" << playerDirect.roleid << ",direct:" << playerDirect.direct << endl;
        for(_umap_roleSock::iterator it = viewRole->begin(); it != viewRole->end(); it++)
        {
            it->second->Write_all(&playerDirect, sizeof(STR_PackOtherPlayerDirect));
        }
    }

    static void BroadCastUserAction(TCPConnection::Pointer conn, hf_uint8 action)
    {
        SessionMgr::SessionPointer smap =  SessionMgr::Instance()->GetSession();

        hf_uint32 roleid = (*smap)[conn].m_roleid;
        STR_PackOtherPlayerAction playerAction(roleid, action);
        umap_roleSock  viewRole = (*smap)[conn].m_viewRole;
//        printf("roleid:%u, action:%d\n", playerAction.roleid, playerAction.action);
//        cout << "roleid:" << playerAction.roleid << ",action:" << playerAction.action << endl;
        for(_umap_roleSock::iterator it = viewRole->begin(); it != viewRole->end(); it++)
        {
            it->second->Write_all(&playerAction, sizeof(STR_PackOtherPlayerAction));
        }
    }

    //
    //向当前玩家推送其他玩家的位置（可见的）
    //
    static void PushOtherUserPosition( TCPConnection::Pointer sk, STR_PackPlayerPosition *pos)
    {
        SessionMgr::SessionPointer ssmap = SessionMgr::Instance()->GetSession();
        SessionMgr::SessionMap::iterator it = ssmap->begin();
        while ( it != ssmap->end() )
        {
                TCPConnection::Pointer first = it->first;
                STR_PackPlayerPosition *spos = &( it->second.m_position);

                //不要推给自己
                if ( first == sk)
                    continue;
                hf_float dx = spos->Pos_x - pos->Pos_x,
                                dy = spos->Pos_y - pos->Pos_y,
                                dz = spos->Pos_z - pos->Pos_z,
                        dis = sqrt( dx*dx + dy * dy + dz * dz );

                //不要推视野范围外的玩家
                if ( dis > PlayerView )
                    continue;
                //Transfer::write_n(sk,(char*)pos,sizeof(STR_PackPlayerPosition));
                sk->Write_all(pos,sizeof(STR_PackPlayerPosition));
        }
    }

};

#endif // USERPOSITION

