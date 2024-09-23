// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/media/router/test/media_router_mojo_test.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"

using testing::_;
using testing::ByRef;
using testing::Invoke;
using testing::NiceMock;
using testing::Not;
using testing::Pointee;
using testing::WithArg;

namespace media_router {

namespace {

const char kDescription[] = "description";
const char kMessage[] = "message";
const char kOrigin[] = "http://origin/";
const char kPresentationId[] = "presentationId";
const char kRouteId[] = "routeId";
const char kSource[] = "source1";
const char kSinkId[] = "sink";
const int kInvalidFrameTreeNodeId = -1;
const int kTimeoutMillis = 5 * 1000;
const uint8_t kBinaryMessage[] = {0x01, 0x02, 0x03, 0x04};

MATCHER_P(Equals, value, "") {
  return arg.Equals(value.get());
}

// Creates a media route whose ID is |kRouteId|.
MediaRoute CreateMediaRoute() {
  MediaRoute route(kRouteId, MediaSource(kSource), kSinkId, kDescription, true);
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
  std::move(cb).Run(route_, nullptr, std::string(),
                    mojom::RouteRequestResultCode::OK);
}

void MockMediaRouteProvider::RouteRequestTimeout(RouteCallback& cb) const {
  std::move(cb).Run(std::nullopt, nullptr, std::string("error"),
                    mojom::RouteRequestResultCode::TIMED_OUT);
}

void MockMediaRouteProvider::TerminateRouteSuccess(
    TerminateRouteCallback& cb) const {
  std::move(cb).Run(std::string(), mojom::RouteRequestResultCode::OK);
}

void MockMediaRouteProvider::BindMediaControllerSuccess(
    BindMediaControllerCallback& cb) const {
  std::move(cb).Run(true);
}

void MockMediaRouteProvider::SetRouteToReturn(const MediaRoute& route) {
  route_ = route;
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

MediaRouterMojoTest::MediaRouterMojoTest() = default;

MediaRouterMojoTest::~MediaRouterMojoTest() = default;

void MediaRouterMojoTest::RegisterCastProvider() {
  RegisterMediaRouteProvider(&mock_cast_provider_,
                             mojom::MediaRouteProviderId::CAST);
}

void MediaRouterMojoTest::RegisterWiredDisplayProvider() {
  RegisterMediaRouteProvider(&mock_wired_display_provider_,
                             mojom::MediaRouteProviderId::WIRED_DISPLAY);
}

void MediaRouterMojoTest::SetUp() {
  media_router_ = CreateMediaRouter();
  RegisterCastProvider();
  media_router_->Initialize();
  base::RunLoop().RunUntilIdle();
}

void MediaRouterMojoTest::TearDown() {
  sinks_observer_.reset();
  routes_observer_.reset();
  media_router_->Shutdown();
  media_router_.reset();
}

void MediaRouterMojoTest::ProvideTestRoute(
    mojom::MediaRouteProviderId provider_id,
    const MediaRoute::Id& route_id) {
  if (!routes_observer_) {
    routes_observer_ = std::make_unique<MediaRoutesObserver>(router());
  }
  MediaRoute route = CreateMediaRoute();
  route.set_media_route_id(route_id);
  router()->OnRoutesUpdated(provider_id, {route});
}

void MediaRouterMojoTest::ProvideTestSink(
    mojom::MediaRouteProviderId provider_id,
    const MediaSink::Id& sink_id) {
  if (!sinks_observer_) {
    sinks_observer_ = std::make_unique<NiceMock<MockMediaSinksObserver>>(
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
  MediaRoute expected_route(kRouteId, media_source, kSinkId, kDescription,
                            true);
  expected_route.set_presentation_id(kPresentationId);
  expected_route.set_controller_type(RouteControllerType::kGeneric);

  ProvideTestSink(mojom::MediaRouteProviderId::CAST, kSinkId);

  // Use a lambda function as an invocation target here to work around
  // a limitation with GMock::Invoke that prevents it from using move-only types
  // in runnable parameter lists.
  EXPECT_CALL(mock_cast_provider_,
              CreateRouteInternal(kSource, kSinkId, _,
                                  url::Origin::Create(GURL(kOrigin)),
                                  kInvalidFrameTreeNodeId, _, _))
      .WillOnce(WithArg<6>(
          Invoke([](mojom::MediaRouteProvider::CreateRouteCallback& cb) {
            std::move(cb).Run(CreateMediaRoute(), nullptr, std::string(),
                              mojom::RouteRequestResultCode::OK);
          })));

  RouteResponseCallbackHandler handler;
  EXPECT_CALL(handler, DoInvoke(Pointee(expected_route), Not(""), "",
                                mojom::RouteRequestResultCode::OK, _));
  router()->CreateRoute(kSource, kSinkId, url::Origin::Create(GURL(kOrigin)),
                        nullptr,
                        base::BindOnce(&RouteResponseCallbackHandler::Invoke,
                                       base::Unretained(&handler)),
                        base::Milliseconds(kTimeoutMillis));
  base::RunLoop().RunUntilIdle();
}

void MediaRouterMojoTest::TestJoinRoute(const std::string& presentation_id) {
  MediaSource media_source(kSource);
  MediaRoute expected_route(kRouteId, media_source, kSinkId, kDescription,
                            true);
  expected_route.set_presentation_id(kPresentationId);
  expected_route.set_controller_type(RouteControllerType::kGeneric);

  MediaRoute route = CreateMediaRoute();
  // Make sure the MR has received an update with the route, so it knows there
  // is a route to join.
  std::vector<MediaRoute> routes;
  routes.push_back(route);
  router()->OnRoutesUpdated(mojom::MediaRouteProviderId::CAST, routes);
  EXPECT_TRUE(router()->HasJoinableRoute());

  // Use a lambda function as an invocation target here to work around
  // a limitation with GMock::Invoke that prevents it from using move-only types
  // in runnable parameter lists.
  EXPECT_CALL(mock_cast_provider_,
              JoinRouteInternal(kSource, presentation_id,
                                url::Origin::Create(GURL(kOrigin)),
                                kInvalidFrameTreeNodeId,
                                base::Milliseconds(kTimeoutMillis), _))
      .WillOnce(WithArg<5>(
          Invoke([&route](mojom::MediaRouteProvider::JoinRouteCallback& cb) {
            std::move(cb).Run(route, nullptr, std::string(),
                              mojom::RouteRequestResultCode::OK);
          })));

  RouteResponseCallbackHandler handler;
  EXPECT_CALL(handler, DoInvoke(Pointee(expected_route), Not(""), "",
                                mojom::RouteRequestResultCode::OK, _));
  router()->JoinRoute(kSource, presentation_id,
                      url::Origin::Create(GURL(kOrigin)), nullptr,
                      base::BindOnce(&RouteResponseCallbackHandler::Invoke,
                                     base::Unretained(&handler)),
                      base::Milliseconds(kTimeoutMillis));
  base::RunLoop().RunUntilIdle();
}

void MediaRouterMojoTest::TestTerminateRoute() {
  ProvideTestRoute(mojom::MediaRouteProviderId::CAST, kRouteId);
  EXPECT_CALL(mock_cast_provider_, TerminateRouteInternal(kRouteId, _))
      .WillOnce(
          Invoke([](const std::string& route_id,
                    mojom::MediaRouteProvider::TerminateRouteCallback& cb) {
            std::move(cb).Run(std::nullopt, mojom::RouteRequestResultCode::OK);
          }));
  router()->TerminateRoute(kRouteId);
  base::RunLoop().RunUntilIdle();
}

void MediaRouterMojoTest::TestSendRouteMessage() {
  ProvideTestRoute(mojom::MediaRouteProviderId::CAST, kRouteId);
  EXPECT_CALL(mock_cast_provider_, SendRouteMessage(kRouteId, kMessage));
  router()->SendRouteMessage(kRouteId, kMessage);
  base::RunLoop().RunUntilIdle();
}

void MediaRouterMojoTest::TestSendRouteBinaryMessage() {
  ProvideTestRoute(mojom::MediaRouteProviderId::CAST, kRouteId);
  auto expected_binary_data = std::make_unique<std::vector<uint8_t>>(
      kBinaryMessage, kBinaryMessage + std::size(kBinaryMessage));
  EXPECT_CALL(mock_cast_provider_, SendRouteBinaryMessage(kRouteId, _))
      .WillOnce([](const MediaRoute::Id& route_id,
                   const std::vector<uint8_t>& data) {
        EXPECT_EQ(
            0, memcmp(kBinaryMessage, &(data[0]), std::size(kBinaryMessage)));
      });

  router()->SendRouteBinaryMessage(kRouteId, std::move(expected_binary_data));
  base::RunLoop().RunUntilIdle();
}

void MediaRouterMojoTest::TestDetachRoute() {
  ProvideTestRoute(mojom::MediaRouteProviderId::CAST, kRouteId);
  EXPECT_CALL(mock_cast_provider_, DetachRoute(kRouteId));
  router()->DetachRoute(kRouteId);
  base::RunLoop().RunUntilIdle();
}

void MediaRouterMojoTest::RegisterMediaRouteProvider(
    mojom::MediaRouteProvider* provider,
    mojom::MediaRouteProviderId provider_id) {
  mojo::PendingRemote<mojom::MediaRouteProvider> mojo_provider;
  provider_receivers_.Add(provider,
                          mojo_provider.InitWithNewPipeAndPassReceiver());
  media_router_->RegisterMediaRouteProvider(provider_id,
                                            std::move(mojo_provider));
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
