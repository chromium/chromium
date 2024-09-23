// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_APP_ACTIVITY_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_APP_ACTIVITY_H_

#include <memory>
#include <optional>
#include <string>

#include "chrome/browser/media/router/providers/cast/cast_activity.h"
#include "chrome/browser/media/router/providers/cast/cast_media_controller.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/media_router/common/providers/cast/cast_media_source.h"
#include "components/media_router/common/providers/cast/channel/cast_message_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

namespace url {
class Origin;
}

namespace media_router {

class CastInternalMessage;
class CastSession;
class CastSessionTracker;
class MediaRoute;

// An activity corresponding to a Cast app, a.k.a. flinging.
class AppActivity : public CastActivity {
 public:
  AppActivity(const MediaRoute& route,
              const std::string& app_id,
              cast_channel::CastMessageHandler* message_handler,
              CastSessionTracker* session_tracker);
  ~AppActivity() override;

  void SendMediaStatusToClients(const base::Value::Dict& media_status,
                                std::optional<int> request_id) override;
  void OnAppMessage(
      const openscreen::cast::proto::CastMessage& message) override;
  void OnInternalMessage(const cast_channel::InternalMessage& message) override;
  void BindMediaController(
      mojo::PendingReceiver<mojom::MediaController> media_controller,
      mojo::PendingRemote<mojom::MediaStatusObserver> observer) override;
  std::optional<int> SendMediaRequestToReceiver(
      const CastInternalMessage& cast_message) override;
  cast_channel::Result SendAppMessageToReceiver(
      const CastInternalMessage& cast_message) override;
  void SendSetVolumeRequestToReceiver(
      const CastInternalMessage& cast_message,
      cast_channel::ResultCallback callback) override;

  bool CanJoinSession(const CastMediaSource& cast_source) const;
  bool HasJoinableClient(AutoJoinPolicy policy,
                         const url::Origin& origin,
                         content::FrameTreeNodeId frame_tree_node_id) const;
  void OnSessionSet(const CastSession& session) override;
  void OnSessionUpdated(const CastSession& session,
                        const std::string& hash_token) override;

 private:
  friend class CastSessionClientImpl;
  friend class CastActivityManager;
  friend class AppActivityTest;

  std::unique_ptr<CastMediaController> media_controller_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_APP_ACTIVITY_H_
