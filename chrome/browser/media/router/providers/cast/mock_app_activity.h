// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MOCK_APP_ACTIVITY_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MOCK_APP_ACTIVITY_H_

#include <string>

#include "chrome/browser/media/router/providers/cast/app_activity.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "chrome/browser/media/router/providers/cast/cast_session_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

class MockAppActivity : public AppActivity {
 public:
  MockAppActivity(const MediaRoute& route, const std::string& app_id);
  ~MockAppActivity() override;

  MOCK_METHOD1(SendAppMessageToReceiver,
               cast_channel::Result(const CastInternalMessage& cast_message));
  MOCK_METHOD1(SendMediaRequestToReceiver,
               std::optional<int>(const CastInternalMessage& cast_message));
  MOCK_METHOD2(SendSetVolumeRequestToReceiver,
               void(const CastInternalMessage& cast_message,
                    cast_channel::ResultCallback callback));
  MOCK_METHOD2(StopSessionOnReceiver,
               void(const std::string& client_id,
                    cast_channel::ResultCallback callback));
  MOCK_METHOD2(CloseConnectionOnReceiver,
               void(const std::string& client_id,
                    blink::mojom::PresentationConnectionCloseReason reason));
  MOCK_METHOD1(SendStopSessionMessageToClients,
               void(const std::string& hash_token));
  MOCK_METHOD1(HandleLeaveSession, void(const std::string& client_id));
  MOCK_METHOD3(
      AddClient,
      mojom::RoutePresentationConnectionPtr(const CastMediaSource& source,
                                            const url::Origin& origin,
                                            content::FrameTreeNodeId tab_id));
  MOCK_METHOD1(RemoveClient, void(const std::string& client_id));
  MOCK_METHOD1(OnSessionSet, void(const CastSession& session));
  MOCK_METHOD2(OnSessionUpdated,
               void(const CastSession& session, const std::string& hash_token));
  MOCK_METHOD2(SendMessageToClient,
               void(const std::string& client_id,
                    blink::mojom::PresentationConnectionMessagePtr message));
  MOCK_METHOD2(SendMediaStatusToClients,
               void(const base::Value::Dict& media_status,
                    std::optional<int> request_id));
  MOCK_METHOD1(
      ClosePresentationConnections,
      void(blink::mojom::PresentationConnectionCloseReason close_reason));
  MOCK_METHOD0(TerminatePresentationConnections, void());
  MOCK_METHOD1(OnAppMessage,
               void(const openscreen::cast::proto::CastMessage& message));
  MOCK_METHOD1(OnInternalMessage,
               void(const cast_channel::InternalMessage& message));
  MOCK_METHOD2(
      CreateMediaController,
      void(mojo::PendingReceiver<mojom::MediaController> media_controller,
           mojo::PendingRemote<mojom::MediaStatusObserver> observer));
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MOCK_APP_ACTIVITY_H_
