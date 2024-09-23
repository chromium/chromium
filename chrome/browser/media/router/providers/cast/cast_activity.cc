// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_activity.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "chrome/browser/media/router/providers/cast/cast_session_client_impl.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"

namespace media_router {

CastActivity::CastActivity(const MediaRoute& route,
                           const std::string& app_id,
                           cast_channel::CastMessageHandler* message_handler,
                           CastSessionTracker* session_tracker)
    : route_(route),
      app_id_(app_id),
      message_handler_(message_handler),
      session_tracker_(session_tracker) {}

CastActivity::~CastActivity() = default;

void CastActivity::SetRouteIsConnecting(bool is_connecting) {
  route_.set_is_connecting(is_connecting);
}

mojom::RoutePresentationConnectionPtr CastActivity::AddClient(
    const CastMediaSource& source,
    const url::Origin& origin,
    content::FrameTreeNodeId frame_tree_node_id) {
  const std::string& client_id = source.client_id();
  DCHECK(!base::Contains(connected_clients_, client_id));
  std::unique_ptr<CastSessionClient> client =
      client_factory_for_test_
          ? client_factory_for_test_->MakeClientForTest(  // IN-TEST
                client_id, origin, frame_tree_node_id)
          : std::make_unique<CastSessionClientImpl>(
                client_id, origin, frame_tree_node_id,
                source.auto_join_policy(), this);
  auto presentation_connection = client->Init();
  connected_clients_.emplace(client_id, std::move(client));

  // Route is now local due to connected client.
  route_.set_local(true);
  return presentation_connection;
}

void CastActivity::RemoveClient(const std::string& client_id) {
  // Don't erase by key here as the |client_id| may be referring to the
  // client being deleted.
  auto it = connected_clients_.find(client_id);
  if (it != connected_clients_.end())
    connected_clients_.erase(it);
}

CastSession* CastActivity::GetSession() const {
  if (!session_id_)
    return nullptr;
  CastSession* session = session_tracker_->GetSessionById(*session_id_);
  if (!session) {
    // TODO(crbug.com/41426190): Add UMA metrics for this and other error
    // conditions.
    LOG(ERROR) << "Session not found: " << *session_id_;
  }
  return session;
}

void CastActivity::SetOrUpdateSession(const CastSession& session,
                                      const MediaSinkInternal& sink,
                                      const std::string& hash_token) {
  DVLOG(2) << "SetOrUpdateSession old session_id = "
           << session_id_.value_or("<missing>")
           << ", new session_id = " << session.session_id();
  DCHECK(sink.is_cast_sink());
  route_.set_description(GetRouteDescription(session));
  sink_ = sink;
  if (session_id_) {
    DCHECK_EQ(*session_id_, session.session_id());
    OnSessionUpdated(session, hash_token);
  } else {
    session_id_ = session.session_id();
    OnSessionSet(session);
  }
}

void CastActivity::SendStopSessionMessageToClients(
    const std::string& hash_token) {
  for (const auto& client : connected_clients_) {
    client.second->SendMessageToClient(
        CreateReceiverActionStopMessage(client.first, sink_, hash_token));
  }
}

void CastActivity::SendMessageToClient(
    const std::string& client_id,
    blink::mojom::PresentationConnectionMessagePtr message) {
  auto it = connected_clients_.find(client_id);
  if (it == connected_clients_.end()) {
    DLOG(ERROR) << "Attempting to send message to nonexistent client: "
                << client_id;
    return;
  }
  it->second->SendMessageToClient(std::move(message));
}

void CastActivity::SendMediaStatusToClients(
    const base::Value::Dict& media_status,
    std::optional<int> request_id) {
  for (auto& client : connected_clients_)
    client.second->SendMediaMessageToClient(media_status, request_id);
}

void CastActivity::ClosePresentationConnections(
    blink::mojom::PresentationConnectionCloseReason close_reason) {
  for (auto& client : connected_clients_)
    client.second->CloseConnection(close_reason);
}

void CastActivity::TerminatePresentationConnections() {
  for (auto& client : connected_clients_)
    client.second->TerminateConnection();
}

std::optional<int> CastActivity::SendMediaRequestToReceiver(
    const CastInternalMessage& cast_message) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

cast_channel::Result CastActivity::SendAppMessageToReceiver(
    const CastInternalMessage& cast_message) {
  NOTIMPLEMENTED();
  return cast_channel::Result::kFailed;
}

void CastActivity::SendSetVolumeRequestToReceiver(
    const CastInternalMessage& cast_message,
    cast_channel::ResultCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(cast_channel::Result::kFailed);
}

void CastActivity::StopSessionOnReceiver(
    const std::string& client_id,
    cast_channel::ResultCallback callback) {
  if (!session_id_) {
    std::move(callback).Run(cast_channel::Result::kFailed);
    return;
  }

  message_handler_->StopSession(cast_channel_id(), *session_id_, client_id,
                                std::move(callback));
}

void CastActivity::CloseConnectionOnReceiver(
    const std::string& client_id,
    blink::mojom::PresentationConnectionCloseReason reason) {
  CastSession* session = GetSession();
  if (!session) {
    return;
  }
  if (reason == blink::mojom::PresentationConnectionCloseReason::CLOSED ||
      !base::FeatureList::IsEnabled(kCastSilentlyRemoveVcOnNavigation)) {
    message_handler_->CloseConnection(cast_channel_id(), client_id,
                                      session->destination_id());

  } else {
    message_handler_->RemoveConnection(cast_channel_id(), client_id,
                                       session->destination_id());
  }
}

void CastActivity::HandleLeaveSession(const std::string& client_id) {
  auto client_it = connected_clients_.find(client_id);
  CHECK(client_it != connected_clients_.end());
  auto& client = *client_it->second;
  std::vector<std::string> leaving_client_ids;
  for (const auto& pair : connected_clients_) {
    if (pair.second->MatchesAutoJoinPolicy(client.origin(),
                                           client.frame_tree_node_id()))
      leaving_client_ids.push_back(pair.first);
  }

  for (const auto& leaving_client_id : leaving_client_ids) {
    auto leaving_client_it = connected_clients_.find(leaving_client_id);
    CHECK(leaving_client_it != connected_clients_.end());
    leaving_client_it->second->CloseConnection(
        blink::mojom::PresentationConnectionCloseReason::CLOSED);
    connected_clients_.erase(leaving_client_it);
  }
}

void CastActivity::OnSessionUpdated(const CastSession& session,
                                    const std::string& hash_token) {}

std::string CastActivity::GetRouteDescription(
    const CastSession& session) const {
  return session.GetRouteDescription();
}

CastSessionClientFactoryForTest* CastActivity::client_factory_for_test_ =
    nullptr;

}  // namespace media_router
