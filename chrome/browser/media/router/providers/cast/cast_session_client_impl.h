// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_SESSION_CLIENT_IMPL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_SESSION_CLIENT_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/router/providers/cast/cast_session_client.h"
#include "components/media_router/common/providers/cast/cast_media_source.h"
#include "components/media_router/common/providers/cast/channel/cast_message_handler.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace media_router {

class CastActivity;

class CastSessionClientImpl : public CastSessionClient,
                              public blink::mojom::PresentationConnection {
 public:
  CastSessionClientImpl(const std::string& client_id,
                        const url::Origin& origin,
                        content::FrameTreeNodeId frame_tree_node_id,
                        AutoJoinPolicy auto_join_policy,
                        CastActivity* activity);
  ~CastSessionClientImpl() override;

  // CastSessionClient implementation
  mojom::RoutePresentationConnectionPtr Init() override;
  // TODO(crbug.com/1291745): Remove redundant "ToClient" in the name of this
  // and other methods.
  void SendMessageToClient(
      blink::mojom::PresentationConnectionMessagePtr message) override;
  void SendMediaMessageToClient(const base::Value::Dict& payload,
                                std::optional<int> request_id) override;
  void CloseConnection(
      blink::mojom::PresentationConnectionCloseReason close_reason) override;
  void TerminateConnection() override;
  bool MatchesAutoJoinPolicy(
      url::Origin origin,
      content::FrameTreeNodeId frame_tree_node_id) const override;
  void SendErrorCodeToClient(int sequence_number,
                             CastInternalMessage::ErrorCode error_code,
                             std::optional<std::string> description) override;
  void SendErrorToClient(int sequence_number, base::Value::Dict error) override;

  // blink::mojom::PresentationConnection implementation
  void OnMessage(
      blink::mojom::PresentationConnectionMessagePtr message) override;
  // Blink does not initiate state change or close using PresentationConnection.
  // Instead, |PresentationService::Close/TerminateConnection| is used.
  void DidChangeState(
      blink::mojom::PresentationConnectionState state) override {}
  void DidClose(
      blink::mojom::PresentationConnectionCloseReason reason) override;

 private:
  void HandleParsedClientMessage(
      data_decoder::DataDecoder::ValueOrError result);
  void HandleV2ProtocolMessage(const CastInternalMessage& cast_message);

  // Resets the PresentationConnection Mojo message pipes.
  void TearDownPresentationConnection();

  // Sends a response to the client indicating that a particular request
  // succeeded or failed.
  void SendResultResponse(int sequence_number, cast_channel::Result result);

  // Builds a callback that calls SendResultResponse().
  cast_channel::ResultCallback MakeResultCallback(
      const CastInternalMessage& cast_message);

  const AutoJoinPolicy auto_join_policy_;

  const raw_ptr<CastActivity> activity_;

  // The maximum number of pending media requests, used to prevent memory leaks.
  // Normally the number of pending requests should be fairly small, but each
  // entry only consumes 2*sizeof(int) bytes, so the upper limit is set fairly
  // high.
  static constexpr std::size_t kMaxPendingMediaRequests = 1024;

  // Maps internal, locally-generated request IDs to sequence numbers from cast
  // messages received from the client.  Used to set an appropriate
  // sequenceNumber field in outgoing messages so a client can associate a media
  // status message with a previous request.
  //
  // TODO(crbug.com/1291745): Investigate whether this mapping is really
  // necessary, or if sequence numbers can be used directly without generating
  // request IDs.
  base::flat_map<int, int> pending_media_requests_;

  // Receiver for the PresentationConnection in Blink to receive incoming
  // messages and respond to state changes.
  mojo::Receiver<blink::mojom::PresentationConnection> connection_receiver_{
      this};

  // Mojo message pipe to PresentationConnection in Blink to send messages and
  // initiate state changes.
  mojo::Remote<blink::mojom::PresentationConnection> connection_remote_;

  base::WeakPtrFactory<CastSessionClientImpl> weak_ptr_factory_{this};
};
}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_SESSION_CLIENT_IMPL_H_
