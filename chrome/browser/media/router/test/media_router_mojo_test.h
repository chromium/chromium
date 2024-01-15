// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_TEST_MEDIA_ROUTER_MOJO_TEST_H_
#define CHROME_BROWSER_MEDIA_ROUTER_TEST_MEDIA_ROUTER_MOJO_TEST_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/media/router/mojo/media_router_desktop.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "chrome/test/base/testing_profile.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/media_router/common/mojom/media_status.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class MediaRouterDesktop;

// TODO(takumif): Move MockMediaRouteProvider into its own files.
class MockMediaRouteProvider : public mojom::MediaRouteProvider {
 public:
  using RouteCallback =
      base::OnceCallback<void(const std::optional<MediaRoute>&,
                              mojom::RoutePresentationConnectionPtr,
                              const std::optional<std::string>&,
                              mojom::RouteRequestResultCode)>;

  MockMediaRouteProvider();
  MockMediaRouteProvider(const MockMediaRouteProvider&) = delete;
  MockMediaRouteProvider& operator=(const MockMediaRouteProvider&) = delete;
  ~MockMediaRouteProvider() override;

  void CreateRoute(const std::string& source_urn,
                   const std::string& sink_id,
                   const std::string& presentation_id,
                   const url::Origin& origin,
                   int frame_tree_node_id,
                   base::TimeDelta timeout,
                   CreateRouteCallback callback) override {
    CreateRouteInternal(source_urn, sink_id, presentation_id, origin,
                        frame_tree_node_id, timeout, callback);
  }
  MOCK_METHOD(void,
              CreateRouteInternal,
              (const std::string& source_urn,
               const std::string& sink_id,
               const std::string& presentation_id,
               const url::Origin& origin,
               int frame_tree_node_id,
               base::TimeDelta timeout,
               CreateRouteCallback& callback));
  void JoinRoute(const std::string& source_urn,
                 const std::string& presentation_id,
                 const url::Origin& origin,
                 int frame_tree_node_id,
                 base::TimeDelta timeout,
                 JoinRouteCallback callback) override {
    JoinRouteInternal(source_urn, presentation_id, origin, frame_tree_node_id,
                      timeout, callback);
  }
  MOCK_METHOD(void,
              JoinRouteInternal,
              (const std::string& source_urn,
               const std::string& presentation_id,
               const url::Origin& origin,
               int frame_tree_node_id,
               base::TimeDelta timeout,
               JoinRouteCallback& callback));
  MOCK_METHOD(void, DetachRoute, (const std::string& route_id));
  void TerminateRoute(const std::string& route_id,
                      TerminateRouteCallback callback) override {
    TerminateRouteInternal(route_id, callback);
  }
  MOCK_METHOD2(TerminateRouteInternal,
               void(const std::string& route_id,
                    TerminateRouteCallback& callback));
  MOCK_METHOD1(StartObservingMediaSinks, void(const std::string& source));
  MOCK_METHOD1(StopObservingMediaSinks, void(const std::string& source));
  MOCK_METHOD2(SendRouteMessage,
               void(const std::string& media_route_id,
                    const std::string& message));
  MOCK_METHOD2(SendRouteBinaryMessage,
               void(const std::string& media_route_id,
                    const std::vector<uint8_t>& data));
  MOCK_METHOD1(OnPresentationSessionDetached,
               void(const std::string& route_id));
  MOCK_METHOD0(StartObservingMediaRoutes, void());
  MOCK_METHOD0(EnableMdnsDiscovery, void());
  MOCK_METHOD0(DiscoverSinksNow, void());
  void BindMediaController(
      const std::string& route_id,
      mojo::PendingReceiver<mojom::MediaController> media_controller,
      mojo::PendingRemote<mojom::MediaStatusObserver> observer,
      BindMediaControllerCallback callback) override {
    BindMediaControllerInternal(route_id, media_controller, observer, callback);
  }
  MOCK_METHOD4(
      BindMediaControllerInternal,
      void(const std::string& route_id,
           mojo::PendingReceiver<mojom::MediaController>& media_controller,
           mojo::PendingRemote<mojom::MediaStatusObserver>& observer,
           BindMediaControllerCallback& callback));
  void GetState(GetStateCallback callback) override {
    GetStateInternal(callback);
  }
  MOCK_METHOD1(GetStateInternal, void(const GetStateCallback& callback));

  // These methods execute the callbacks with the success or timeout result
  // code. If the callback takes a route, the route set in SetRouteToReturn() is
  // used.
  void RouteRequestSuccess(RouteCallback& cb) const;
  void RouteRequestTimeout(RouteCallback& cb) const;
  void TerminateRouteSuccess(TerminateRouteCallback& cb) const;
  void BindMediaControllerSuccess(BindMediaControllerCallback& cb) const;

  // Sets the route to pass into callbacks.
  void SetRouteToReturn(const MediaRoute& route);

 private:
  // The route that is passed into callbacks.
  std::optional<MediaRoute> route_;
};

class MockMediaStatusObserver : public mojom::MediaStatusObserver {
 public:
  explicit MockMediaStatusObserver(
      mojo::PendingReceiver<mojom::MediaStatusObserver> receiver);
  ~MockMediaStatusObserver() override;

  MOCK_METHOD1(OnMediaStatusUpdated, void(mojom::MediaStatusPtr status));

  // Use this instead of RunUntilIdle to explicitly show what we are waiting
  // for in a test.
  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  mojo::Receiver<mojom::MediaStatusObserver> receiver_;
};

class MockMediaController : public mojom::MediaController {
 public:
  MockMediaController();
  ~MockMediaController() override;

  void Bind(mojo::PendingReceiver<mojom::MediaController> receiver);
  mojo::PendingRemote<mojom::MediaController> BindInterfaceRemote();
  void CloseReceiver();

  MOCK_METHOD0(Play, void());
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD1(SetMute, void(bool mute));
  MOCK_METHOD1(SetVolume, void(float volume));
  MOCK_METHOD1(Seek, void(base::TimeDelta time));
  MOCK_METHOD0(NextTrack, void());
  MOCK_METHOD0(PreviousTrack, void());

 private:
  mojo::Receiver<mojom::MediaController> receiver_{this};
};

// Tests the API call flow between the MediaRouterDesktop and the Media Router
// Mojo service in both directions.
class MediaRouterMojoTest : public ::testing::Test {
 public:
  MediaRouterMojoTest();
  MediaRouterMojoTest(const MediaRouterMojoTest&) = delete;
  MediaRouterMojoTest& operator=(const MediaRouterMojoTest&) = delete;
  ~MediaRouterMojoTest() override;

 protected:
  void SetUp() override;
  void TearDown() override;

  // Creates a MediaRouterDesktop instance to be used for this test.
  virtual std::unique_ptr<MediaRouterDesktop> CreateMediaRouter() = 0;

  // Notify media router that the provider provides a route or a sink.
  // Need to be called after the provider is registered.
  void ProvideTestRoute(mojom::MediaRouteProviderId provider_id,
                        const MediaRoute::Id& route_id);
  void ProvideTestSink(mojom::MediaRouteProviderId provider_id,
                       const MediaSink::Id& sink_id);

  // Register |mock_cast_provider_| or |mock_wired_display_provider_| with
  // |media_router_|.
  void RegisterCastProvider();
  void RegisterWiredDisplayProvider();

  // Tests that calling MediaRouter methods result in calls to corresponding
  // MediaRouteProvider methods.
  void TestCreateRoute();
  void TestJoinRoute(const std::string& presentation_id);
  void TestTerminateRoute();
  void TestSendRouteMessage();
  void TestSendRouteBinaryMessage();
  void TestDetachRoute();

  MediaRouterDesktop* router() const { return media_router_.get(); }

  Profile* profile() { return &profile_; }

  // Mock objects.
  testing::NiceMock<MockMediaRouteProvider> mock_cast_provider_;
  testing::NiceMock<MockMediaRouteProvider> mock_wired_display_provider_;

 private:
  // Helper method for RegisterCastProvider() and
  // RegisterWiredDisplayProvider().
  void RegisterMediaRouteProvider(mojom::MediaRouteProvider* provider,
                                  mojom::MediaRouteProviderId provider_id);

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<MediaRouterDesktop> media_router_;
  mojo::ReceiverSet<mojom::MediaRouteProvider> provider_receivers_;
  std::unique_ptr<MediaRoutesObserver> routes_observer_;
  std::unique_ptr<MockMediaSinksObserver> sinks_observer_;
};

// An object whose Invoke method can be passed as a MediaRouteResponseCallback.
class RouteResponseCallbackHandler {
 public:
  RouteResponseCallbackHandler();
  RouteResponseCallbackHandler(const RouteResponseCallbackHandler&) = delete;
  RouteResponseCallbackHandler& operator=(const RouteResponseCallbackHandler&) =
      delete;
  ~RouteResponseCallbackHandler();

  // Calls DoInvoke with the contents of |connection| and |result|.
  void Invoke(mojom::RoutePresentationConnectionPtr connection,
              const RouteRequestResult& result);

  MOCK_METHOD5(DoInvoke,
               void(const MediaRoute* route,
                    const std::string& presentation_id,
                    const std::string& error_text,
                    mojom::RouteRequestResultCode result_code,
                    mojom::RoutePresentationConnectionPtr& connection));
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_TEST_MEDIA_ROUTER_MOJO_TEST_H_
