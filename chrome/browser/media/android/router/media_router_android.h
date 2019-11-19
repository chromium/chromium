// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ANDROID_ROUTER_MEDIA_ROUTER_ANDROID_H_
#define CHROME_BROWSER_MEDIA_ANDROID_ROUTER_MEDIA_ROUTER_ANDROID_H_

#include <stdint.h>

#include <memory>
#include <unordered_map>

#include "base/containers/id_map.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/media/android/router/media_router_android_bridge.h"
#include "chrome/browser/media/router/media_router_base.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class BrowserContext;
}

namespace media_router {

// An implementation of MediaRouter interface on Android.
class MediaRouterAndroid : public MediaRouterBase {
 public:
  ~MediaRouterAndroid() override;

  const MediaRoute* FindRouteBySource(const MediaSource::Id& source_id) const;

  // MediaRouter implementation.
  void CreateRoute(const MediaSource::Id& source_id,
                   const MediaSink::Id& sink_id,
                   const url::Origin& origin,
                   content::WebContents* web_contents,
                   MediaRouteResponseCallback callback,
                   base::TimeDelta timeout,
                   bool incognito) override;
  void JoinRoute(const MediaSource::Id& source,
                 const std::string& presentation_id,
                 const url::Origin& origin,
                 content::WebContents* web_contents,
                 MediaRouteResponseCallback callback,
                 base::TimeDelta timeout,
                 bool incognito) override;
  void ConnectRouteByRouteId(const MediaSource::Id& source,
                             const MediaRoute::Id& route_id,
                             const url::Origin& origin,
                             content::WebContents* web_contents,
                             MediaRouteResponseCallback callback,
                             base::TimeDelta timeout,
                             bool incognito) override;
  void DetachRoute(const MediaRoute::Id& route_id) override;
  void TerminateRoute(const MediaRoute::Id& route_id) override;
  void SendRouteMessage(const MediaRoute::Id& route_id,
                        const std::string& message) override;
  void SendRouteBinaryMessage(
      const MediaRoute::Id& route_id,
      std::unique_ptr<std::vector<uint8_t>> data) override;
  void OnUserGesture() override;
  void SearchSinks(const MediaSink::Id& sink_id,
                   const MediaSource::Id& source_id,
                   const std::string& search_input,
                   const std::string& domain,
                   MediaSinkSearchResponseCallback sink_callback) override;
  std::unique_ptr<media::FlingingController> GetFlingingController(
      const MediaRoute::Id& route_id) override;

  // The methods called by the Java bridge.
  // Notifies the media router that information about sinks is received for
  // a specific source id.
  void OnSinksReceived(const MediaSource::Id& source_id,
                       const std::vector<MediaSink>& sinks);

  // Notifies the media router about a successful route creation.
  void OnRouteCreated(const MediaRoute::Id& route_id,
                      const MediaSink::Id& sink_id,
                      int request_id,
                      bool is_local);

  // Notifies the media router that route creation or joining failed.
  void OnRouteRequestError(const std::string& error_text, int request_id);

  // Notifies the media router when the route was terminated.
  void OnRouteTerminated(const MediaRoute::Id& route_id);

  // Notifies the media router when the route was closed with an optional error.
  // Null error indicates no error.
  void OnRouteClosed(const MediaRoute::Id& route_id,
                     const base::Optional<std::string>& error);

  // Notifies the media router about a message received from the media route.
  void OnMessage(const MediaRoute::Id& route_id, const std::string& message);

 private:
  friend class MediaRouterFactory;
  friend class MediaRouterAndroidTest;

  // This class bridges messages between MediaRouterAndroid's messages API and a
  // PresentationConnection.
  class PresentationConnectionProxy final
      : public blink::mojom::PresentationConnection {
   public:
    using OnMessageCallback = base::OnceCallback<void(bool)>;

    PresentationConnectionProxy(MediaRouterAndroid* media_router_android,
                                const MediaRoute::Id& route_id);
    ~PresentationConnectionProxy() override;

    // Initializes the connection binding and interface request and returns that
    // as a mojom::RoutePresentationConnectionPtr.
    mojom::RoutePresentationConnectionPtr Init();

    // blink::mojom::PresentationConnection overrides.
    void OnMessage(
        blink::mojom::PresentationConnectionMessagePtr message) override;
    void DidChangeState(
        blink::mojom::PresentationConnectionState state) override {}
    // Destroys |this| by removing it from MediaRouterAndroid's collection.
    void DidClose(
        blink::mojom::PresentationConnectionCloseReason reason) override;

    // Sends a text message back to router's peer for this connection (|peer_|).
    void SendMessage(const std::string& message);

    // Sends a TERMINATED state change message directly via |peer_|.
    void Terminate();

   private:
    mojo::PendingRemote<blink::mojom::PresentationConnection> Bind();

    mojo::Remote<blink::mojom::PresentationConnection> peer_;
    mojo::Receiver<blink::mojom::PresentationConnection> receiver_{this};
    // |media_router_android_| owns |this|, so it will outlive |this|.
    MediaRouterAndroid* media_router_android_;
    MediaRoute::Id route_id_;

    DISALLOW_COPY_AND_ASSIGN(PresentationConnectionProxy);
  };

  explicit MediaRouterAndroid(content::BrowserContext*);

  // Removes the route with the given id from |active_routes_| and updates the
  // registered route observers.
  void RemoveRoute(const MediaRoute::Id& route_id);

  // MediaRouter implementation.
  bool RegisterMediaSinksObserver(MediaSinksObserver* observer) override;
  void UnregisterMediaSinksObserver(MediaSinksObserver* observer) override;
  void RegisterMediaRoutesObserver(MediaRoutesObserver* observer) override;
  void UnregisterMediaRoutesObserver(MediaRoutesObserver* observer) override;
  void RegisterRouteMessageObserver(RouteMessageObserver* observer) override;
  void UnregisterRouteMessageObserver(RouteMessageObserver* observer) override;

  void OnPresentationConnectionError(const std::string& route_id);

  void SetMediaRouterBridgeForTest(MediaRouterAndroidBridge* bridge) {
    bridge_.reset(bridge);
  }

  std::unique_ptr<MediaRouterAndroidBridge> bridge_;

  using MediaSinksObserverList =
      base::ObserverList<MediaSinksObserver>::Unchecked;
  using MediaSinkObservers =
      std::unordered_map<MediaSource::Id,
                         std::unique_ptr<MediaSinksObserverList>>;
  MediaSinkObservers sinks_observers_;

  base::ObserverList<MediaRoutesObserver>::Unchecked routes_observers_;

  struct MediaRouteRequest {
    MediaRouteRequest(const MediaSource& source,
                      const std::string& presentation_id,
                      MediaRouteResponseCallback callback);
    ~MediaRouteRequest();

    MediaSource media_source;
    std::string presentation_id;
    MediaRouteResponseCallback callback;
  };

  using MediaRouteRequests = base::IDMap<std::unique_ptr<MediaRouteRequest>>;
  MediaRouteRequests route_requests_;

  using MediaRoutes = std::vector<MediaRoute>;
  MediaRoutes active_routes_;

  std::unordered_map<MediaRoute::Id,
                     std::vector<std::unique_ptr<PresentationConnectionProxy>>>
      presentation_connections_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterAndroid);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ANDROID_ROUTER_MEDIA_ROUTER_ANDROID_H_
