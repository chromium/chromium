// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_TEST_MOCK_MEDIA_ROUTER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_TEST_MOCK_MEDIA_ROUTER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "build/build_config.h"
#include "chrome/browser/media/router/media_router_base.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/media_sink.h"
#include "chrome/common/media_router/media_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
}

namespace media_router {

// Media Router mock class. Used for testing purposes.
class MockMediaRouter : public MediaRouterBase {
 public:
  // This method can be passed into MediaRouterFactory::SetTestingFactory() to
  // make the factory return a MockMediaRouter.
  static std::unique_ptr<KeyedService> Create(content::BrowserContext* context);

  MockMediaRouter();
  ~MockMediaRouter() override;

  // TODO(crbug.com/729950): Use MOCK_METHOD directly once GMock gets the
  // move-only type support.
  void CreateRoute(const MediaSource::Id& source,
                   const MediaSink::Id& sink_id,
                   const url::Origin& origin,
                   content::WebContents* web_contents,
                   MediaRouteResponseCallback callback,
                   base::TimeDelta timeout,
                   bool incognito) override {
    CreateRouteInternal(source, sink_id, origin, web_contents, callback,
                        timeout, incognito);
  }
  MOCK_METHOD7(CreateRouteInternal,
               void(const MediaSource::Id& source,
                    const MediaSink::Id& sink_id,
                    const url::Origin& origin,
                    content::WebContents* web_contents,
                    MediaRouteResponseCallback& callback,
                    base::TimeDelta timeout,
                    bool incognito));

  void JoinRoute(const MediaSource::Id& source,
                 const std::string& presentation_id,
                 const url::Origin& origin,
                 content::WebContents* web_contents,
                 MediaRouteResponseCallback callback,
                 base::TimeDelta timeout,
                 bool incognito) override {
    JoinRouteInternal(source, presentation_id, origin, web_contents, callback,
                      timeout, incognito);
  }
  MOCK_METHOD7(JoinRouteInternal,
               void(const MediaSource::Id& source,
                    const std::string& presentation_id,
                    const url::Origin& origin,
                    content::WebContents* web_contents,
                    MediaRouteResponseCallback& callback,
                    base::TimeDelta timeout,
                    bool incognito));

  void ConnectRouteByRouteId(const MediaSource::Id& source,
                             const MediaRoute::Id& route_id,
                             const url::Origin& origin,
                             content::WebContents* web_contents,
                             MediaRouteResponseCallback callback,
                             base::TimeDelta timeout,
                             bool incognito) override {
    ConnectRouteByRouteIdInternal(source, route_id, origin, web_contents,
                                  callback, timeout, incognito);
  }
  MOCK_METHOD7(ConnectRouteByRouteIdInternal,
               void(const MediaSource::Id& source,
                    const MediaRoute::Id& route_id,
                    const url::Origin& origin,
                    content::WebContents* web_contents,
                    MediaRouteResponseCallback& callback,
                    base::TimeDelta timeout,
                    bool incognito));

  MOCK_METHOD1(DetachRoute, void(const MediaRoute::Id& route_id));
  MOCK_METHOD1(TerminateRoute, void(const MediaRoute::Id& route_id));
  MOCK_METHOD2(SendRouteMessage,
               void(const MediaRoute::Id& route_id,
                    const std::string& message));
  MOCK_METHOD2(SendRouteBinaryMessage,
               void(const MediaRoute::Id& route_id,
                    std::unique_ptr<std::vector<uint8_t>> data));
  MOCK_METHOD0(OnUserGesture, void());

  void SearchSinks(const MediaSink::Id& sink_id,
                   const MediaSource::Id& source_id,
                   const std::string& search_input,
                   const std::string& domain,
                   MediaSinkSearchResponseCallback sink_callback) override {
    SearchSinksInternal(sink_id, source_id, search_input, domain,
                        sink_callback);
  }
  MOCK_METHOD5(SearchSinksInternal,
               void(const MediaSink::Id& sink_id,
                    const MediaSource::Id& source_id,
                    const std::string& search_input,
                    const std::string& domain,
                    MediaSinkSearchResponseCallback& sink_callback));

  MOCK_METHOD1(OnPresentationSessionDetached,
               void(const MediaRoute::Id& route_id));
  std::unique_ptr<PresentationConnectionStateSubscription>
  AddPresentationConnectionStateChangedCallback(
      const MediaRoute::Id& route_id,
      const content::PresentationConnectionStateChangedCallback& callback)
      override {
    OnAddPresentationConnectionStateChangedCallbackInvoked(callback);
    return connection_state_callbacks_.Add(callback);
  }
  MOCK_CONST_METHOD0(GetCurrentRoutes, std::vector<MediaRoute>());

  MOCK_METHOD0(OnIncognitoProfileShutdown, void());
#if !defined(OS_ANDROID)
  MOCK_METHOD3(GetMediaController,
               void(const MediaRoute::Id& route_id,
                    mojo::PendingReceiver<mojom::MediaController> controller,
                    mojo::PendingRemote<mojom::MediaStatusObserver> observer));
#endif  // !defined(OS_ANDROID)
  MOCK_METHOD1(OnAddPresentationConnectionStateChangedCallbackInvoked,
               void(const content::PresentationConnectionStateChangedCallback&
                        callback));
  MOCK_METHOD1(RegisterMediaSinksObserver, bool(MediaSinksObserver* observer));
  MOCK_METHOD1(UnregisterMediaSinksObserver,
               void(MediaSinksObserver* observer));
  MOCK_METHOD1(RegisterMediaRoutesObserver,
               void(MediaRoutesObserver* observer));
  MOCK_METHOD1(UnregisterMediaRoutesObserver,
               void(MediaRoutesObserver* observer));
  MOCK_METHOD1(RegisterRouteMessageObserver,
               void(RouteMessageObserver* observer));
  MOCK_METHOD1(UnregisterRouteMessageObserver,
               void(RouteMessageObserver* observer));
  MOCK_METHOD0(GetMediaSinkServiceStatus, std::string());

 private:
  base::CallbackList<void(
      const content::PresentationConnectionStateChangeInfo&)>
      connection_state_callbacks_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_TEST_MOCK_MEDIA_ROUTER_H_
