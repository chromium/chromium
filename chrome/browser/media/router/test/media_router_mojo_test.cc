// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/test/media_router_mojo_test.h"

#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/media/router/event_page_request_manager_factory.h"
#include "extensions/common/extension_builder.h"

using testing::_;
using testing::ByRef;
using testing::Invoke;
using testing::Not;
using testing::Pointee;

namespace media_router {

namespace {

const char kDescription[] = "description";
const char kInstanceId[] = "instance123";
const char kMessage[] = "message";
const char kOrigin[] = "http://origin/";
const char kPresentationId[] = "presentationId";
const char kRouteId[] = "routeId";
const char kSource[] = "source1";
const char kSinkId[] = "sink";
const char kSinkId2[] = "sink2";
const int kInvalidTabId = -1;
const int kTimeoutMillis = 5 * 1000;
const uint8_t kBinaryMessage[] = {0x01, 0x02, 0x03, 0x04};

MATCHER_P(Equals, value, "") {
  return arg.Equals(value.get());
}

// Creates a media route whose ID is |kRouteId|.
MediaRoute CreateMediaRoute() {
  MediaRoute route(kRouteId, MediaSource(kSource), kSinkId, kDescription, true,
                   true);
  route.set_presentation_id(kPresentationId);
  route.set_controller_type(RouteControllerType::kGeneric);
  return route;
}

class SendMessageCallbackHandler {
 public:
  MOCK_METHOD1(Invoke, void(bool));
};

class SinkResponseCallbackHandler {
 public:
  MOCK_METHOD1(Invoke, void(const std::string& sink_id));
};

}  // namespace

MockMediaRouteProvider::MockMediaRouteProvider() {}

MockMediaRouteProvider::~MockMediaRouteProvider() {}

void MockMediaRouteProvider::RouteRequestSuccess(RouteCallback& cb) const {
  DCHECK(route_);
  std::move(cb).Run(route_, nullptr, std::string(), RouteRequestResult::OK);
}

void MockMediaRouteProvider::RouteRequestTimeout(RouteCallback& cb) const {
  std::move(cb).Run(base::nullopt, nullptr, std::string("error"),
                    RouteRequestResult::TIMED_OUT);
}

void MockMediaRouteProvider::TerminateRouteSuccess(
    TerminateRouteCallback& cb) const {
  std::move(cb).Run(std::string(), RouteRequestResult::OK);
}

void MockMediaRouteProvider::SearchSinksSuccess(SearchSinksCallback& cb) const {
  std::string sink_id = route_ ? route_->media_sink_id() : std::string();
  std::move(cb).Run(sink_id);
}

void MockMediaRouteProvider::CreateMediaRouteControllerSuccess(
    CreateMediaRouteControllerCallback& cb) const {
  std::move(cb).Run(true);
}

void MockMediaRouteProvider::SetRouteToReturn(const MediaRoute& route) {
  route_ = route;
}

MockEventPageTracker::MockEventPageTracker() {}

MockEventPageTracker::~MockEventPageTracker() {}

// static
std::unique_ptr<KeyedService> MockEventPageRequestManager::Create(
    content::BrowserContext* context) {
  return std::make_unique<MockEventPageRequestManager>(context);
}

MockEventPageRequestManager::MockEventPageRequestManager(
    content::BrowserContext* context)
    : EventPageRequestManager(context) {}

MockEventPageRequestManager::~MockEventPageRequestManager() = default;

void MockEventPageRequestManager::RunOrDefer(
    base::OnceClosure request,
    MediaRouteProviderWakeReason wake_reason) {
  RunOrDeferInternal(request, wake_reason);
}

MockMediaStatusObserver::MockMediaStatusObserver(
    mojo::PendingReceiver<mojom::MediaStatusObserver> receiver)
    : receiver_(this, std::move(receiver)) {}

MockMediaStatusObserver::~MockMediaStatusObserver() {}

MockMediaController::MockMediaController() = default;

MockMediaController::~MockMediaController() = default;

void MockMediaController::Bind(
    mojo::PendingReceiver<mojom::MediaController> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

mojo::PendingRemote<mojom::MediaController>
MockMediaController::BindInterfaceRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void MockMediaController::CloseReceiver() {
  receiver_.reset();
}

MediaRouterMojoTest::MediaRouterMojoTest() {
  request_manager_ = static_cast<MockEventPageRequestManager*>(
      EventPageRequestManagerFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(),
          base::BindRepeating(&MockEventPageRequestManager::Create)));
  request_manager_->set_mojo_connections_ready_for_test(true);
  ON_CALL(*request_manager_, RunOrDeferInternal(_, _))
      .WillByDefault(Invoke([](base::OnceClosure& request,
                               MediaRouteProviderWakeReason wake_reason) {
        std::move(request).Run();
      }));
}

MediaRouterMojoTest::~MediaRouterMojoTest() {}

void MediaRouterMojoTest::RegisterExtensionProvider() {
  RegisterMediaRouteProvider(&mock_extension_provider_,
                             MediaRouteProviderId::EXTENSION);
}

void MediaRouterMojoTest::RegisterWiredDisplayProvider() {
  RegisterMediaRouteProvider(&mock_wired_display_provider_,
                             MediaRouteProviderId::WIRED_DISPLAY);
}

void MediaRouterMojoTest::SetUp() {
  media_router_ = CreateMediaRouter();
  media_router_->set_instance_id_for_test(kInstanceId);
  RegisterExtensionProvider();
  media_router_->Initialize();
  extension_ = extensions::ExtensionBuilder("Test").Build();
  base::RunLoop().RunUntilIdle();
}

void MediaRouterMojoTest::TearDown() {
  sinks_observer_.reset();
  routes_observer_.reset();
  media_router_->Shutdown();
  media_router_.reset();
}

void MediaRouterMojoTest::ProvideTestRoute(MediaRouteProviderId provider_id,
                                           const MediaRoute::Id& route_id) {
  if (!routes_observer_)
    routes_observer_ = std::make_unique<MediaRoutesObserver>(router(), kSource);
  MediaRoute route = CreateMediaRoute();
  route.set_media_route_id(route_id);
  router()->OnRoutesUpdated(provider_id, {route}, kSource, {});
}

void MediaRouterMojoTest::ProvideTestSink(MediaRouteProviderId provider_id,
                                          const MediaSink::Id& sink_id) {
  if (!sinks_observer_) {
    sinks_observer_ = std::make_unique<MockMediaSinksObserver>(
        router(), MediaSource(kSource), url::Origin::Create(GURL(kOrigin)));
    router()->RegisterMediaSinksObserver(sinks_observer_.get());
  }

  MediaSinkInternal test_sink;
  test_sink.sink().set_sink_id(sink_id);
  test_sink.sink().set_provider_id(provider_id);
  router()->OnSinksReceived(provider_id, kSource, {test_sink}, {});
}

void MediaRouterMojoTest::TestCreateRoute() {
  MediaSource media_source(kSource);
  MediaRoute expected_route(kRouteId, media_source, kSinkId, kDescription, true,
                            true);
  expected_route.set_presentation_id(kPresentationId);
  expected_route.set_controller_type(RouteControllerType::kGeneric);

  ProvideTestSink(MediaRouteProviderId::EXTENSION, kSinkId);

  // Use a lambda function as an invocation target here to work around
  // a limitation with GMock::Invoke that prevents it from using move-only types
  // in runnable parameter lists.
  EXPECT_CALL(mock_extension_provider_,
              CreateRouteInternal(kSource, kSinkId, _,
                                  url::Origin::Create(GURL(kOrigin)),
                                  kInvalidTabId, _, _, _))
      .WillOnce(Invoke([](const std::string& source, const std::string& sink,
                          const std::string& presentation_id,
                          const url::Origin& origin, int tab_id,
                          base::TimeDelta timeout, bool incognito,
                          mojom::MediaRouteProvider::CreateRouteCallback& cb) {
        std::move(cb).Run(CreateMediaRoute(), nullptr, std::string(),
                          RouteRequestResult::OK);
      }));

  RouteResponseCallbackHandler handler;
  EXPECT_CALL(handler, DoInvoke(Pointee(expected_route), Not(""), "",
                                RouteRequestResult::OK, _));
  router()->CreateRoute(
      kSource, kSinkId, url::Origin::Create(GURL(kOrigin)), nullptr,
      base::BindOnce(&RouteResponseCallbackHandler::Invoke,
                     base::Unretained(&handler)),
      base::TimeDelta::FromMilliseconds(kTimeoutMillis), false);
  base::RunLoop().RunUntilIdle();
}

void MediaRouterMojoTest::TestJoinRoute(const std::string& presentation_id) {
  MediaSource media_source(kSource);
  MediaRoute expected_route(kRouteId, media_source, kSinkId, kDescription, true,
                            true);
  expected_route.set_presentation_id(kPresentationId);
  expected_route.set_controller_type(RouteControllerType::kGeneric);

  MediaRoute route = CreateMediaRoute();
  // Make sure the MR has received an update with the route, so it knows there
  // is a route to join.
  std::vector<MediaRoute> routes;
  routes.push_back(route);
  router()->OnRoutesUpdated(MediaRouteProviderId::EXTENSION, routes,
                            std::string(), std::vector<std::string>());
  EXPECT_TRUE(router()->HasJoinableRoute());

  // Use a lambda function as an invocation target here to work around
  // a limitation with GMock::Invoke that prevents it from using move-only types
  // in runnable parameter lists.
  EXPECT_CALL(mock_extension_provider_,
              JoinRouteInternal(
                  kSource, presentation_id, url::Origin::Create(GURL(kOrigin)),
                  kInvalidTabId,
                  base::TimeDelta::FromMilliseconds(kTimeoutMillis), _, _))
      .WillOnce(
          Invoke([&route](const std::string& source,
                          const std::string& presentation_id,
                          const url::Origin& origin, int tab_id,
                          base::TimeDelta timeout, bool incognito,
                          mojom::MediaRouteProvider::JoinRouteCallback& cb) {
            std::move(cb).Run(route, nullptr, std::string(),
                              RouteRequestResult::OK);
          }));

  RouteResponseCallbackHandler handler;
  EXPECT_CALL(handler, DoInvoke(Pointee(expected_route), Not(""), "",
                                RouteRequestResult::OK, _));
  router()->JoinRoute(kSource, presentation_id,
                      url::Origin::Create(GURL(kOrigin)), nullptr,
                      base::BindOnce(&RouteResponseCallbackHandler::Invoke,
                                     base::Unretained(&handler)),
                      base::TimeDelta::FromMilliseconds(kTimeoutMillis), false);
  base::RunLoop().RunUntilIdle();
}

void MediaRouterMojoTest::TestConnectRouteByRouteId() {
  MediaSource media_source(kSource);
  MediaRoute expected_route(kRouteId, media_source, kSinkId, kDescription, true,
                            true);
  expected_route.set_presentation_id(kPresentationId);
  expected_route.set_controller_type(RouteControllerType::kGeneric);
  MediaRoute route = CreateMediaRoute();
  ProvideTestRoute(MediaRouteProviderId::EXTENSION, kRouteId);

  // Use a lambda function as an invocation target here to work around
  // a limitation with GMock::Invoke that prevents it from using move-only types
  // in runnable parameter lists.
  EXPECT_CALL(mock_extension_provider_,
              ConnectRouteByRouteIdInternal(
                  kSource, kRouteId, _, url::Origin::Create(GURL(kOrigin)),
                  kInvalidTabId,
                  base::TimeDelta::FromMilliseconds(kTimeoutMillis), false, _))
      .WillOnce(Invoke(
          [&route](const std::string& source, const std::string& route_id,
                   const std::string& presentation_id,
                   const url::Origin& origin, int tab_id,
                   base::TimeDelta timeout, bool incognito,
                   mojom::MediaRouteProvider::JoinRouteCallback& cb) {
            std::move(cb).Run(route, nullptr, std::string(),
                              RouteRequestResult::OK);
          }));

  RouteResponseCallbackHandler handler;
  EXPECT_CALL(handler, DoInvoke(Pointee(expected_route), Not(""), "",
                                RouteRequestResult::OK, _));
  router()->ConnectRouteByRouteId(
      kSource, kRouteId, url::Origin::Create(GURL(kOrigin)), nullptr,
      base::BindOnce(&RouteResponseCallbackHandler::Invoke,
                     base::Unretained(&handler)),
      base::TimeDelta::FromMilliseconds(kTimeoutMillis), false);
  base::RunLoop().RunUntilIdle();
}

void MediaRouterMojoTest::TestTerminateRoute() {
  ProvideTestRoute(MediaRouteProviderId::EXTENSION, kRouteId);
  EXPECT_CALL(mock_extension_provider_, TerminateRouteInternal(kRouteId, _))
      .WillOnce(
          Invoke([](const std::string& route_id,
                    mojom::MediaRouteProvider::TerminateRouteCallback& cb) {
            std::move(cb).Run(base::nullopt, RouteRequestResult::OK);
          }));
  router()->TerminateRoute(kRouteId);
  base::RunLoop().RunUntilIdle();
}

void MediaRouterMojoTest::TestSendRouteMessage() {
  ProvideTestRoute(MediaRouteProviderId::EXTENSION, kRouteId);
  EXPECT_CALL(mock_extension_provider_, SendRouteMessage(kRouteId, kMessage));
  router()->SendRouteMessage(kRouteId, kMessage);
  base::RunLoop().RunUntilIdle();
}

void MediaRouterMojoTest::TestSendRouteBinaryMessage() {
  ProvideTestRoute(MediaRouteProviderId::EXTENSION, kRouteId);
  auto expected_binary_data = std::make_unique<std::vector<uint8_t>>(
      kBinaryMessage, kBinaryMessage + base::size(kBinaryMessage));
  EXPECT_CALL(mock_extension_provider_, SendRouteBinaryMessage(kRouteId, _))
      .WillOnce([](const MediaRoute::Id& route_id,
                   const std::vector<uint8_t>& data) {
        EXPECT_EQ(
            0, memcmp(kBinaryMessage, &(data[0]), base::size(kBinaryMessage)));
      });

  router()->SendRouteBinaryMessage(kRouteId, std::move(expected_binary_data));
  base::RunLoop().RunUntilIdle();
}

void MediaRouterMojoTest::TestDetachRoute() {
  ProvideTestRoute(MediaRouteProviderId::EXTENSION, kRouteId);
  EXPECT_CALL(mock_extension_provider_, DetachRoute(kRouteId));
  router()->DetachRoute(kRouteId);
  base::RunLoop().RunUntilIdle();
}

void MediaRouterMojoTest::TestSearchSinks() {
  std::string search_input("input");
  std::string domain("google.com");
  MediaSource media_source(kSource);
  ProvideTestSink(MediaRouteProviderId::EXTENSION, kSinkId);

  EXPECT_CALL(mock_extension_provider_,
              SearchSinksInternal(kSinkId, kSource, _, _))
      .WillOnce(
          Invoke([&search_input, &domain](
                     const std::string& sink_id, const std::string& source,
                     const mojom::SinkSearchCriteriaPtr& search_criteria,
                     mojom::MediaRouteProvider::SearchSinksCallback& cb) {
            EXPECT_EQ(search_input, search_criteria->input);
            EXPECT_EQ(domain, search_criteria->domain);
            std::move(cb).Run(kSinkId2);
          }));

  SinkResponseCallbackHandler sink_handler;
  EXPECT_CALL(sink_handler, Invoke(kSinkId2)).Times(1);
  MediaSinkSearchResponseCallback sink_callback = base::BindOnce(
      &SinkResponseCallbackHandler::Invoke, base::Unretained(&sink_handler));

  router()->SearchSinks(kSinkId, kSource, search_input, domain,
                        std::move(sink_callback));
  base::RunLoop().RunUntilIdle();
}

void MediaRouterMojoTest::RegisterMediaRouteProvider(
    mojom::MediaRouteProvider* provider,
    MediaRouteProviderId provider_id) {
  mojo::PendingRemote<mojom::MediaRouteProvider> mojo_provider;
  provider_receivers_.Add(provider,
                          mojo_provider.InitWithNewPipeAndPassReceiver());
  media_router_->RegisterMediaRouteProvider(
      provider_id, std::move(mojo_provider),
      base::BindOnce([](const std::string& instance_id,
                        mojom::MediaRouteProviderConfigPtr config) {}));
}

RouteResponseCallbackHandler::RouteResponseCallbackHandler() = default;
RouteResponseCallbackHandler::~RouteResponseCallbackHandler() = default;

void RouteResponseCallbackHandler::Invoke(
    mojom::RoutePresentationConnectionPtr connection,
    const RouteRequestResult& result) {
  DoInvoke(result.route(), result.presentation_id(), result.error(),
           result.result_code(), connection);
}

}  // namespace media_router
