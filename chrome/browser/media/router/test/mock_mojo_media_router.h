// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_TEST_MOCK_MOJO_MEDIA_ROUTER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_TEST_MOCK_MOJO_MEDIA_ROUTER_H_

#include "chrome/browser/media/router/test/mock_media_router.h"
#include "chrome/common/media_router/media_route_provider_helper.h"
#include "chrome/common/media_router/mojom/media_router.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

class MockMojoMediaRouter : public MockMediaRouter, public mojom::MediaRouter {
 public:
  MockMojoMediaRouter();
  ~MockMojoMediaRouter() override;

  // mojom::MediaRouter overrides:
  void RegisterMediaRouteProvider(
      MediaRouteProviderId provider_id,
      mojo::PendingRemote<mojom::MediaRouteProvider> provider_remote,
      RegisterMediaRouteProviderCallback callback) override {
    RegisterMediaRouteProviderInternal(provider_id, provider_remote, callback);
  }
  MOCK_METHOD3(
      RegisterMediaRouteProviderInternal,
      void(MediaRouteProviderId provider_id,
           mojo::PendingRemote<mojom::MediaRouteProvider>& provider_remote,
           RegisterMediaRouteProviderCallback& callback));
  MOCK_METHOD1(OnIssue, void(const IssueInfo& issue));
  MOCK_METHOD4(OnSinksReceived,
               void(MediaRouteProviderId provider_id,
                    const std::string& media_source,
                    const std::vector<MediaSinkInternal>& internal_sinks,
                    const std::vector<url::Origin>& origins));
  MOCK_METHOD4(OnRoutesUpdated,
               void(MediaRouteProviderId provider_id,
                    const std::vector<MediaRoute>& routes,
                    const std::string& media_source,
                    const std::vector<std::string>& joinable_route_ids));
  MOCK_METHOD2(OnSinkAvailabilityUpdated,
               void(MediaRouteProviderId provider_id,
                    mojom::MediaRouter::SinkAvailability availability));
  MOCK_METHOD2(
      OnPresentationConnectionStateChanged,
      void(
          const std::string& route_id,
          media_router::mojom::MediaRouter::PresentationConnectionState state));
  MOCK_METHOD3(
      OnPresentationConnectionClosed,
      void(const std::string& route_id,
           media_router::mojom::MediaRouter::PresentationConnectionCloseReason
               reason,
           const std::string& message));
  MOCK_METHOD2(OnRouteMessagesReceived,
               void(const std::string& route_id,
                    std::vector<mojom::RouteMessagePtr> messages));
  void OnMediaRemoterCreated(
      int32_t tab_id,
      mojo::PendingRemote<media::mojom::MirrorServiceRemoter> remoter,
      mojo::PendingReceiver<media::mojom::MirrorServiceRemotingSource>
          source_receiver) override {
    OnMediaRemoterCreatedInternal(tab_id, std::move(remoter),
                                  std::move(source_receiver));
  }
  MOCK_METHOD3(
      OnMediaRemoterCreatedInternal,
      void(int32_t tab_id,
           mojo::PendingRemote<media::mojom::MirrorServiceRemoter> remoter,
           mojo::PendingReceiver<media::mojom::MirrorServiceRemotingSource>
               source_receiver));
  void GetMediaSinkServiceStatus(
      mojom::MediaRouter::GetMediaSinkServiceStatusCallback callback) override {
    GetMediaSinkServiceStatusInternal(callback);
  }
  MOCK_METHOD1(
      GetMediaSinkServiceStatusInternal,
      void(mojom::MediaRouter::GetMediaSinkServiceStatusCallback& callback));
  MOCK_METHOD0(GetMediaSinkServiceStatus, std::string());
  MOCK_METHOD2(
      GetMirroringServiceHostForTab,
      void(int32_t target_tab_id,
           mojo::PendingReceiver<mirroring::mojom::MirroringServiceHost>
               receiver));
  MOCK_METHOD3(
      GetMirroringServiceHostForDesktop,
      void(int32_t initiator_tab_id,
           const std::string& desktop_stream_id,
           mojo::PendingReceiver<mirroring::mojom::MirroringServiceHost>
               receiver));
  MOCK_METHOD3(
      GetMirroringServiceHostForOffscreenTab,
      void(const GURL& presentation_url,
           const std::string& presentation_id,
           mojo::PendingReceiver<mirroring::mojom::MirroringServiceHost>
               receiver));
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_TEST_MOCK_MOJO_MEDIA_ROUTER_H_
