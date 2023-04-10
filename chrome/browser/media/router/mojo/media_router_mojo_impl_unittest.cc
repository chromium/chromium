// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_mojo_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/string_escape.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_metrics.h"
#include "chrome/browser/media/router/test/media_router_mojo_test.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/media_router/browser/presentation_connection_message_observer.h"
#include "components/media_router/browser/route_message_util.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/common/issue.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/test/test_helper.h"
#include "components/version_info/version_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using blink::mojom::PresentationConnectionCloseReason;
using blink::mojom::PresentationConnectionState;
using media_router::mojom::RouteMessagePtr;
using testing::_;
using testing::AtMost;
using testing::Eq;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::IsEmpty;
using testing::Mock;
using testing::NiceMock;
using testing::Not;
using testing::Pointee;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;
using testing::Sequence;
using testing::SizeIs;
using testing::StrictMock;
using testing::UnorderedElementsAre;
using testing::Unused;
using testing::WithArg;

namespace media_router {

namespace {

const char kDescription[] = "description";
const char kError[] = "error";
const char kSource[] = "source1";
const char kSource2[] = "source2";
const char kTabSourceOne[] = "urn:x-org.chromium.media:source:tab:1";
const char kTabSourceTwo[] = "urn:x-org.chromium.media:source:tab:2";
const char kRouteId[] = "routeId";
const char kRouteId2[] = "routeId2";
const char kSinkId[] = "sink";
const char kSinkId2[] = "sink2";
const char kSinkName[] = "sinkName";
const char kPresentationId[] = "presentationId";
const char kOrigin[] = "http://origin/";
const int kInvalidFrameNodeId = -1;
const int kTimeoutMillis = 5 * 1000;

IssueInfo CreateIssueInfo(const std::string& title) {
  IssueInfo issue_info;
  issue_info.title = title;
  issue_info.message = std::string("msg");
  issue_info.severity = IssueInfo::Severity::WARNING;
  return issue_info;
}

// Creates a media route whose ID is |kRouteId|.
MediaRoute CreateMediaRoute() {
  MediaRoute route(kRouteId, MediaSource(kSource), kSinkId, kDescription, true);
  route.set_presentation_id(kPresentationId);
  route.set_controller_type(RouteControllerType::kGeneric);
  return route;
}

// Creates a media route whose ID is |kRouteId2|.
MediaRoute CreateMediaRoute2() {
  MediaRoute route(kRouteId2, MediaSource(kSource), kSinkId, kDescription,
                   true);
  route.set_presentation_id(kPresentationId);
  route.set_controller_type(RouteControllerType::kGeneric);
  return route;
}

std::string RouteMessageToString(const RouteMessagePtr& message) {
  if (!message->message && !message->data)
    return "null";
  std::string result;
  if (message->message) {
    result = "text=";
    base::EscapeJSONString(message->message.value(), true, &result);
  } else {
    const base::StringPiece src(
        reinterpret_cast<const char*>(message->data.value().data()),
        message->data.value().size());
    base::Base64Encode(src, &result);
    result = "binary=" + result;
  }
  return result;
}

std::vector<MediaSinkInternal> ToInternalSinks(
    const std::vector<MediaSink>& sinks) {
  std::vector<MediaSinkInternal> internal_sinks;
  internal_sinks.reserve(sinks.size());
  for (const auto& sink : sinks) {
    MediaSinkInternal internal_sink;
    internal_sink.set_sink(sink);
    internal_sinks.emplace_back(std::move(internal_sink));
  }
  return internal_sinks;
}

class StubMediaRouterMojoImpl : public MediaRouterMojoImpl {
 public:
  explicit StubMediaRouterMojoImpl(content::BrowserContext* context)
      : MediaRouterMojoImpl(context) {}
  ~StubMediaRouterMojoImpl() override = default;

  // media_router::MediaRouter:
  base::Value::Dict GetState() const override {
    NOTIMPLEMENTED();
    return base::Value::Dict();
  }

  void GetProviderState(
      mojom::MediaRouteProviderId provider_id,
      mojom::MediaRouteProvider::GetStateCallback callback) const override {
    NOTIMPLEMENTED();
  }
};

}  // namespace

class MediaRouterMojoImplTest : public MediaRouterMojoTest {
 public:
  MediaRouterMojoImplTest() = default;
  ~MediaRouterMojoImplTest() override = default;

 protected:
  void ExpectCastResultBucketCount(const std::string& operation,
                                   mojom::RouteRequestResultCode result_code,
                                   int expected_count) {
    ExpectBucketCount("MediaRouter.Provider." + operation + ".Result.Cast",
                      result_code, expected_count);
  }

  void ExpectResultBucketCount(const std::string& operation,
                               mojom::RouteRequestResultCode result_code,
                               int expected_count) {
    ExpectBucketCount("MediaRouter.Provider." + operation + ".Result",
                      result_code, expected_count);
  }

  std::unique_ptr<MediaRouterMojoImpl> CreateMediaRouter() override {
    return std::unique_ptr<MediaRouterMojoImpl>(
        new StubMediaRouterMojoImpl(profile()));
  }

  void ReceiveSinks(mojom::MediaRouteProviderId provider_id,
                    const std::string& media_source,
                    const std::vector<MediaSinkInternal>& sinks) {
    router()->OnSinksReceived(
        provider_id, media_source, sinks,
        std::vector<url::Origin>{1, url::Origin::Create(GURL(kOrigin))});
  }

  void ReceiveRouteMessages(const std::string& route_id,
                            std::vector<mojom::RouteMessagePtr> messages) {
    router()->OnRouteMessagesReceived(route_id, std::move(messages));
  }

  void RegisterMediaRoutesObserver(MediaRoutesObserver* observer) {
    router()->RegisterMediaRoutesObserver(observer);
  }

  void UnregisterMediaRoutesObserver(MediaRoutesObserver* observer) {
    router()->UnregisterMediaRoutesObserver(observer);
  }

  void UpdateRoutes(mojom::MediaRouteProviderId provider_id,
                    const std::vector<MediaRoute>& routes) {
    router()->OnRoutesUpdated(provider_id, routes);
  }

  void RecordPresentationRequestUrlBySink(
      const media_router::MediaSource& source,
      media_router::mojom::MediaRouteProviderId provider_id,
      int times) {
    for (int i = 0; i < times; i++) {
      MediaRouterMojoImpl::RecordPresentationRequestUrlBySink(source,
                                                              provider_id);
    }
  }

 private:
  void ExpectBucketCount(const std::string& histogram_name,
                         mojom::RouteRequestResultCode result_code,
                         int expected_count) {
    histogram_tester_.ExpectBucketCount(histogram_name, result_code,
                                        expected_count);
  }

  base::HistogramTester histogram_tester_;
};

TEST_F(MediaRouterMojoImplTest, CreateRoute) {
  TestCreateRoute();
  ExpectCastResultBucketCount("CreateRoute", mojom::RouteRequestResultCode::OK,
                              1);
}

// Tests that MediaRouter is aware when a route is created, even if
// MediaRouteProvider doesn't call OnRoutesUpdated().
TEST_F(MediaRouterMojoImplTest, RouteRecognizedAfterCreation) {
  MockMediaRoutesObserver routes_observer(router());

  EXPECT_CALL(routes_observer, OnRoutesUpdated(SizeIs(0)));
  EXPECT_CALL(routes_observer, OnRoutesUpdated(SizeIs(1)));

  // TestCreateRoute() does not explicitly call OnRoutesUpdated() on the router.
  TestCreateRoute();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaRouterMojoImplTest, CreateIncognitoRoute) {
  ProvideTestSink(mojom::MediaRouteProviderId::CAST, kSinkId);
  MediaSource media_source(kSource);
  MediaRoute expected_route(kRouteId, media_source, kSinkId, "", false);
  expected_route.set_off_the_record(true);

  // Use a lambda function as an invocation target here to work around
  // a limitation with GMock::Invoke that prevents it from using move-only types
  // in runnable parameter lists.
  EXPECT_CALL(mock_cast_provider_,
              CreateRouteInternal(kSource, kSinkId, _,
                                  url::Origin::Create(GURL(kOrigin)),
                                  kInvalidFrameNodeId, _, _, _))
      .WillOnce(WithArg<7>(
          Invoke([&expected_route](
                     mojom::MediaRouteProvider::CreateRouteCallback& cb) {
            std::move(cb).Run(expected_route, nullptr, std::string(),
                              mojom::RouteRequestResultCode::OK);
          })));

  base::RunLoop run_loop;
  RouteResponseCallbackHandler handler;
  EXPECT_CALL(handler, DoInvoke(Pointee(expected_route), Not(""), "",
                                mojom::RouteRequestResultCode::OK, _))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  router()->CreateRoute(kSource, kSinkId, url::Origin::Create(GURL(kOrigin)),
                        nullptr,
                        base::BindOnce(&RouteResponseCallbackHandler::Invoke,
                                       base::Unretained(&handler)),
                        base::Milliseconds(kTimeoutMillis), true);
  run_loop.Run();
  ExpectCastResultBucketCount("CreateRoute", mojom::RouteRequestResultCode::OK,
                              1);
}

TEST_F(MediaRouterMojoImplTest, CreateRouteFails) {
  ProvideTestSink(mojom::MediaRouteProviderId::CAST, kSinkId);
  EXPECT_CALL(mock_cast_provider_,
              CreateRouteInternal(kSource, kSinkId, _,
                                  url::Origin::Create(GURL(kOrigin)),
                                  kInvalidFrameNodeId, _, _, _))
      .WillOnce(WithArg<7>(
          Invoke([](mojom::MediaRouteProvider::CreateRouteCallback& cb) {
            std::move(cb).Run(absl::nullopt, nullptr, std::string(kError),
                              mojom::RouteRequestResultCode::TIMED_OUT);
          })));

  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  EXPECT_CALL(handler, DoInvoke(nullptr, "", kError,
                                mojom::RouteRequestResultCode::TIMED_OUT, _))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  router()->CreateRoute(kSource, kSinkId, url::Origin::Create(GURL(kOrigin)),
                        nullptr,
                        base::BindOnce(&RouteResponseCallbackHandler::Invoke,
                                       base::Unretained(&handler)),
                        base::Milliseconds(kTimeoutMillis), false);
  run_loop.Run();
  ExpectCastResultBucketCount("CreateRoute",
                              mojom::RouteRequestResultCode::TIMED_OUT, 1);
}

TEST_F(MediaRouterMojoImplTest, CreateRouteIncognitoMismatchFails) {
  ProvideTestSink(mojom::MediaRouteProviderId::CAST, kSinkId);
  EXPECT_CALL(mock_cast_provider_,
              CreateRouteInternal(kSource, kSinkId, _,
                                  url::Origin::Create(GURL(kOrigin)),
                                  kInvalidFrameNodeId, _, _, _))
      .WillOnce(WithArg<7>(
          Invoke([](mojom::MediaRouteProvider::CreateRouteCallback& cb) {
            std::move(cb).Run(CreateMediaRoute(), nullptr, std::string(),
                              mojom::RouteRequestResultCode::OK);
          })));

  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  std::string error(
      "Mismatch in OffTheRecord status: request = 1, response = 0");
  EXPECT_CALL(handler,
              DoInvoke(nullptr, "", error,
                       mojom::RouteRequestResultCode::ROUTE_NOT_FOUND, _))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  router()->CreateRoute(kSource, kSinkId, url::Origin::Create(GURL(kOrigin)),
                        nullptr,
                        base::BindOnce(&RouteResponseCallbackHandler::Invoke,
                                       base::Unretained(&handler)),
                        base::Milliseconds(kTimeoutMillis), true);
  run_loop.Run();
  ExpectCastResultBucketCount(
      "CreateRoute", mojom::RouteRequestResultCode::ROUTE_NOT_FOUND, 1);
}

TEST_F(MediaRouterMojoImplTest, JoinRoute) {
  TestJoinRoute(kPresentationId);
  ExpectCastResultBucketCount("JoinRoute", mojom::RouteRequestResultCode::OK,
                              1);
}

TEST_F(MediaRouterMojoImplTest, JoinRouteNotFoundFails) {
  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  EXPECT_CALL(handler,
              DoInvoke(nullptr, "", "Route not found",
                       mojom::RouteRequestResultCode::ROUTE_NOT_FOUND, _))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  router()->JoinRoute(kSource, kPresentationId,
                      url::Origin::Create(GURL(kOrigin)), nullptr,
                      base::BindOnce(&RouteResponseCallbackHandler::Invoke,
                                     base::Unretained(&handler)),
                      base::Milliseconds(kTimeoutMillis), false);
  run_loop.Run();
  ExpectResultBucketCount("JoinRoute",
                          mojom::RouteRequestResultCode::ROUTE_NOT_FOUND, 1);
}

TEST_F(MediaRouterMojoImplTest, JoinRouteTimedOutFails) {
  // Make sure the MR has received an update with the route, so it knows there
  // is a route to join.
  const std::vector<MediaRoute> routes{CreateMediaRoute()};
  UpdateRoutes(mojom::MediaRouteProviderId::CAST, routes);
  EXPECT_TRUE(router()->HasJoinableRoute());

  EXPECT_CALL(
      mock_cast_provider_,
      JoinRouteInternal(kSource, kPresentationId,
                        url::Origin::Create(GURL(kOrigin)), kInvalidFrameNodeId,
                        base::Milliseconds(kTimeoutMillis), _, _))
      .WillOnce(WithArg<6>(
          Invoke([](mojom::MediaRouteProvider::JoinRouteCallback& cb) {
            std::move(cb).Run(absl::nullopt, nullptr, std::string(kError),
                              mojom::RouteRequestResultCode::TIMED_OUT);
          })));

  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  EXPECT_CALL(handler, DoInvoke(nullptr, "", kError,
                                mojom::RouteRequestResultCode::TIMED_OUT, _))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  router()->JoinRoute(kSource, kPresentationId,
                      url::Origin::Create(GURL(kOrigin)), nullptr,
                      base::BindOnce(&RouteResponseCallbackHandler::Invoke,
                                     base::Unretained(&handler)),
                      base::Milliseconds(kTimeoutMillis), false);
  run_loop.Run();
  ExpectCastResultBucketCount("JoinRoute",
                              mojom::RouteRequestResultCode::TIMED_OUT, 1);
}

TEST_F(MediaRouterMojoImplTest, JoinRouteIncognitoMismatchFails) {
  const MediaRoute route = CreateMediaRoute();

  // Make sure the MR has received an update with the route, so it knows there
  // is a route to join.
  const std::vector<MediaRoute> routes{route};
  UpdateRoutes(mojom::MediaRouteProviderId::CAST, routes);
  EXPECT_TRUE(router()->HasJoinableRoute());

  // Use a lambda function as an invocation target here to work around
  // a limitation with GMock::Invoke that prevents it from using move-only types
  // in runnable parameter lists.
  EXPECT_CALL(
      mock_cast_provider_,
      JoinRouteInternal(kSource, kPresentationId,
                        url::Origin::Create(GURL(kOrigin)), kInvalidFrameNodeId,
                        base::Milliseconds(kTimeoutMillis), _, _))
      .WillOnce(WithArg<6>(
          Invoke([&route](mojom::MediaRouteProvider::JoinRouteCallback& cb) {
            std::move(cb).Run(route, nullptr, std::string(),
                              mojom::RouteRequestResultCode::OK);
          })));

  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  std::string error(
      "Mismatch in OffTheRecord status: request = 1, response = 0");
  EXPECT_CALL(handler,
              DoInvoke(nullptr, "", error,
                       mojom::RouteRequestResultCode::ROUTE_NOT_FOUND, _))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  router()->JoinRoute(kSource, kPresentationId,
                      url::Origin::Create(GURL(kOrigin)), nullptr,
                      base::BindOnce(&RouteResponseCallbackHandler::Invoke,
                                     base::Unretained(&handler)),
                      base::Milliseconds(kTimeoutMillis), true);
  run_loop.Run();
  ExpectCastResultBucketCount(
      "JoinRoute", mojom::RouteRequestResultCode::ROUTE_NOT_FOUND, 1);
}

TEST_F(MediaRouterMojoImplTest, DetachRoute) {
  TestDetachRoute();
}

TEST_F(MediaRouterMojoImplTest, TerminateRoute) {
  TestTerminateRoute();
  ExpectCastResultBucketCount("TerminateRoute",
                              mojom::RouteRequestResultCode::OK, 1);
}

TEST_F(MediaRouterMojoImplTest, TerminateRouteFails) {
  ProvideTestRoute(mojom::MediaRouteProviderId::CAST, kRouteId);
  EXPECT_CALL(mock_cast_provider_, TerminateRouteInternal(kRouteId, _))
      .WillOnce(
          Invoke([](const std::string& route_id,
                    mojom::MediaRouteProvider::TerminateRouteCallback& cb) {
            std::move(cb).Run(std::string("timed out"),
                              mojom::RouteRequestResultCode::TIMED_OUT);
          }));
  router()->TerminateRoute(kRouteId);
  base::RunLoop().RunUntilIdle();
  ExpectCastResultBucketCount("TerminateRoute",
                              mojom::RouteRequestResultCode::OK, 0);
  ExpectCastResultBucketCount("TerminateRoute",
                              mojom::RouteRequestResultCode::TIMED_OUT, 1);
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(MediaRouterMojoImplTest, HandleIssue) {
  MockIssuesObserver issue_observer1(router()->GetIssueManager());
  MockIssuesObserver issue_observer2(router()->GetIssueManager());
  issue_observer1.Init();
  issue_observer2.Init();

  IssueInfo issue_info = CreateIssueInfo("title 1");

  Issue issue_from_observer1((IssueInfo()));
  Issue issue_from_observer2((IssueInfo()));
  EXPECT_CALL(issue_observer1, OnIssue(_))
      .WillOnce(SaveArg<0>(&issue_from_observer1));
  EXPECT_CALL(issue_observer2, OnIssue(_))
      .WillOnce(SaveArg<0>(&issue_from_observer2));

  router()->OnIssue(issue_info);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(issue_from_observer1.id(), issue_from_observer2.id());
  EXPECT_EQ(issue_info, issue_from_observer1.info());
  EXPECT_EQ(issue_info, issue_from_observer2.info());
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(MediaRouterMojoImplTest, RegisterAndUnregisterMediaSinksObserver) {
  MediaSource media_source(kSource);

  // These should only be called once even if there is more than one observer
  // for a given source.
  EXPECT_CALL(mock_cast_provider_, StartObservingMediaSinks(kSource));
  EXPECT_CALL(mock_cast_provider_, StartObservingMediaSinks(kSource2));

  auto sinks_observer = std::make_unique<NiceMock<MockMediaSinksObserver>>(
      router(), media_source, url::Origin::Create(GURL(kOrigin)));
  EXPECT_TRUE(sinks_observer->Init());
  auto extra_sinks_observer =
      std::make_unique<NiceMock<MockMediaSinksObserver>>(
          router(), media_source, url::Origin::Create(GURL(kOrigin)));
  EXPECT_TRUE(extra_sinks_observer->Init());
  auto unrelated_sinks_observer =
      std::make_unique<NiceMock<MockMediaSinksObserver>>(
          router(), MediaSource(kSource2), url::Origin::Create(GURL(kOrigin)));
  EXPECT_TRUE(unrelated_sinks_observer->Init());
  base::RunLoop().RunUntilIdle();

  const std::vector<MediaSink> kExpectedSinks{
      CreateCastSink(kSinkId, kSinkName), CreateCastSink(kSinkId2, kSinkName)};

  EXPECT_CALL(*sinks_observer, OnSinksReceived(kExpectedSinks));
  EXPECT_CALL(*extra_sinks_observer, OnSinksReceived(kExpectedSinks));
  ReceiveSinks(mojom::MediaRouteProviderId::CAST, media_source.id(),
               ToInternalSinks(kExpectedSinks));

  // Since the MediaRouterMojoImpl has already received results for
  // |media_source|, return cached results to observers that are subsequently
  // registered.
  auto cached_sinks_observer = std::make_unique<MockMediaSinksObserver>(
      router(), media_source, url::Origin::Create(GURL(kOrigin)));
  EXPECT_CALL(*cached_sinks_observer, OnSinksReceived(kExpectedSinks));
  EXPECT_TRUE(cached_sinks_observer->Init());

  // Different origin from cached result. Empty list will be returned.
  auto cached_sinks_observer2 = std::make_unique<MockMediaSinksObserver>(
      router(), media_source, url::Origin::Create(GURL("https://youtube.com")));
  EXPECT_CALL(*cached_sinks_observer2, OnSinksReceived(IsEmpty()));
  EXPECT_TRUE(cached_sinks_observer2->Init());

  EXPECT_CALL(mock_cast_provider_, StopObservingMediaSinks(kSource));
  EXPECT_CALL(mock_cast_provider_, StopObservingMediaSinks(kSource2));
  sinks_observer.reset();
  extra_sinks_observer.reset();
  unrelated_sinks_observer.reset();
  cached_sinks_observer.reset();
  cached_sinks_observer2.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaRouterMojoImplTest, TabSinksObserverIsShared) {
  MediaSource tab_source_one(kTabSourceOne);
  MediaSource tab_source_two(kTabSourceTwo);

  // Media router should not try to start observing for each tab, but instead
  // only once for all tabs.
  EXPECT_CALL(mock_cast_provider_, StartObservingMediaSinks(kTabSourceOne))
      .Times(0);
  EXPECT_CALL(mock_cast_provider_, StartObservingMediaSinks(kTabSourceTwo))
      .Times(0);
  EXPECT_CALL(mock_cast_provider_,
              StartObservingMediaSinks(MediaSource::ForAnyTab().id()))
      .Times(1);

  const auto origin = url::Origin::Create(GURL(kOrigin));
  auto sinks_observer = std::make_unique<NiceMock<MockMediaSinksObserver>>(
      router(), tab_source_one, origin);
  EXPECT_TRUE(sinks_observer->Init());
  auto extra_sinks_observer =
      std::make_unique<NiceMock<MockMediaSinksObserver>>(
          router(), tab_source_one, origin);
  EXPECT_TRUE(extra_sinks_observer->Init());
  auto second_tab_sinks_observer =
      std::make_unique<NiceMock<MockMediaSinksObserver>>(
          router(), MediaSource(kTabSourceTwo), origin);
  EXPECT_TRUE(second_tab_sinks_observer->Init());
  base::RunLoop().RunUntilIdle();

  const std::vector<MediaSink> kExpectedSinks{
      CreateCastSink(kSinkId, kSinkName), CreateCastSink(kSinkId2, kSinkName)};

  // All tabs should get the same updates.
  EXPECT_CALL(*sinks_observer, OnSinksReceived(kExpectedSinks));
  EXPECT_CALL(*extra_sinks_observer, OnSinksReceived(kExpectedSinks));
  EXPECT_CALL(*second_tab_sinks_observer, OnSinksReceived(kExpectedSinks));
  ReceiveSinks(mojom::MediaRouteProviderId::CAST, tab_source_one.id(),
               ToInternalSinks(kExpectedSinks));

  // Since the MediaRouterMojoImpl has already received results for
  // |media_source|, return cached results to observers that are subsequently
  // registered.
  auto cached_sinks_observer = std::make_unique<MockMediaSinksObserver>(
      router(), tab_source_one, url::Origin::Create(GURL(kOrigin)));
  EXPECT_CALL(*cached_sinks_observer, OnSinksReceived(kExpectedSinks));
  EXPECT_TRUE(cached_sinks_observer->Init());

  // Different origin from cached result. Empty list will be returned.
  auto cached_sinks_observer2 = std::make_unique<MockMediaSinksObserver>(
      router(), tab_source_one,
      url::Origin::Create(GURL("https://youtube.com")));
  EXPECT_CALL(*cached_sinks_observer2, OnSinksReceived(IsEmpty()));
  EXPECT_TRUE(cached_sinks_observer2->Init());

  // Since tabs share observation, stop observing should not be called.
  EXPECT_CALL(mock_cast_provider_, StopObservingMediaSinks(kTabSourceOne))
      .Times(0);
  EXPECT_CALL(mock_cast_provider_, StopObservingMediaSinks(kTabSourceTwo))
      .Times(0);
  EXPECT_CALL(mock_cast_provider_,
              StopObservingMediaSinks(MediaSource::ForAnyTab().id()))
      .Times(0);
  sinks_observer.reset();
  extra_sinks_observer.reset();
  second_tab_sinks_observer.reset();
  cached_sinks_observer.reset();
  cached_sinks_observer2.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaRouterMojoImplTest, RegisterAndUnregisterMediaRoutesObserver) {
  MockMediaRouter mock_router;

  auto routes_observer =
      std::make_unique<MockMediaRoutesObserver>(&mock_router);
  EXPECT_TRUE(
      mock_router.routes_observers().HasObserver(routes_observer.get()));

  auto extra_routes_observer =
      std::make_unique<MockMediaRoutesObserver>(&mock_router);
  EXPECT_TRUE(
      mock_router.routes_observers().HasObserver(extra_routes_observer.get()));

  routes_observer.reset();
  extra_routes_observer.reset();
  EXPECT_TRUE(mock_router.routes_observers().empty());
}

TEST_F(MediaRouterMojoImplTest, UnregisterBeforeNotificationDoesntCrash) {
  auto routes_observer = std::make_unique<MediaRoutesObserver>(router());
  auto routes_observer_two = std::make_unique<MediaRoutesObserver>(router());

  // Resetting the observer immediately should cause it to be invalidated before
  // the callback for NotifyOfExistingRoutesIfRegistered is called.
  routes_observer_two.reset();

  // Make sure the Notify task executes.
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaRouterMojoImplTest, RegisteredObserversGetMediaRouteUpdates) {
  auto routes_observer =
      std::make_unique<StrictMock<MockMediaRoutesObserver>>(router());
  auto extra_routes_observer =
      std::make_unique<StrictMock<MockMediaRoutesObserver>>(router());

  MediaSource media_source(kSource);
  std::vector<MediaRoute> expected_routes{
      MediaRoute(kRouteId, media_source, kSinkId, kDescription, false)};

  MediaRoute incognito_expected_route(kRouteId2, media_source, kSinkId,
                                      kDescription, false);
  incognito_expected_route.set_off_the_record(true);
  expected_routes.push_back(incognito_expected_route);

  EXPECT_CALL(*routes_observer, OnRoutesUpdated(expected_routes)).Times(1);
  EXPECT_CALL(*extra_routes_observer, OnRoutesUpdated(expected_routes))
      .Times(1);

  UpdateRoutes(mojom::MediaRouteProviderId::CAST, expected_routes);

  routes_observer.reset();
  extra_routes_observer.reset();

  // No route observers should be notified.
  UpdateRoutes(mojom::MediaRouteProviderId::CAST, expected_routes);
}

TEST_F(MediaRouterMojoImplTest, SendRouteMessage) {
  TestSendRouteMessage();
}

TEST_F(MediaRouterMojoImplTest, SendRouteBinaryMessage) {
  TestSendRouteBinaryMessage();
}

namespace {

// Used in the RouteMessages* tests to populate the messages that will be
// processed and dispatched to PresentationConnectionMessageObservers.
void PopulateRouteMessages(std::vector<RouteMessagePtr>* batch1,
                           std::vector<RouteMessagePtr>* batch2,
                           std::vector<RouteMessagePtr>* batch3,
                           std::vector<RouteMessagePtr>* all_messages) {
  batch1->clear();
  batch2->clear();
  batch3->clear();
  batch1->emplace_back(message_util::RouteMessageFromString("text1"));
  batch2->emplace_back(
      message_util::RouteMessageFromData(std::vector<uint8_t>({UINT8_C(1)})));
  batch2->emplace_back(message_util::RouteMessageFromString("text2"));
  batch3->emplace_back(message_util::RouteMessageFromString("text3"));
  batch3->emplace_back(
      message_util::RouteMessageFromData(std::vector<uint8_t>({UINT8_C(2)})));
  batch3->emplace_back(
      message_util::RouteMessageFromData(std::vector<uint8_t>({UINT8_C(3)})));
  all_messages->clear();
  for (auto* message_batch : {batch1, batch2, batch3}) {
    for (auto& message : *message_batch) {
      all_messages->emplace_back(message->Clone());
    }
  }
}

// Used in the RouteMessages* tests to observe and sanity-check that the
// messages being received from the router are correct and in-sequence. The
// checks here correspond to the expected messages in PopulateRouteMessages()
// above.
class ExpectedMessagesObserver final
    : public PresentationConnectionMessageObserver {
 public:
  ExpectedMessagesObserver(MediaRouter* router,
                           const MediaRoute::Id& route_id,
                           std::vector<RouteMessagePtr> expected_messages)
      : PresentationConnectionMessageObserver(router, route_id),
        expected_messages_(std::move(expected_messages)) {}

  ~ExpectedMessagesObserver() override { CheckReceivedMessages(); }

 private:
  void OnMessagesReceived(std::vector<RouteMessagePtr> messages) override {
    for (auto& message : messages)
      messages_.emplace_back(std::move(message));
  }

  void CheckReceivedMessages() {
    ASSERT_EQ(expected_messages_.size(), messages_.size());
    for (size_t i = 0; i < expected_messages_.size(); i++) {
      EXPECT_EQ(expected_messages_[i], messages_[i])
          << "Message mismatch at index " << i
          << ": expected: " << RouteMessageToString(expected_messages_[i])
          << ", actual: " << RouteMessageToString(messages_[i]);
    }
  }

  std::vector<RouteMessagePtr> expected_messages_;
  std::vector<RouteMessagePtr> messages_;
};

}  // namespace

TEST_F(MediaRouterMojoImplTest, RouteMessagesSingleObserver) {
  std::vector<RouteMessagePtr> incoming_batch1, incoming_batch2,
      incoming_batch3, all_messages;
  ProvideTestRoute(mojom::MediaRouteProviderId::CAST, kRouteId);
  PopulateRouteMessages(&incoming_batch1, &incoming_batch2, &incoming_batch3,
                        &all_messages);

  base::RunLoop run_loop;
  EXPECT_CALL(mock_cast_provider_, StartListeningForRouteMessages(Eq(kRouteId)))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  // Creating ExpectedMessagesObserver will register itself to the
  // MediaRouter, which in turn will start listening for route messages.
  ExpectedMessagesObserver observer(router(), kRouteId,
                                    std::move(all_messages));
  run_loop.Run();  // Will quit when StartListeningForRouteMessages() is called.
  ReceiveRouteMessages(kRouteId, std::move(incoming_batch1));
  ReceiveRouteMessages(kRouteId, std::move(incoming_batch2));
  ReceiveRouteMessages(kRouteId, std::move(incoming_batch3));
  // When |observer| goes out-of-scope, its destructor will ensure all expected
  // messages have been received.
}

TEST_F(MediaRouterMojoImplTest, RouteMessagesMultipleObservers) {
  std::vector<RouteMessagePtr> incoming_batch1, incoming_batch2,
      incoming_batch3, all_messages;
  ProvideTestRoute(mojom::MediaRouteProviderId::CAST, kRouteId);
  PopulateRouteMessages(&incoming_batch1, &incoming_batch2, &incoming_batch3,
                        &all_messages);
  std::vector<RouteMessagePtr> all_messages2;
  for (auto& message : all_messages)
    all_messages2.emplace_back(message->Clone());

  base::RunLoop run_loop;
  EXPECT_CALL(mock_cast_provider_, StartListeningForRouteMessages(Eq(kRouteId)))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  // The ExpectedMessagesObservers will register themselves with the
  // MediaRouter, which in turn will start listening for route messages.
  ExpectedMessagesObserver observer1(router(), kRouteId,
                                     std::move(all_messages));
  ExpectedMessagesObserver observer2(router(), kRouteId,
                                     std::move(all_messages2));
  run_loop.Run();  // Will quit when StartListeningForRouteMessages() is called.
  ReceiveRouteMessages(kRouteId, std::move(incoming_batch1));
  ReceiveRouteMessages(kRouteId, std::move(incoming_batch2));
  ReceiveRouteMessages(kRouteId, std::move(incoming_batch3));
  // As each |observer| goes out-of-scope, its destructor will ensure all
  // expected messages have been received.
}

TEST_F(MediaRouterMojoImplTest, PresentationConnectionStateChangedCallback) {
  MediaRoute::Id route_id("route-id");
  const GURL presentation_url("http://www.example.com/presentation.html");
  const std::string presentation_id("pid");
  blink::mojom::PresentationInfo connection(presentation_url, presentation_id);
  base::MockCallback<content::PresentationConnectionStateChangedCallback>
      callback;
  base::CallbackListSubscription subscription =
      router()->AddPresentationConnectionStateChangedCallback(route_id,
                                                              callback.Get());

  {
    base::RunLoop run_loop;
    content::PresentationConnectionStateChangeInfo closed_info(
        PresentationConnectionState::CLOSED);
    closed_info.close_reason = PresentationConnectionCloseReason::WENT_AWAY;
    closed_info.message = "Foo";

    EXPECT_CALL(callback, Run(StateChangeInfoEquals(closed_info)))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    router()->OnPresentationConnectionClosed(
        route_id, PresentationConnectionCloseReason::WENT_AWAY, "Foo");
    run_loop.Run();
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&callback));
  }

  content::PresentationConnectionStateChangeInfo terminated_info(
      PresentationConnectionState::TERMINATED);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(callback, Run(StateChangeInfoEquals(terminated_info)))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    router()->OnPresentationConnectionStateChanged(
        route_id, PresentationConnectionState::TERMINATED);
    run_loop.Run();

    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&callback));
  }
}

TEST_F(MediaRouterMojoImplTest,
       PresentationConnectionStateChangedCallbackRemoved) {
  MediaRoute::Id route_id("route-id");
  base::MockCallback<content::PresentationConnectionStateChangedCallback>
      callback;
  base::CallbackListSubscription subscription =
      router()->AddPresentationConnectionStateChangedCallback(route_id,
                                                              callback.Get());

  // Callback has been removed, so we don't expect it to be called anymore.
  subscription = {};
  EXPECT_TRUE(router()->presentation_connection_state_callbacks_.empty());

  EXPECT_CALL(callback, Run(_)).Times(0);
  router()->OnPresentationConnectionStateChanged(
      route_id, PresentationConnectionState::TERMINATED);
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaRouterMojoImplTest, GetMediaController) {
  MockMediaController mock_controller;
  mojo::Remote<mojom::MediaController> controller_remote;
  mojo::PendingRemote<mojom::MediaStatusObserver> observer_remote;
  MockMediaStatusObserver mock_observer(
      observer_remote.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::MediaStatusObserver> observer_remote_held_by_controller;
  UpdateRoutes(mojom::MediaRouteProviderId::CAST, {CreateMediaRoute()});

  EXPECT_CALL(mock_cast_provider_,
              CreateMediaRouteControllerInternal(kRouteId, _, _, _))
      .WillOnce(
          [&](const std::string& route_id,
              mojo::PendingReceiver<mojom::MediaController>& media_controller,
              mojo::PendingRemote<mojom::MediaStatusObserver>& observer,
              MockMediaRouteProvider::CreateMediaRouteControllerCallback&
                  callback) {
            mock_controller.Bind(std::move(media_controller));
            observer_remote_held_by_controller.Bind(std::move(observer));
            std::move(callback).Run(true);
          });
  router()->GetMediaController(kRouteId,
                               controller_remote.BindNewPipeAndPassReceiver(),
                               std::move(observer_remote));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_controller, Play());
  controller_remote->Play();
  EXPECT_CALL(mock_observer, OnMediaStatusUpdated(_));
  observer_remote_held_by_controller->OnMediaStatusUpdated(
      mojom::MediaStatus::New());
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaRouterMojoImplTest, SendSinkRequestsToMultipleProviders) {
  // Register |mock_wired_display_provider_| with the router.
  // |mock_cast_provider_| has already been registered during setup.
  RegisterWiredDisplayProvider();

  // Set |mock_cast_provider_| to have one sink with ID |kSinkId|.
  ProvideTestSink(mojom::MediaRouteProviderId::CAST, kSinkId);
  mock_cast_provider_.SetRouteToReturn(CreateMediaRoute());

  // Set |mock_wired_display_provider_| to have one sink with ID |kSinkId2|.
  ProvideTestSink(mojom::MediaRouteProviderId::WIRED_DISPLAY, kSinkId2);
  mock_wired_display_provider_.SetRouteToReturn(CreateMediaRoute2());

  // A request to |kSinkId| should only be sent to |mock_cast_provider_|.
  EXPECT_CALL(mock_cast_provider_,
              CreateRouteInternal(_, kSinkId, _, _, _, _, _, _))
      .WillOnce(WithArg<7>(Invoke(
          &mock_cast_provider_, &MockMediaRouteProvider::RouteRequestSuccess)));
  EXPECT_CALL(mock_wired_display_provider_,
              CreateRouteInternal(_, kSinkId, _, _, _, _, _, _))
      .Times(0);
  router()->CreateRoute(kSource, kSinkId, url::Origin::Create(GURL(kOrigin)),
                        nullptr, base::DoNothing(),
                        base::Milliseconds(kTimeoutMillis), true);

  // A request to |kSinkId2| should only be sent to
  // |mock_wired_display_provider_|.
  EXPECT_CALL(mock_cast_provider_,
              CreateRouteInternal(_, kSinkId2, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(mock_wired_display_provider_,
              CreateRouteInternal(_, kSinkId2, _, _, _, _, _, _))
      .WillOnce(
          WithArg<7>(Invoke(&mock_wired_display_provider_,
                            &MockMediaRouteProvider::RouteRequestSuccess)));
  router()->CreateRoute(kSource, kSinkId2, url::Origin::Create(GURL(kOrigin)),
                        nullptr, base::DoNothing(),
                        base::Milliseconds(kTimeoutMillis), true);
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaRouterMojoImplTest, SendRouteRequestsToMultipleProviders) {
  // Register |mock_wired_display_provider_| with the router.
  // |mock_cast_provider_| has already been registered during setup.
  RegisterWiredDisplayProvider();

  // Set |mock_cast_provider_| to have one route with ID |kRouteId|.
  ProvideTestRoute(mojom::MediaRouteProviderId::CAST, kRouteId);

  // Set |mock_wired_display_provider_| to have one route with ID |kRouteId2|.
  ProvideTestRoute(mojom::MediaRouteProviderId::WIRED_DISPLAY, kRouteId2);

  // A request for |kRouteId| should only be sent to |mock_cast_provider_|.
  EXPECT_CALL(mock_cast_provider_, DetachRoute(kRouteId));
  EXPECT_CALL(mock_wired_display_provider_, DetachRoute(kRouteId)).Times(0);
  router()->DetachRoute(kRouteId);

  // A request for |kRouteId2| should only be sent to
  // |mock_wired_display_provider_|.
  EXPECT_CALL(mock_cast_provider_, TerminateRouteInternal(kRouteId2, _))
      .Times(0);
  EXPECT_CALL(mock_wired_display_provider_,
              TerminateRouteInternal(kRouteId2, _))
      .WillOnce(
          WithArg<1>(Invoke(&mock_wired_display_provider_,
                            &MockMediaRouteProvider::TerminateRouteSuccess)));
  router()->TerminateRoute(kRouteId2);
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaRouterMojoImplTest, ObserveSinksFromMultipleProviders) {
  const MediaSource source(kSource);
  // Sinks for the Cast MRP.
  MediaSinkInternal sink1a;
  MediaSinkInternal sink1b;
  sink1a.set_sink(MediaSink("sink1a", "", SinkIconType::CAST,
                            mojom::MediaRouteProviderId::CAST));
  sink1b.set_sink(MediaSink("sink1b", "", SinkIconType::CAST,
                            mojom::MediaRouteProviderId::CAST));
  // Sinks for the wired display MRP.
  MediaSinkInternal sink2a;
  MediaSinkInternal sink2b;
  sink2a.set_sink(MediaSink("sink2a", "", SinkIconType::CAST,
                            mojom::MediaRouteProviderId::WIRED_DISPLAY));
  sink2b.set_sink(MediaSink("sink2b", "", SinkIconType::CAST,
                            mojom::MediaRouteProviderId::WIRED_DISPLAY));
  RegisterWiredDisplayProvider();
  NiceMock<MockMediaSinksObserver> observer(router(), source,
                                            url::Origin::Create(GURL(kOrigin)));
  observer.Init();

  // Have the Cast MRP report sinks.
  EXPECT_CALL(observer, OnSinksReceived(UnorderedElementsAre(sink1a.sink(),
                                                             sink1b.sink())));
  ReceiveSinks(mojom::MediaRouteProviderId::CAST, kSource, {sink1a, sink1b});

  // Have the wired display MRP report sinks.
  EXPECT_CALL(observer,
              OnSinksReceived(UnorderedElementsAre(
                  sink1a.sink(), sink1b.sink(), sink2a.sink(), sink2b.sink())));
  ReceiveSinks(mojom::MediaRouteProviderId::WIRED_DISPLAY, kSource,
               {sink2a, sink2b});

  // Have the Cast MRP report an empty list of sinks.
  EXPECT_CALL(observer, OnSinksReceived(UnorderedElementsAre(sink2a.sink(),
                                                             sink2b.sink())));
  ReceiveSinks(mojom::MediaRouteProviderId::CAST, kSource, {});

  // Have the wired display MRP report an empty list of sinks.
  EXPECT_CALL(observer, OnSinksReceived(IsEmpty()));
  ReceiveSinks(mojom::MediaRouteProviderId::WIRED_DISPLAY, kSource, {});
}

TEST_F(MediaRouterMojoImplTest, ObserveRoutesFromMultipleProviders) {
  const MediaSource source(kSource);
  // Routes for the Cast MRP.
  const MediaRoute route1a("route1a", source, "sink 1a", "", true);
  const MediaRoute route1b("route1b", source, "sink 1b", "", true);
  // Routes for the wired display MRP.
  const MediaRoute route2a("route2a", source, "sink 2a", "", true);
  const MediaRoute route2b("route2b", source, "sink 2b", "", true);
  RegisterWiredDisplayProvider();
  MockMediaRoutesObserver observer(router());

  // Have the Cast MRP report routes.
  EXPECT_CALL(observer,
              OnRoutesUpdated(UnorderedElementsAre(route1a, route1b)));
  UpdateRoutes(mojom::MediaRouteProviderId::CAST, {route1a, route1b});

  // Have the wired display MRP report routes.
  EXPECT_CALL(observer, OnRoutesUpdated(UnorderedElementsAre(
                            route1a, route1b, route2a, route2b)));
  UpdateRoutes(mojom::MediaRouteProviderId::WIRED_DISPLAY, {route2a, route2b});

  // Have the Cast MRP report an empty list of routes.
  EXPECT_CALL(observer,
              OnRoutesUpdated(UnorderedElementsAre(route2a, route2b)));
  UpdateRoutes(mojom::MediaRouteProviderId::CAST, {});

  // Have the wired display MRP report an empty list of routes.
  EXPECT_CALL(observer, OnRoutesUpdated(IsEmpty()));
  UpdateRoutes(mojom::MediaRouteProviderId::WIRED_DISPLAY, {});
}

TEST_F(MediaRouterMojoImplTest, TestRecordPresentationRequestUrlBySink) {
  using base::Bucket;
  using PresentationUrlBySink = MediaRouterMojoImpl::PresentationUrlBySink;

  MediaSource cast_source("cast:ABCD1234");
  MediaSource dial_source(
      GURL(base::StrCat({kCastDialPresentationUrlScheme, ":YouTube"})));
  MediaSource presentation_url(GURL("https://www.example.com"));

  base::HistogramTester tester;
  RecordPresentationRequestUrlBySink(cast_source,
                                     mojom::MediaRouteProviderId::CAST, 5);
  RecordPresentationRequestUrlBySink(dial_source,
                                     mojom::MediaRouteProviderId::DIAL, 4);
  RecordPresentationRequestUrlBySink(presentation_url,
                                     mojom::MediaRouteProviderId::CAST, 3);
  RecordPresentationRequestUrlBySink(
      presentation_url, mojom::MediaRouteProviderId::WIRED_DISPLAY, 2);
  // DIAL devices don't support normal URLs, so this will get logged as
  // kUnknown.
  RecordPresentationRequestUrlBySink(presentation_url,
                                     mojom::MediaRouteProviderId::DIAL, 1);

  EXPECT_THAT(
      tester.GetAllSamples("MediaRouter.PresentationRequest.UrlBySink"),
      testing::UnorderedElementsAre(
          Bucket(static_cast<int>(PresentationUrlBySink::kUnknown), 1),
          Bucket(
              static_cast<int>(PresentationUrlBySink::kNormalUrlToChromecast),
              3),
          Bucket(
              static_cast<int>(PresentationUrlBySink::kNormalUrlToWiredDisplay),
              2),
          Bucket(static_cast<int>(PresentationUrlBySink::kCastUrlToChromecast),
                 5),
          Bucket(static_cast<int>(PresentationUrlBySink::kDialUrlToDial), 4)));
}

TEST_F(MediaRouterMojoImplTest, TestGetCurrentRoutes) {
  MediaSource source1("source_1");
  MediaSource source2("source_1");
  MediaRoute route1("route_1", source1, "sink_1", "", false);
  MediaRoute route2("route_2", source2, "sink_2", "", true);
  std::vector<MediaRoute> routes = {route1, route2};

  EXPECT_TRUE(router()->GetCurrentRoutes().empty());
  router()->internal_routes_observer_->OnRoutesUpdated(routes);
  std::vector<MediaRoute> current_routes = router()->GetCurrentRoutes();
  ASSERT_EQ(current_routes.size(), 2u);
  EXPECT_EQ(current_routes[0], route1);
  EXPECT_EQ(current_routes[1], route2);

  router()->internal_routes_observer_->OnRoutesUpdated(
      std::vector<MediaRoute>());
  EXPECT_TRUE(router()->GetCurrentRoutes().empty());
}

TEST_F(MediaRouterMojoImplTest, GetMirroringMediaControllerHost) {
  MediaSource tab_source(kTabSourceOne);
  auto local_mirroring_route =
      MediaRoute(kRouteId, tab_source, kSinkId, kDescription, true);
  local_mirroring_route.set_controller_type(RouteControllerType::kGeneric);
  std::vector<MediaRoute> local_mirroring_routes{local_mirroring_route};
  EXPECT_CALL(mock_cast_provider_,
              CreateMediaRouteControllerInternal(kRouteId, _, _, _))
      .WillOnce(
          [&](const std::string& route_id,
              mojo::PendingReceiver<mojom::MediaController>& media_controller,
              mojo::PendingRemote<mojom::MediaStatusObserver>& observer,
              MockMediaRouteProvider::CreateMediaRouteControllerCallback&
                  callback) { std::move(callback).Run(true); });
  UpdateRoutes(mojom::MediaRouteProviderId::CAST, local_mirroring_routes);
  base::RunLoop().RunUntilIdle();

  // Expect the host to exist for a local mirroring source.
  EXPECT_NE(nullptr, router()->GetMirroringMediaControllerHost(kRouteId));

  std::vector<MediaRoute> nonlocal_mirroring_routes{
      MediaRoute(kRouteId2, tab_source, kSinkId, kDescription, false)};
  UpdateRoutes(mojom::MediaRouteProviderId::CAST, nonlocal_mirroring_routes);

  // Expect that the host for kRouteId no longer exists.
  EXPECT_EQ(nullptr, router()->GetMirroringMediaControllerHost(kRouteId));

  // Expect that no host for kRouteId2 exists, as it is not a local source.
  EXPECT_EQ(nullptr, router()->GetMirroringMediaControllerHost(kRouteId2));
}

}  // namespace media_router
