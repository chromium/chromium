// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_TEST_TEST_MEDIA_ROUTE_PROVIDER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_TEST_TEST_MEDIA_ROUTE_PROVIDER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media_router {

// The Test MediaRouteProvider class is used for integration browser test.
class TestMediaRouteProvider : public mojom::MediaRouteProvider {
 public:
  static const MediaRouteProviderId kProviderId;
  TestMediaRouteProvider(
      mojo::PendingReceiver<mojom::MediaRouteProvider> receiver,
      mojo::PendingRemote<mojom::MediaRouter> media_router);
  ~TestMediaRouteProvider() override;

  void CreateRoute(const std::string& media_source,
                   const std::string& sink_id,
                   const std::string& presentation_id,
                   const url::Origin& origin,
                   int32_t tab_id,
                   base::TimeDelta timeout,
                   bool incognito,
                   CreateRouteCallback callback) override;
  void JoinRoute(const std::string& media_source,
                 const std::string& presentation_id,
                 const url::Origin& origin,
                 int32_t tab_id,
                 base::TimeDelta timeout,
                 bool incognito,
                 JoinRouteCallback callback) override;
  void ConnectRouteByRouteId(const std::string& media_source,
                             const std::string& route_id,
                             const std::string& presentation_id,
                             const url::Origin& origin,
                             int32_t tab_id,
                             base::TimeDelta timeout,
                             bool incognito,
                             ConnectRouteByRouteIdCallback callback) override;
  void TerminateRoute(const std::string& route_id,
                      TerminateRouteCallback callback) override;
  void SendRouteMessage(const std::string& media_route_id,
                        const std::string& message) override;
  void SendRouteBinaryMessage(const std::string& media_route_id,
                              const std::vector<uint8_t>& data) override;
  void StartObservingMediaSinks(const std::string& media_source) override;
  void StopObservingMediaSinks(const std::string& media_source) override;
  void StartObservingMediaRoutes(const std::string& media_source) override;
  void StopObservingMediaRoutes(const std::string& media_source) override;
  void StartListeningForRouteMessages(const std::string& route_id) override;
  void StopListeningForRouteMessages(const std::string& route_id) override;
  void DetachRoute(const std::string& route_id) override;
  void EnableMdnsDiscovery() override;
  void UpdateMediaSinks(const std::string& media_source) override;
  void CreateMediaRouteController(
      const std::string& route_id,
      mojo::PendingReceiver<mojom::MediaController> media_controller,
      mojo::PendingRemote<mojom::MediaStatusObserver> observer,
      CreateMediaRouteControllerCallback callback) override;
  void GetState(GetStateCallback callback) override;

  void set_close_route_error_on_send() {
    close_route_with_error_on_send_ = true;
  }

  void set_route_error_message(std::string error_message) {
    route_error_message_ = error_message;
  }

  void set_empty_sink_list() { sinks_ = {}; }

  void set_unsupported_media_sources_list() {
    unsupported_media_sources_ = {"urn:x-org.chromium.media:source:tab:0",
                                  "urn:x-org.chromium.media:source:tab:*",
                                  "urn:x-org.chromium.media:source:desktop",
                                  "https://www.example.com/presentation.html"};
  }

  void set_delay(base::TimeDelta delay) { delay_ = delay; }

  std::vector<std::string> get_presentation_ids() {
    std::vector<std::string> presentation_ids;
    for (auto& element : presentation_ids_to_routes_)
      presentation_ids.push_back(element.first);
    return presentation_ids;
  }

  void CaptureOffScreenTab(content::WebContents* web_contents,
                           GURL source_urn,
                           std::string& presentation_id);

 private:
  base::WeakPtr<TestMediaRouteProvider> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  std::vector<MediaRoute> GetMediaRoutes();
  void SetSinks();
  void CreateRouteTimeOut(CreateRouteCallback callback);

  bool close_route_with_error_on_send_ = false;
  base::TimeDelta delay_ = base::TimeDelta::FromSeconds(0);
  std::string route_error_message_;
  std::map<std::string, MediaRoute> presentation_ids_to_routes_;
  std::map<MediaRoute::Id, MediaRoute> routes_;
  std::vector<MediaSinkInternal> sinks_;
  std::vector<std::string> unsupported_media_sources_;

  // Binds |this| to the Mojo receiver passed into the ctor.
  mojo::Receiver<mojom::MediaRouteProvider> receiver_;
  // Mojo remote to the Media Router.
  mojo::Remote<mojom::MediaRouter> media_router_;

  base::WeakPtrFactory<TestMediaRouteProvider> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_TEST_TEST_MEDIA_ROUTE_PROVIDER_H_
