// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_ACTIVITY_RECORD_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_ACTIVITY_RECORD_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "chrome/browser/media/router/providers/cast/cast_session_client.h"
#include "chrome/browser/media/router/providers/cast/cast_session_tracker.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/mojom/media_router.mojom.h"
#include "chrome/common/media_router/providers/cast/cast_media_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace cast_channel {
class CastMessageHandler;
}

namespace url {
class Origin;
}

namespace media_router {

class CastSessionTracker;

class ActivityRecord {
 public:
  using ClientMap =
      base::flat_map<std::string, std::unique_ptr<CastSessionClient>>;

  ActivityRecord(const MediaRoute& route,
                 const std::string& app_id,
                 cast_channel::CastMessageHandler* message_handler,
                 CastSessionTracker* session_tracker);
  ActivityRecord(const ActivityRecord&) = delete;
  ActivityRecord& operator=(const ActivityRecord&) = delete;
  virtual ~ActivityRecord();

  const MediaRoute& route() const { return route_; }
  const std::string& app_id() const { return app_id_; }
  const base::Optional<std::string>& session_id() const { return session_id_; }
  base::Optional<int> mirroring_tab_id() const { return mirroring_tab_id_; }
  const MediaSinkInternal sink() const { return sink_; }

  // On the first call, saves the ID of |session|.  On subsequent calls,
  // notifies all connected clients that the session has been updated.  In both
  // cases, the stored route description is updated to match the session
  // description.
  //
  // The |hash_token| parameter is used for hashing receiver IDs in messages
  // sent to the Cast SDK, and |sink| is the sink associated with |session|.
  virtual void SetOrUpdateSession(const CastSession& session,
                                  const MediaSinkInternal& sink,
                                  const std::string& hash_token);

  // TODO(jrw): Get rid of this accessor.
  const ClientMap& connected_clients() const { return connected_clients_; }

  // Sends app message |cast_message|, which came from the SDK client, to the
  // receiver hosting this session. Returns true if the message is sent
  // successfully.
  //
  // TODO(jrw): Move this method to CastActivityRecord.
  virtual cast_channel::Result SendAppMessageToReceiver(
      const CastInternalMessage& cast_message) = 0;

  // Sends media command |cast_message|, which came from the SDK client, to the
  // receiver hosting this session. Returns the locally-assigned request ID of
  // the message sent to the receiver.
  //
  // TODO(jrw): Move this method to CastActivityRecord.
  virtual base::Optional<int> SendMediaRequestToReceiver(
      const CastInternalMessage& cast_message) = 0;

  // Sends a SET_VOLUME request to the receiver and calls |callback| when a
  // response indicating whether the request succeeded is received.
  //
  // TODO(jrw): Move this method to CastActivityRecord.
  virtual void SendSetVolumeRequestToReceiver(
      const CastInternalMessage& cast_message,
      cast_channel::ResultCallback callback) = 0;

  virtual void SendStopSessionMessageToReceiver(
      const base::Optional<std::string>& client_id,
      const std::string& hash_token,
      mojom::MediaRouteProvider::TerminateRouteCallback callback) = 0;

  // Called when the client given by |client_id| requests to leave the session.
  // This will also cause all clients within the session with matching origin
  // and/or tab ID to leave (i.e., their presentation connections will be
  // closed).
  //
  // TODO(jrw): Move this method to CastActivityRecord.
  virtual void HandleLeaveSession(const std::string& client_id) = 0;

  // Adds a new client |client_id| to this session and returns the handles of
  // the two pipes to be held by Blink It is invalid to call this method if the
  // client already exists.
  //
  // TODO(jrw): This method is only called on CastActivityRecord instances.
  // Should it be moved?
  virtual mojom::RoutePresentationConnectionPtr AddClient(
      const CastMediaSource& source,
      const url::Origin& origin,
      int tab_id) = 0;

  // TODO(jrw): This method is never called outside of unit tests.  Figure out
  // where it should be called.  If AddClient() is moved to CastActivityRecord,
  // this method probably should be, too.
  virtual void RemoveClient(const std::string& client_id) = 0;

  // Sends |message| to the client given by |client_id|.
  //
  // TODO(jrw): This method's functionality overlaps that of OnAppMessage().
  // Can the methods be combined?
  virtual void SendMessageToClient(
      const std::string& client_id,
      blink::mojom::PresentationConnectionMessagePtr message) = 0;

  virtual void SendMediaStatusToClients(const base::Value& media_status,
                                        base::Optional<int> request_id) = 0;

  // Handles a message forwarded by CastActivityManager.
  virtual void OnAppMessage(const cast_channel::CastMessage& message) = 0;
  virtual void OnInternalMessage(
      const cast_channel::InternalMessage& message) = 0;

  // Closes / Terminates the PresentationConnections of all clients connected
  // to this activity.
  virtual void ClosePresentationConnections(
      blink::mojom::PresentationConnectionCloseReason close_reason) = 0;
  virtual void TerminatePresentationConnections() = 0;

  virtual void CreateMediaController(
      mojo::PendingReceiver<mojom::MediaController> media_controller,
      mojo::PendingRemote<mojom::MediaStatusObserver> observer) = 0;

 protected:
  CastSession* GetSession() const;

  MediaRoute route_;
  std::string app_id_;
  base::Optional<int> mirroring_tab_id_;
  ClientMap connected_clients_;

  // Called when a session is initially set from SetOrUpdateSession().
  base::OnceCallback<void()> on_session_set_;

  // TODO(https://crbug.com/809249): Consider wrapping CastMessageHandler with
  // known parameters (sink, client ID, session transport ID) and passing them
  // to objects that need to send messages to the receiver.
  cast_channel::CastMessageHandler* const message_handler_;

  CastSessionTracker* const session_tracker_;

  // Set by CastActivityManager after the session is launched successfully.
  base::Optional<std::string> session_id_;

  MediaSinkInternal sink_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_ACTIVITY_RECORD_H_
