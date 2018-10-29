// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_TEST_MEDIA_ROUTER_MOJO_TEST_H_
#define CHROME_BROWSER_MEDIA_ROUTER_TEST_MEDIA_ROUTER_MOJO_TEST_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/media/router/event_page_request_manager.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_impl.h"
#include "chrome/browser/media/router/test/mock_media_router.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/common/media_router/mojo/media_router.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/event_page_tracker.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class MediaRouterMojoImpl;

// TODO(takumif): Move MockMediaRouteProvider into its own files.
class MockMediaRouteProvider : public mojom::MediaRouteProvider {
 public:
  using RouteCallback =
      base::OnceCallback<void(const base::Optional<MediaRoute>&,
                              mojom::RoutePresentationConnectionPtr,
                              const base::Optional<std::string>&,
                              RouteRequestResult::ResultCode)>;

  MockMediaRouteProvider();
  ~MockMediaRouteProvider() override;

  void CreateRoute(const std::string& source_urn,
                   const std::string& sink_id,
                   const std::string& presentation_id,
                   const url::Origin& origin,
                   int tab_id,
                   base::TimeDelta timeout,
                   bool incognito,
                   CreateRouteCallback callback) override {
    CreateRouteInternal(source_urn, sink_id, presentation_id, origin, tab_id,
                        timeout, incognito, callback);
  }
  MOCK_METHOD8(CreateRouteInternal,
               void(const std::string& source_urn,
                    const std::string& sink_id,
                    const std::string& presentation_id,
                    const url::Origin& origin,
                    int tab_id,
                    base::TimeDelta timeout,
                    bool incognito,
                    CreateRouteCallback& callback));
  void JoinRoute(const std::string& source_urn,
                 const std::string& presentation_id,
                 const url::Origin& origin,
                 int tab_id,
                 base::TimeDelta timeout,
                 bool incognito,
                 JoinRouteCallback callback) override {
    JoinRouteInternal(source_urn, presentation_id, origin, tab_id, timeout,
                      incognito, callback);
  }
  MOCK_METHOD7(JoinRouteInternal,
               void(const std::string& source_urn,
                    const std::string& presentation_id,
                    const url::Origin& origin,
                    int tab_id,
                    base::TimeDelta timeout,
                    bool incognito,
                    JoinRouteCallback& callback));
  void ConnectRouteByRouteId(const std::string& source_urn,
                             const std::string& route_id,
                             const std::string& presentation_id,
                             const url::Origin& origin,
                             int tab_id,
                             base::TimeDelta timeout,
                             bool incognito,
                             JoinRouteCallback callback) override {
    ConnectRouteByRouteIdInternal(source_urn, route_id, presentation_id, origin,
                                  tab_id, timeout, incognito, callback);
  }
  MOCK_METHOD8(ConnectRouteByRouteIdInternal,
               void(const std::string& source_urn,
                    const std::string& route_id,
                    const std::string& presentation_id,
                    const url::Origin& origin,
                    int tab_id,
                    base::TimeDelta timeout,
                    bool incognito,
                    JoinRouteCallback& callback));
  MOCK_METHOD1(DetachRoute, void(const std::string& route_id));
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
  MOCK_METHOD1(StartListeningForRouteMessages,
               void(const std::string& route_id));
  MOCK_METHOD1(StopListeningForRouteMessages,
               void(const std::string& route_id));
  MOCK_METHOD1(OnPresentationSessionDetached,
               void(const std::string& route_id));
  MOCK_METHOD1(StartObservingMediaRoutes, void(const std::string& source));
  MOCK_METHOD1(StopObservingMediaRoutes, void(const std::string& source));
  MOCK_METHOD0(EnableMdnsDiscovery, void());
  MOCK_METHOD1(UpdateMediaSinks, void(const std::string& source));
  void SearchSinks(const std::string& sink_id,
                   const std::string& media_source,
                   mojom::SinkSearchCriteriaPtr search_criteria,
                   SearchSinksCallback callback) override {
    SearchSinksInternal(sink_id, media_source, search_criteria, callback);
  }
  MOCK_METHOD4(SearchSinksInternal,
               void(const std::string& sink_id,
                    const std::string& media_source,
                    mojom::SinkSearchCriteriaPtr& search_criteria,
                    SearchSinksCallback& callback));
  MOCK_METHOD2(ProvideSinks,
               void(const std::string&, const std::vector<MediaSinkInternal>&));
  void CreateMediaRouteController(
      const std::string& route_id,
      mojom::MediaControllerRequest media_controller,
      mojom::MediaStatusObserverPtr observer,
      CreateMediaRouteControllerCallback callback) override {
    CreateMediaRouteControllerInternal(route_id, media_controller, observer,
                                       callback);
  }
  MOCK_METHOD4(CreateMediaRouteControllerInternal,
               void(const std::string& route_id,
                    mojom::MediaControllerRequest& media_controller,
                    mojom::MediaStatusObserverPtr& observer,
                    CreateMediaRouteControllerCallback& callback));

  // These methods execute the callbacks with the success or timeout result
  // code. If the callback takes a route, the route set in SetRouteToReturn() is
  // used.
  void RouteRequestSuccess(RouteCallback& cb) const;
  void RouteRequestTimeout(RouteCallback& cb) const;
  void TerminateRouteSuccess(TerminateRouteCallback& cb) const;
  void SearchSinksSuccess(SearchSinksCallback& cb) const;
  void CreateMediaRouteControllerSuccess(
      CreateMediaRouteControllerCallback& cb) const;

  // Sets the route to pass into callbacks.
  void SetRouteToReturn(const MediaRoute& route);

 private:
  // The route that is passed into callbacks.
  base::Optional<MediaRoute> route_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaRouteProvider);
};

class MockEventPageTracker : public extensions::EventPageTracker {
 public:
  MockEventPageTracker();
  ~MockEventPageTracker();

  MOCK_METHOD1(IsEventPageSuspended, bool(const std::string& extension_id));
  MOCK_METHOD2(WakeEventPage,
               bool(const std::string& extension_id,
                    base::OnceCallback<void(bool)> callback));
};

class MockEventPageRequestManager : public EventPageRequestManager {
 public:
  static std::unique_ptr<KeyedService> Create(content::BrowserContext* context);

  explicit MockEventPageRequestManager(content::BrowserContext* context);
  ~MockEventPageRequestManager() override;

  MOCK_METHOD1(SetExtensionId, void(const std::string& extension_id));
  void RunOrDefer(base::OnceClosure request,
                  MediaRouteProviderWakeReason wake_reason) override;
  MOCK_METHOD2(RunOrDeferInternal,
               void(base::OnceClosure& request,
                    MediaRouteProviderWakeReason wake_reason));
  MOCK_METHOD0(OnMojoConnectionsReady, void());
  MOCK_METHOD0(OnMojoConnectionError, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEventPageRequestManager);
};

class MockMediaStatusObserver : public mojom::MediaStatusObserver {
 public:
  explicit MockMediaStatusObserver(mojom::MediaStatusObserverRequest request);
  ~MockMediaStatusObserver() override;

  MOCK_METHOD1(OnMediaStatusUpdated, void(const MediaStatus& status));

 private:
  mojo::Binding<mojom::MediaStatusObserver> binding_;
};

class MockMediaController : public mojom::MediaController,
                            mojom::HangoutsMediaRouteController {
 public:
  MockMediaController();
  ~MockMediaController() override;

  void Bind(mojom::MediaControllerRequest request);
  mojom::MediaControllerPtr BindInterfacePtr();
  void CloseBinding();

  MOCK_METHOD0(Play, void());
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD1(SetMute, void(bool mute));
  MOCK_METHOD1(SetVolume, void(float volume));
  MOCK_METHOD1(Seek, void(base::TimeDelta time));
  void ConnectHangoutsMediaRouteController(
      mojom::HangoutsMediaRouteControllerRequest controller_request) override {
    hangouts_binding_.Bind(std::move(controller_request));
    ConnectHangoutsMediaRouteController();
  };
  MOCK_METHOD0(ConnectHangoutsMediaRouteController, void());
  MOCK_METHOD1(SetLocalPresent, void(bool local_present));

 private:
  mojo::Binding<mojom::MediaController> binding_;
  mojo::Binding<mojom::HangoutsMediaRouteController> hangouts_binding_;
};

class MockMediaRouteController : public MediaRouteController {
 public:
  MockMediaRouteController(const MediaRoute::Id& route_id,
                           content::BrowserContext* context,
                           MediaRouter* router);
  MOCK_METHOD0(Play, void());
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD1(Seek, void(base::TimeDelta time));
  MOCK_METHOD1(SetMute, void(bool mute));
  MOCK_METHOD1(SetVolume, void(float volume));

 protected:
  // The dtor is protected because MockMediaRouteController is ref-counted.
  ~MockMediaRouteController() override;
};

class MockMediaRouteControllerObserver : public MediaRouteController::Observer {
 public:
  MockMediaRouteControllerObserver(
      scoped_refptr<MediaRouteController> controller);
  ~MockMediaRouteControllerObserver() override;

  MOCK_METHOD1(OnMediaStatusUpdated, void(const MediaStatus& status));
  MOCK_METHOD0(OnControllerInvalidated, void());
};

// Tests the API call flow between the MediaRouterMojoImpl and the Media Router
// Mojo service in both directions.
class MediaRouterMojoTest : public ::testing::Test {
 public:
  MediaRouterMojoTest();
  ~MediaRouterMojoTest() override;

 protected:
  void SetUp() override;
  void TearDown() override;

  // Creates a MediaRouterMojoImpl instance to be used for this test.
  virtual std::unique_ptr<MediaRouterMojoImpl> CreateMediaRouter() = 0;

  // Notify media router that the provider provides a route or a sink.
  // Need to be called after the provider is registered.
  void ProvideTestRoute(MediaRouteProviderId provider_id,
                        const MediaRoute::Id& route_id);
  void ProvideTestSink(MediaRouteProviderId provider_id,
                       const MediaSink::Id& sink_id);

  // Register |mock_extension_provider_| or |mock_wired_display_provider_| with
  // |media_router_|.
  void RegisterExtensionProvider();
  void RegisterWiredDisplayProvider();

  // Tests that calling MediaRouter methods result in calls to corresponding
  // MediaRouteProvider methods.
  void TestCreateRoute();
  void TestJoinRoute(const std::string& presentation_id);
  void TestConnectRouteByRouteId();
  void TestTerminateRoute();
  void TestSendRouteMessage();
  void TestSendRouteBinaryMessage();
  void TestDetachRoute();
  void TestSearchSinks();
  void TestCreateMediaRouteController();
  void TestCreateHangoutsMediaRouteController();

  const std::string& extension_id() const { return extension_->id(); }

  MediaRouterMojoImpl* router() const { return media_router_.get(); }

  Profile* profile() { return &profile_; }

  // Mock objects.
  MockMediaRouteProvider mock_extension_provider_;
  MockMediaRouteProvider mock_wired_display_provider_;
  MockEventPageRequestManager* request_manager_ = nullptr;

 private:
  // Helper method for RegisterExtensionProvider() and
  // RegisterWiredDisplayProvider().
  void RegisterMediaRouteProvider(mojom::MediaRouteProvider* provider,
                                  MediaRouteProviderId provider_id);

  content::TestBrowserThreadBundle test_thread_bundle_;
  scoped_refptr<const extensions::Extension> extension_;
  TestingProfile profile_;
  std::unique_ptr<MediaRouterMojoImpl> media_router_;
  mojo::BindingSet<mojom::MediaRouteProvider> provider_bindings_;
  std::unique_ptr<MediaRoutesObserver> routes_observer_;
  std::unique_ptr<MockMediaSinksObserver> sinks_observer_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterMojoTest);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_TEST_MEDIA_ROUTER_MOJO_TEST_H_
