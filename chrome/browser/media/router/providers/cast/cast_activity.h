// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_ACTIVITY_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_ACTIVITY_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "chrome/browser/media/router/providers/cast/cast_session_client.h"
#include "chrome/browser/media/router/providers/cast/cast_session_tracker.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/media_router/common/providers/cast/cast_media_source.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"

namespace cast_channel {
class CastMessageHandler;
}

namespace media_router {

class AppActivity;
class MirroringActivity;
class CastSessionTracker;

using OnSourceChangedCallback = base::RepeatingCallback<void(
    content::FrameTreeNodeId old_frame_tree_node_id,
    content::FrameTreeNodeId frame_tree_node_id)>;

class CastActivityFactoryForTest {
 public:
  virtual std::unique_ptr<AppActivity> MakeAppActivity(
      const MediaRoute& route,
      const std::string& app_id) = 0;
  virtual std::unique_ptr<MirroringActivity> MakeMirroringActivity(
      const MediaRoute& route,
      const std::string& app_id,
      base::OnceClosure on_stop,
      OnSourceChangedCallback on_source_changed) = 0;
};

class CastActivity {
 public:
  CastActivity(const MediaRoute& route,
               const std::string& app_id,
               cast_channel::CastMessageHandler* message_handler,
               CastSessionTracker* session_tracker);
  CastActivity(const CastActivity&) = delete;
  CastActivity& operator=(const CastActivity&) = delete;
  virtual ~CastActivity();

  const MediaRoute& route() const { return route_; }
  const std::string& app_id() const { return app_id_; }
  const std::optional<std::string>& session_id() const { return session_id_; }
  const MediaSinkInternal sink() const { return sink_; }

  void SetRouteIsConnecting(bool is_connecting);

  // Adds a new client |client_id| to this session and returns the handles of
  // the two pipes to be held by Blink It is invalid to call this method if the
  // client already exists.
  virtual mojom::RoutePresentationConnectionPtr AddClient(
      const CastMediaSource& source,
      const url::Origin& origin,
      content::FrameTreeNodeId frame_tree_node_id);

  virtual void RemoveClient(const std::string& client_id);

  // On the first call, saves the ID of |session|.  On subsequent calls,
  // notifies all connected clients that the session has been updated.  In both
  // cases, the stored route description is updated to match the session
  // description.
  //
  // The |hash_token| parameter is used for hashing receiver IDs in messages
  // sent to the Cast SDK, and |sink| is the sink associated with |session|.
  void SetOrUpdateSession(const CastSession& session,
                          const MediaSinkInternal& sink,
                          const std::string& hash_token);

  virtual void SendStopSessionMessageToClients(const std::string& hash_token);

  // Sends |message| to the client given by |client_id|.
  virtual void SendMessageToClient(
      const std::string& client_id,
      blink::mojom::PresentationConnectionMessagePtr message);

  virtual void SendMediaStatusToClients(const base::Value::Dict& media_status,
                                        std::optional<int> request_id);

  // Handles a message forwarded by CastActivityManager.
  virtual void OnAppMessage(
      const openscreen::cast::proto::CastMessage& message) = 0;
  virtual void OnInternalMessage(
      const cast_channel::InternalMessage& message) = 0;

  // Closes/terminates the PresentationConnections of all clients connected to
  // this activity.
  virtual void ClosePresentationConnections(
      blink::mojom::PresentationConnectionCloseReason close_reason);
  virtual void TerminatePresentationConnections();

  // Binds the given |media_controller| and |observer| to the activity to
  // receive media commands and notify observers.
  virtual void BindMediaController(
      mojo::PendingReceiver<mojom::MediaController> media_controller,
      mojo::PendingRemote<mojom::MediaStatusObserver> observer) = 0;

  // Sends media command |cast_message|, which came from the SDK client, to the
  // receiver hosting this session. Returns the locally-assigned request ID of
  // the message sent to the receiver.
  virtual std::optional<int> SendMediaRequestToReceiver(
      const CastInternalMessage& cast_message);

  // Sends app message |cast_message|, which came from the SDK client, to the
  // receiver hosting this session. Returns true if the message is sent
  // successfully.
  virtual cast_channel::Result SendAppMessageToReceiver(
      const CastInternalMessage& cast_message);

  // Sends a SET_VOLUME request to the receiver and calls |callback| when a
  // response indicating whether the request succeeded is received.
  virtual void SendSetVolumeRequestToReceiver(
      const CastInternalMessage& cast_message,
      cast_channel::ResultCallback callback);

  // Stops the currently active session on the receiver, and invokes |callback|
  // with the result. Called when a SDK client requests to stop the session.
  virtual void StopSessionOnReceiver(const std::string& client_id,
                                     cast_channel::ResultCallback callback);

  // Closes any virtual connection between |client_id| and this session on the
  // receiver.
  virtual void CloseConnectionOnReceiver(
      const std::string& client_id,
      blink::mojom::PresentationConnectionCloseReason reason);

  // Called when the client given by |client_id| requests to leave the session.
  // This will also cause all clients within the session with matching origin
  // and/or tab ID to leave (i.e., their presentation connections will be
  // closed).
  virtual void HandleLeaveSession(const std::string& client_id);

  static void SetClientFactoryForTest(
      CastSessionClientFactoryForTest* factory) {
    client_factory_for_test_ = factory;
  }

  void SetSessionIdForTest(const std::string& session_id) {
    session_id_ = session_id;
  }

 protected:
  using ClientMap =
      base::flat_map<std::string, std::unique_ptr<CastSessionClient>>;

  // Gets the session based on its ID.  May return null.
  CastSession* GetSession() const;

  // Called after the session has been set by SetOrUpdateSession.  The |session|
  // parameters are somewhat redundant because the same information is available
  // using the GetSession() method, but passing the parameter avoids some
  // unnecessary lookups and eliminates the need to a null check.
  virtual void OnSessionSet(const CastSession& session) = 0;
  virtual void OnSessionUpdated(const CastSession& session,
                                const std::string& hash_token);

  CastSessionClient* GetClient(const std::string& client_id) {
    auto it = connected_clients_.find(client_id);
    return it == connected_clients_.end() ? nullptr : it->second.get();
  }

  virtual std::string GetRouteDescription(const CastSession& session) const;

  int cast_channel_id() const { return sink_.cast_channel_id(); }

  MediaRoute route_;
  std::string app_id_;

  // TODO(crbug.com/40561499): Consider wrapping CastMessageHandler with
  // known parameters (sink, client ID, session transport ID) and passing them
  // to objects that need to send messages to the receiver.
  const raw_ptr<cast_channel::CastMessageHandler> message_handler_;

  const raw_ptr<CastSessionTracker> session_tracker_;

  // Set by CastActivityManager after the session is launched successfully.
  std::optional<std::string> session_id_;

  MediaSinkInternal sink_;
  ClientMap connected_clients_;

 private:
  static CastSessionClientFactoryForTest* client_factory_for_test_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_ACTIVITY_H_
