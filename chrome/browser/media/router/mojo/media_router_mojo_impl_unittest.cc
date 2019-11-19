// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_mojo_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/string_escape.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_metrics.h"
#include "chrome/browser/media/router/route_message_observer.h"
#include "chrome/browser/media/router/route_message_util.h"
#include "chrome/browser/media/router/test/media_router_mojo_test.h"
#include "chrome/browser/media/router/test/mock_media_router.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/common/media_router/issue.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/media_source.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::mojom::PresentationConnectionState;
using media_router::mojom::RouteMessagePtr;
using testing::_;
using testing::AtMost;
using testing::Eq;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::IsEmpty;
using testing::Mock;
using testing::Not;
using testing::Pointee;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;
using testing::Sequence;
using testing::SizeIs;
using testing::UnorderedElementsAre;
using testing::Unused;
using testing::WithArg;

namespace media_router {

namespace {

const char kDescription[] = "description";
const char kError[] = "error";
const char kSource[] = "source1";
const char kSource2[] = "source2";
const char kRouteId[] = "routeId";
const char kRouteId2[] = "routeId2";
const char kJoinableRouteId[] = "joinableRouteId";
const char kJoinableRouteId2[] = "joinableRouteId2";
const char kSinkId[] = "sink";
const char kSinkId2[] = "sink2";
const char kSinkName[] = "sinkName";
const char kPresentationId[] = "presentationId";
const char kOrigin[] = "http://origin/";
const int kInvalidTabId = -1;
const int kTimeoutMillis = 5 * 1000;

IssueInfo CreateIssueInfo(const std::string& title) {
  IssueInfo issue_info;
  issue_info.title = title;
  issue_info.message = std::string("msg");
  issue_info.default_action = IssueInfo::Action::DISMISS;
  issue_info.severity = IssueInfo::Severity::WARNING;
  return issue_info;
}

// Creates a media route whose ID is |kRouteId|.
MediaRoute CreateMediaRoute() {
  MediaRoute route(kRouteId, MediaSource(kSource), kSinkId, kDescription, true,
                   true);
  route.set_presentation_id(kPresentationId);
  route.set_controller_type(RouteControllerType::kGeneric);
  return route;
}

// Creates a media route whose ID is |kRouteId2|.
MediaRoute CreateMediaRoute2() {
  MediaRoute route(kRouteId2, MediaSource(kSource), kSinkId, kDescription, true,
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

}  // namespace

class MediaRouterMojoImplTest : public MediaRouterMojoTest {
 public:
  MediaRouterMojoImplTest() {}
  ~MediaRouterMojoImplTest() override {}

 protected:
  void ExpectResultBucketCount(const std::string& operation,
                               RouteRequestResult::ResultCode result_code,
                               int expected_count) {
    histogram_tester_.ExpectBucketCount(
        "MediaRouter.Provider." + operation + ".Result", result_code,
        expected_count);
  }

  std::unique_ptr<MediaRouterMojoImpl> CreateMediaRouter() override {
    return std::unique_ptr<MediaRouterMojoImpl>(
        new MediaRouterMojoImpl(profile()));
  }

 private:
  base::HistogramTester histogram_tester_;
};

TEST_F(MediaRouterMojoImplTest, CreateRoute) {
  TestCreateRoute();
  ExpectResultBucketCount("CreateRoute", RouteRequestResult::OK, 1);
}

// Tests that MediaRouter is aware when a route is created, even if
// MediaRouteProvider doesn't call OnRoutesUpdated().
TEST_F(MediaRouterMojoImplTest, RouteRecognizedAfterCreation) {
  MockMediaRoutesObserver routes_observer(router());

  EXPECT_CALL(routes_observer, OnRoutesUpdated(SizeIs(1), _));
  // TestCreateRoute() does not explicitly call OnRoutesUpdated() on the router.
  TestCreateRoute();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaRouterMojoImplTest, CreateIncognitoRoute) {
  ProvideTestSink(MediaRouteProviderId::EXTENSION, kSinkId);
  MediaSource media_source(kSource);
  MediaRoute expected_route(kRouteId, media_source, kSinkId, "", false, false);
  expected_route.set_incognito(true);

  // Use a lambda function as an invocation target here to work around
  // a limitation with GMock::Invoke that prevents it from using move-only types
  // in runnable parameter lists.
  EXPECT_CALL(mock_extension_provider_,
              CreateRouteInternal(kSource, kSinkId, _,
                                  url::Origin::Create(GURL(kOrigin)),
                                  kInvalidTabId, _, _, _))
      .WillOnce(Invoke([&expected_route](
                           const std::string& source, const std::string& sink,
                           const std::string& presentation_id,
                           const url::Origin& origin, int tab_id,
                           base::TimeDelta timeout, bool incognito,
                           mojom::MediaRouteProvider::CreateRouteCallback& cb) {
        std::move(cb).Run(expected_route, nullptr, std::string(),
                          RouteRequestResult::OK);
      }));

  base::RunLoop run_loop;
  RouteResponseCallbackHandler handler;
  EXPECT_CALL(handler, DoInvoke(Pointee(expected_route), Not(""), "",
                                RouteRequestResult::OK, _))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  router()->CreateRoute(
      kSource, kSinkId, url::Origin::Create(GURL(kOrigin)), nullptr,
      base::BindOnce(&RouteResponseCallbackHandler::Invoke,
                     base::Unretained(&handler)),
      base::TimeDelta::FromMilliseconds(kTimeoutMillis), true);
  run_loop.Run();
  ExpectResultBucketCount("CreateRoute", RouteRequestResult::OK, 1);
}

TEST_F(MediaRouterMojoImplTest, CreateRouteFails) {
  ProvideTestSink(MediaRouteProviderId::EXTENSION, kSinkId);
  EXPECT_CALL(mock_extension_provider_,
              CreateRouteInternal(
                  kSource, kSinkId, _, url::Origin::Create(GURL(kOrigin)),
                  kInvalidTabId,
                  base::TimeDelta::FromMilliseconds(kTimeoutMillis), _, _))
      .WillOnce(Invoke([](const std::string& source, const std::string& sink,
                          const std::string& presentation_id,
                          const url::Origin& origin, int tab_id,
                          base::TimeDelta timeout, bool incognito,
                          mojom::MediaRouteProvider::CreateRouteCallback& cb) {
        std::move(cb).Run(base::nullopt, nullptr, std::string(kError),
                          RouteRequestResult::TIMED_OUT);
      }));

  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  EXPECT_CALL(handler,
              DoInvoke(nullptr, "", kError, RouteRequestResult::TIMED_OUT, _))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  router()->CreateRoute(
      kSource, kSinkId, url::Origin::Create(GURL(kOrigin)), nullptr,
      base::BindOnce(&RouteResponseCallbackHandler::Invoke,
                     base::Unretained(&handler)),
      base::TimeDelta::FromMilliseconds(kTimeoutMillis), false);
  run_loop.Run();
  ExpectResultBucketCount("CreateRoute", RouteRequestResult::TIMED_OUT, 1);
}

TEST_F(MediaRouterMojoImplTest, CreateRouteIncognitoMismatchFails) {
  ProvideTestSink(MediaRouteProviderId::EXTENSION, kSinkId);
  EXPECT_CALL(mock_extension_provider_,
              CreateRouteInternal(
                  kSource, kSinkId, _, url::Origin::Create(GURL(kOrigin)),
                  kInvalidTabId,
                  base::TimeDelta::FromMilliseconds(kTimeoutMillis), true, _))
      .WillOnce(Invoke([](const std::string& source, const std::string& sink,
                          const std::string& presentation_id,
                          const url::Origin& origin, int tab_id,
                          base::TimeDelta timeout, bool incognito,
                          mojom::MediaRouteProvider::CreateRouteCallback& cb) {
        std::move(cb).Run(CreateMediaRoute(), nullptr, std::string(),
                          RouteRequestResult::OK);
      }));

  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  std::string error("Mismatch in incognito status: request = 1, response = 0");
  EXPECT_CALL(handler, DoInvoke(nullptr, "", error,
                                RouteRequestResult::INCOGNITO_MISMATCH, _))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  router()->CreateRoute(
      kSource, kSinkId, url::Origin::Create(GURL(kOrigin)), nullptr,
      base::BindOnce(&RouteResponseCallbackHandler::Invoke,
                     base::Unretained(&handler)),
      base::TimeDelta::FromMilliseconds(kTimeoutMillis), true);
  run_loop.Run();
  ExpectResultBucketCount("CreateRoute", RouteRequestResult::INCOGNITO_MISMATCH,
                          1);
}

TEST_F(MediaRouterMojoImplTest, IncognitoRoutesTerminatedOnProfileShutdown) {
  MediaRoute route = CreateMediaRoute();
  route.set_incognito(true);
  ProvideTestSink(MediaRouteProviderId::EXTENSION, kSinkId);

  EXPECT_CALL(mock_extension_provider_,
              CreateRouteInternal(
                  kSource, kSinkId, _, url::Origin::Create(GURL(kOrigin)),
                  kInvalidTabId,
                  base::TimeDelta::FromMilliseconds(kTimeoutMillis), true, _))
      .WillOnce(
          Invoke([&route](const std::string& source, const std::string& sink,
                          const std::string& presentation_id,
                          const url::Origin& origin, int tab_id,
                          base::TimeDelta timeout, bool incognito,
                          mojom::MediaRouteProvider::CreateRouteCallback& cb) {
            std::move(cb).Run(route, nullptr, std::string(),
                              RouteRequestResult::OK);
          }));
  base::RunLoop run_loop;
  router()->CreateRoute(kSource, kSinkId, url::Origin::Create(GURL(kOrigin)),
                        nullptr, base::DoNothing(),
                        base::TimeDelta::FromMilliseconds(kTimeoutMillis),
                        true);
  std::vector<MediaRoute> routes;
  routes.push_back(route);
  router()->OnRoutesUpdated(MediaRouteProviderId::EXTENSION, routes,
                            std::string(), std::vector<std::string>());

  // TODO(mfoltz): Where possible, convert other tests to use RunUntilIdle
  // instead of manually calling Run/Quit on the run loop.
  run_loop.RunUntilIdle();

  EXPECT_CALL(mock_extension_provider_, TerminateRouteInternal(kRouteId, _))
      .WillOnce(
          Invoke([](const std::string& route_id,
                    mojom::MediaRouteProvider::TerminateRouteCallback& cb) {
            std::move(cb).Run(base::nullopt, RouteRequestResult::OK);
          }));

  base::RunLoop run_loop2;
  router()->OnIncognitoProfileShutdown();
  run_loop2.RunUntilIdle();
}

TEST_F(MediaRouterMojoImplTest, JoinRoute) {
  TestJoinRoute(kPresentationId);
  ExpectResultBucketCount("JoinRoute", RouteRequestResult::OK, 1);
}

TEST_F(MediaRouterMojoImplTest, JoinRouteNotFoundFails) {
  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  EXPECT_CALL(handler, DoInvoke(nullptr, "", "Route not found",
                                RouteRequestResult::ROUTE_NOT_FOUND, _))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  router()->JoinRoute(kSource, kPresentationId,
                      url::Origin::Create(GURL(kOrigin)), nullptr,
                      base::BindOnce(&RouteResponseCallbackHandler::Invoke,
                                     base::Unretained(&handler)),
                      base::TimeDelta::FromMilliseconds(kTimeoutMillis), false);
  run_loop.Run();
  ExpectResultBucketCount("JoinRoute", RouteRequestResult::ROUTE_NOT_FOUND, 1);
}

TEST_F(MediaRouterMojoImplTest, JoinRouteTimedOutFails) {
  // Make sure the MR has received an update with the route, so it knows there
  // is a route to join.
  std::vector<MediaRoute> routes;
  routes.push_back(CreateMediaRoute());
  router()->OnRoutesUpdated(MediaRouteProviderId::EXTENSION, routes,
                            std::string(), std::vector<std::string>());
  EXPECT_TRUE(router()->HasJoinableRoute());

  EXPECT_CALL(mock_extension_provider_,
              JoinRouteInternal(
                  kSource, kPresentationId, url::Origin::Create(GURL(kOrigin)),
                  kInvalidTabId,
                  base::TimeDelta::FromMilliseconds(kTimeoutMillis), _, _))
      .WillOnce(Invoke(
          [](const std::string& source, const std::string& presentation_id,
             const url::Origin& origin, int tab_id, base::TimeDelta timeout,
             bool incognito, mojom::MediaRouteProvider::JoinRouteCallback& cb) {
            std::move(cb).Run(base::nullopt, nullptr, std::string(kError),
                              RouteRequestResult::TIMED_OUT);
          }));

  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  EXPECT_CALL(handler,
              DoInvoke(nullptr, "", kError, RouteRequestResult::TIMED_OUT, _))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  router()->JoinRoute(kSource, kPresentationId,
                      url::Origin::Create(GURL(kOrigin)), nullptr,
                      base::BindOnce(&RouteResponseCallbackHandler::Invoke,
                                     base::Unretained(&handler)),
                      base::TimeDelta::FromMilliseconds(kTimeoutMillis), false);
  run_loop.Run();
  ExpectResultBucketCount("JoinRoute", RouteRequestResult::TIMED_OUT, 1);
}

TEST_F(MediaRouterMojoImplTest, JoinRouteIncognitoMismatchFails) {
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
                  kSource, kPresentationId, url::Origin::Create(GURL(kOrigin)),
                  kInvalidTabId,
                  base::TimeDelta::FromMilliseconds(kTimeoutMillis), true, _))
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
  base::RunLoop run_loop;
  std::string error("Mismatch in incognito status: request = 1, response = 0");
  EXPECT_CALL(handler, DoInvoke(nullptr, "", error,
                                RouteRequestResult::INCOGNITO_MISMATCH, _))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  router()->JoinRoute(kSource, kPresentationId,
                      url::Origin::Create(GURL(kOrigin)), nullptr,
                      base::BindOnce(&RouteResponseCallbackHandler::Invoke,
                                     base::Unretained(&handler)),
                      base::TimeDelta::FromMilliseconds(kTimeoutMillis), true);
  run_loop.Run();
  ExpectResultBucketCount("JoinRoute", RouteRequestResult::INCOGNITO_MISMATCH,
                          1);
}

TEST_F(MediaRouterMojoImplTest, ConnectRouteByRouteId) {
  TestConnectRouteByRouteId();
  ExpectResultBucketCount("JoinRoute", RouteRequestResult::OK, 1);
}

TEST_F(MediaRouterMojoImplTest, ConnectRouteByRouteIdFails) {
  ProvideTestRoute(MediaRouteProviderId::EXTENSION, kRouteId);
  EXPECT_CALL(mock_extension_provider_,
              ConnectRouteByRouteIdInternal(
                  kSource, kRouteId, _, url::Origin::Create(GURL(kOrigin)),
                  kInvalidTabId,
                  base::TimeDelta::FromMilliseconds(kTimeoutMillis), true, _))
      .WillOnce(Invoke(
          [](const std::string& source, const std::string& route_id,
             const std::string& presentation_id, const url::Origin& origin,
             int tab_id, base::TimeDelta timeout, bool incognito,
             mojom::MediaRouteProvider::JoinRouteCallback& cb) {
            std::move(cb).Run(base::nullopt, nullptr, std::string(kError),
                              RouteRequestResult::TIMED_OUT);
          }));

  RouteResponseCallbackHandler handler;
  base::RunLoop run_loop;
  EXPECT_CALL(handler,
              DoInvoke(nullptr, "", kError, RouteRequestResult::TIMED_OUT, _))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  router()->ConnectRouteByRouteId(
      kSource, kRouteId, url::Origin::Create(GURL(kOrigin)), nullptr,
      base::BindOnce(&RouteResponseCallbackHandler::Invoke,
                     base::Unretained(&handler)),
      base::TimeDelta::FromMilliseconds(kTimeoutMillis), true);
  run_loop.Run();
  ExpectResultBucketCount("JoinRoute", RouteRequestResult::TIMED_OUT, 1);
}

TEST_F(MediaRouterMojoImplTest, ConnectRouteByIdIncognitoMismatchFails) {
  MediaRoute route = CreateMediaRoute();
  ProvideTestRoute(MediaRouteProviderId::EXTENSION, kRouteId);

  // Use a lambda function as an invocation target here to work around
  // a limitation with GMock::Invoke that prevents it from using move-only types
  // in runnable parameter lists.
  EXPECT_CALL(mock_extension_provider_,
              ConnectRouteByRouteIdInternal(
                  kSource, kRouteId, _, url::Origin::Create(GURL(kOrigin)),
                  kInvalidTabId,
                  base::TimeDelta::FromMilliseconds(kTimeoutMillis), true, _))
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
  base::RunLoop run_loop;
  std::string error("Mismatch in incognito status: request = 1, response = 0");
  EXPECT_CALL(handler, DoInvoke(nullptr, "", error,
                                RouteRequestResult::INCOGNITO_MISMATCH, _))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  router()->ConnectRouteByRouteId(
      kSource, kRouteId, url::Origin::Create(GURL(kOrigin)), nullptr,
      base::BindOnce(&RouteResponseCallbackHandler::Invoke,
                     base::Unretained(&handler)),
      base::TimeDelta::FromMilliseconds(kTimeoutMillis), true);
  run_loop.Run();
  ExpectResultBucketCount("JoinRoute", RouteRequestResult::INCOGNITO_MISMATCH,
                          1);
}

TEST_F(MediaRouterMojoImplTest, DetachRoute) {
  TestDetachRoute();
}

TEST_F(MediaRouterMojoImplTest, TerminateRoute) {
  TestTerminateRoute();
  ExpectResultBucketCount("TerminateRoute", RouteRequestResult::OK, 1);
}

TEST_F(MediaRouterMojoImplTest, TerminateRouteFails) {
  ProvideTestRoute(MediaRouteProviderId::EXTENSION, kRouteId);
  EXPECT_CALL(mock_extension_provider_, TerminateRouteInternal(kRouteId, _))
      .WillOnce(
          Invoke([](const std::string& route_id,
                    mojom::MediaRouteProvider::TerminateRouteCallback& cb) {
            std::move(cb).Run(std::string("timed out"),
                              RouteRequestResult::TIMED_OUT);
          }));
  router()->TerminateRoute(kRouteId);
  base::RunLoop().RunUntilIdle();
  ExpectResultBucketCount("TerminateRoute", RouteRequestResult::OK, 0);
  ExpectResultBucketCount("TerminateRoute", RouteRequestResult::TIMED_OUT, 1);
}

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

TEST_F(MediaRouterMojoImplTest, RegisterAndUnregisterMediaSinksObserver) {
  router()->OnSinkAvailabilityUpdated(
      MediaRouteProviderId::EXTENSION,
      mojom::MediaRouter::SinkAvailability::AVAILABLE);
  MediaSource media_source(kSource);

  // These should only be called once even if there is more than one observer
  // for a given source.
  EXPECT_CALL(mock_extension_provider_, StartObservingMediaSinks(kSource));
  EXPECT_CALL(mock_extension_provider_, StartObservingMediaSinks(kSource2));

  auto sinks_observer = std::make_unique<MockMediaSinksObserver>(
      router(), media_source, url::Origin::Create(GURL(kOrigin)));
  EXPECT_TRUE(sinks_observer->Init());
  auto extra_sinks_observer = std::make_unique<MockMediaSinksObserver>(
      router(), media_source, url::Origin::Create(GURL(kOrigin)));
  EXPECT_TRUE(extra_sinks_observer->Init());
  auto unrelated_sinks_observer = std::make_unique<MockMediaSinksObserver>(
      router(), MediaSource(kSource2), url::Origin::Create(GURL(kOrigin)));
  EXPECT_TRUE(unrelated_sinks_observer->Init());
  base::RunLoop().RunUntilIdle();

  std::vector<MediaSink> expected_sinks;
  expected_sinks.push_back(MediaSink(kSinkId, kSinkName, SinkIconType::CAST));
  expected_sinks.push_back(MediaSink(kSinkId2, kSinkName, SinkIconType::CAST));

  std::vector<MediaSinkInternal> sinks;
  for (const auto& expected_sink : expected_sinks) {
    MediaSinkInternal sink_internal;
    sink_internal.set_sink(expected_sink);
    sinks.push_back(std::move(sink_internal));
  }

  EXPECT_CALL(*sinks_observer, OnSinksReceived(expected_sinks));
  EXPECT_CALL(*extra_sinks_observer, OnSinksReceived(expected_sinks));
  router()->OnSinksReceived(
      MediaRouteProviderId::EXTENSION, media_source.id(), sinks,
      std::vector<url::Origin>(1, url::Origin::Create(GURL(kOrigin))));

  // Since the MediaRouterMojoImpl has already received results for
  // |media_source|, return cached results to observers that are subsequently
  // registered.
  auto cached_sinks_observer = std::make_unique<MockMediaSinksObserver>(
      router(), media_source, url::Origin::Create(GURL(kOrigin)));
  EXPECT_CALL(*cached_sinks_observer, OnSinksReceived(expected_sinks));
  EXPECT_TRUE(cached_sinks_observer->Init());

  // Different origin from cached result. Empty list will be returned.
  auto cached_sinks_observer2 = std::make_unique<MockMediaSinksObserver>(
      router(), media_source, url::Origin::Create(GURL("https://youtube.com")));
  EXPECT_CALL(*cached_sinks_observer2, OnSinksReceived(IsEmpty()));
  EXPECT_TRUE(cached_sinks_observer2->Init());

  EXPECT_CALL(mock_extension_provider_, StopObservingMediaSinks(kSource));
  EXPECT_CALL(mock_extension_provider_, StopObservingMediaSinks(kSource2));
  sinks_observer.reset();
  extra_sinks_observer.reset();
  unrelated_sinks_observer.reset();
  cached_sinks_observer.reset();
  cached_sinks_observer2.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaRouterMojoImplTest,
       RegisterMediaSinksObserverWithAvailabilityChange) {
  // When availability is UNAVAILABLE, no calls should be made to MRPM.
  router()->OnSinkAvailabilityUpdated(
      MediaRouteProviderId::EXTENSION,
      mojom::MediaRouter::SinkAvailability::UNAVAILABLE);
  MediaSource media_source(kSource);
  auto sinks_observer = std::make_unique<MockMediaSinksObserver>(
      router(), media_source, url::Origin::Create(GURL(kOrigin)));
  EXPECT_CALL(*sinks_observer, OnSinksReceived(IsEmpty()));
  EXPECT_TRUE(sinks_observer->Init());
  MediaSource media_source2(kSource2);
  auto sinks_observer2 = std::make_unique<MockMediaSinksObserver>(
      router(), media_source2, url::Origin::Create(GURL(kOrigin)));
  EXPECT_CALL(*sinks_observer2, OnSinksReceived(IsEmpty()));
  EXPECT_TRUE(sinks_observer2->Init());
  EXPECT_CALL(mock_extension_provider_, StartObservingMediaSinks(kSource))
      .Times(0);
  EXPECT_CALL(mock_extension_provider_, StartObservingMediaSinks(kSource2))
      .Times(0);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_extension_provider_));

  // When availability transitions AVAILABLE, existing sink queries should be
  // sent to MRPM.
  router()->OnSinkAvailabilityUpdated(
      MediaRouteProviderId::EXTENSION,
      mojom::MediaRouter::SinkAvailability::AVAILABLE);
  EXPECT_CALL(mock_extension_provider_, StartObservingMediaSinks(kSource))
      .Times(1);
  EXPECT_CALL(mock_extension_provider_, StartObservingMediaSinks(kSource2))
      .Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_extension_provider_));

  // No change in availability status; no calls should be made to MRPM.
  router()->OnSinkAvailabilityUpdated(
      MediaRouteProviderId::EXTENSION,
      mojom::MediaRouter::SinkAvailability::AVAILABLE);
  EXPECT_CALL(mock_extension_provider_, StartObservingMediaSinks(kSource))
      .Times(0);
  EXPECT_CALL(mock_extension_provider_, StartObservingMediaSinks(kSource2))
      .Times(0);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_extension_provider_));

  // When availability is UNAVAILABLE, queries are already removed from MRPM.
  // Unregistering observer won't result in call to MRPM to remove query.
  router()->OnSinkAvailabilityUpdated(
      MediaRouteProviderId::EXTENSION,
      mojom::MediaRouter::SinkAvailability::UNAVAILABLE);
  EXPECT_CALL(mock_extension_provider_, StopObservingMediaSinks(kSource))
      .Times(0);
  sinks_observer.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_extension_provider_));

  // When availability is AVAILABLE, call is made to MRPM to remove query when
  // observer is unregistered.
  router()->OnSinkAvailabilityUpdated(
      MediaRouteProviderId::EXTENSION,
      mojom::MediaRouter::SinkAvailability::AVAILABLE);
  EXPECT_CALL(mock_extension_provider_, StopObservingMediaSinks(kSource2));
  sinks_observer2.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaRouterMojoImplTest, RegisterAndUnregisterMediaRoutesObserver) {
  MockMediaRouter mock_router;
  MediaSource media_source(kSource);
  MediaSource different_media_source(kSource2);
  EXPECT_CALL(mock_extension_provider_,
              StartObservingMediaRoutes(media_source.id()))
      .Times(1);
  EXPECT_CALL(mock_extension_provider_,
              StartObservingMediaRoutes(different_media_source.id()))
      .Times(1);

  MediaRoutesObserver* observer_captured;
  EXPECT_CALL(mock_router, RegisterMediaRoutesObserver(_))
      .Times(3)
      .WillRepeatedly(SaveArg<0>(&observer_captured));
  MockMediaRoutesObserver routes_observer(&mock_router, media_source.id());
  EXPECT_EQ(observer_captured, &routes_observer);
  MockMediaRoutesObserver extra_routes_observer(&mock_router,
                                                media_source.id());
  EXPECT_EQ(observer_captured, &extra_routes_observer);
  MockMediaRoutesObserver different_routes_observer(
      &mock_router, different_media_source.id());
  EXPECT_EQ(observer_captured, &different_routes_observer);
  router()->RegisterMediaRoutesObserver(&routes_observer);
  router()->RegisterMediaRoutesObserver(&extra_routes_observer);
  router()->RegisterMediaRoutesObserver(&different_routes_observer);

  std::vector<MediaRoute> expected_routes;
  expected_routes.push_back(
      MediaRoute(kRouteId, media_source, kSinkId, kDescription, false, false));
  MediaRoute incognito_expected_route(kRouteId2, media_source, kSinkId,
                                      kDescription, false, false);
  incognito_expected_route.set_incognito(true);
  expected_routes.push_back(incognito_expected_route);
  std::vector<MediaRoute::Id> expected_joinable_route_ids;
  expected_joinable_route_ids.push_back(kJoinableRouteId);
  expected_joinable_route_ids.push_back(kJoinableRouteId2);

  EXPECT_CALL(routes_observer,
              OnRoutesUpdated(expected_routes, expected_joinable_route_ids));
  EXPECT_CALL(extra_routes_observer,
              OnRoutesUpdated(expected_routes, expected_joinable_route_ids));
  EXPECT_CALL(different_routes_observer,
              OnRoutesUpdated(expected_routes, expected_joinable_route_ids))
      .Times(0);
  router()->OnRoutesUpdated(MediaRouteProviderId::EXTENSION, expected_routes,
                            media_source.id(), expected_joinable_route_ids);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_router, UnregisterMediaRoutesObserver(&routes_observer));
  EXPECT_CALL(mock_router,
              UnregisterMediaRoutesObserver(&extra_routes_observer));
  EXPECT_CALL(mock_router,
              UnregisterMediaRoutesObserver(&different_routes_observer));
  router()->UnregisterMediaRoutesObserver(&routes_observer);
  router()->UnregisterMediaRoutesObserver(&extra_routes_observer);
  router()->UnregisterMediaRoutesObserver(&different_routes_observer);
  EXPECT_CALL(mock_extension_provider_,
              StopObservingMediaRoutes(media_source.id()))
      .Times(1);
  EXPECT_CALL(mock_extension_provider_,
              StopObservingMediaRoutes(different_media_source.id()));
  base::RunLoop().RunUntilIdle();
}

// Tests that multiple MediaRoutesObservers having the same query do not cause
// extra extension wake-ups because the OnRoutesUpdated() results are cached.
TEST_F(MediaRouterMojoImplTest, RegisterMediaRoutesObserver_DedupingWithCache) {
  const MediaSource media_source = MediaSource(kSource);
  std::vector<MediaRoute> expected_routes;
  expected_routes.push_back(
      MediaRoute(kRouteId, media_source, kSinkId, kDescription, false, false));
  std::vector<MediaRoute::Id> expected_joinable_route_ids;
  expected_joinable_route_ids.push_back(kJoinableRouteId);

  Sequence sequence;

  // Creating the first observer will wake-up the provider and ask it to start
  // observing routes having source |kSource|. The provider will respond with
  // the existing route.
  EXPECT_CALL(mock_extension_provider_,
              StartObservingMediaRoutes(media_source.id()))
      .Times(1);
  auto observer1 =
      std::make_unique<MockMediaRoutesObserver>(router(), media_source.id());
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(*observer1,
              OnRoutesUpdated(expected_routes, expected_joinable_route_ids))
      .Times(1);
  router()->OnRoutesUpdated(MediaRouteProviderId::EXTENSION, expected_routes,
                            media_source.id(), expected_joinable_route_ids);
  base::RunLoop().RunUntilIdle();

  // Creating two more observers will not wake up the provider. Instead, the
  // cached route list will be returned.
  auto observer2 =
      std::make_unique<MockMediaRoutesObserver>(router(), media_source.id());
  auto observer3 =
      std::make_unique<MockMediaRoutesObserver>(router(), media_source.id());
  EXPECT_CALL(*observer2,
              OnRoutesUpdated(expected_routes, expected_joinable_route_ids))
      .Times(1);
  EXPECT_CALL(*observer3,
              OnRoutesUpdated(expected_routes, expected_joinable_route_ids))
      .Times(1);
  base::RunLoop().RunUntilIdle();

  // Kill 2 of three observers, and expect nothing happens at the provider.
  observer1.reset();
  observer2.reset();
  base::RunLoop().RunUntilIdle();

  // Kill the final observer, and expect the provider to be woken-up and called
  // with the "stop observing" notification.
  EXPECT_CALL(mock_extension_provider_,
              StopObservingMediaRoutes(media_source.id()))
      .Times(1);
  observer3.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaRouterMojoImplTest, SendRouteMessage) {
  TestSendRouteMessage();
}

TEST_F(MediaRouterMojoImplTest, SendRouteBinaryMessage) {
  TestSendRouteBinaryMessage();
}

namespace {

// Used in the RouteMessages* tests to populate the messages that will be
// processed and dispatched to RouteMessageObservers.
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
class ExpectedMessagesObserver : public RouteMessageObserver {
 public:
  ExpectedMessagesObserver(MediaRouter* router,
                           const MediaRoute::Id& route_id,
                           std::vector<RouteMessagePtr> expected_messages)
      : RouteMessageObserver(router, route_id),
        expected_messages_(std::move(expected_messages)) {}

  ~ExpectedMessagesObserver() final { CheckReceivedMessages(); }

 private:
  void OnMessagesReceived(std::vector<RouteMessagePtr> messages) final {
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
  ProvideTestRoute(MediaRouteProviderId::EXTENSION, kRouteId);
  PopulateRouteMessages(&incoming_batch1, &incoming_batch2, &incoming_batch3,
                        &all_messages);

  base::RunLoop run_loop;
  EXPECT_CALL(mock_extension_provider_,
              StartListeningForRouteMessages(Eq(kRouteId)))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  // Creating ExpectedMessagesObserver will register itself to the
  // MediaRouter, which in turn will start listening for route messages.
  ExpectedMessagesObserver observer(router(), kRouteId,
                                    std::move(all_messages));
  run_loop.Run();  // Will quit when StartListeningForRouteMessages() is called.
  router()->OnRouteMessagesReceived(kRouteId, std::move(incoming_batch1));
  router()->OnRouteMessagesReceived(kRouteId, std::move(incoming_batch2));
  router()->OnRouteMessagesReceived(kRouteId, std::move(incoming_batch3));
  // When |observer| goes out-of-scope, its destructor will ensure all expected
  // messages have been received.
}

TEST_F(MediaRouterMojoImplTest, RouteMessagesMultipleObservers) {
  std::vector<RouteMessagePtr> incoming_batch1, incoming_batch2,
      incoming_batch3, all_messages;
  ProvideTestRoute(MediaRouteProviderId::EXTENSION, kRouteId);
  PopulateRouteMessages(&incoming_batch1, &incoming_batch2, &incoming_batch3,
                        &all_messages);
  std::vector<RouteMessagePtr> all_messages2;
  for (auto& message : all_messages)
    all_messages2.emplace_back(message->Clone());

  base::RunLoop run_loop;
  EXPECT_CALL(mock_extension_provider_,
              StartListeningForRouteMessages(Eq(kRouteId)))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  // The ExpectedMessagesObservers will register themselves with the
  // MediaRouter, which in turn will start listening for route messages.
  ExpectedMessagesObserver observer1(router(), kRouteId,
                                     std::move(all_messages));
  ExpectedMessagesObserver observer2(router(), kRouteId,
                                     std::move(all_messages2));
  run_loop.Run();  // Will quit when StartListeningForRouteMessages() is called.
  router()->OnRouteMessagesReceived(kRouteId, std::move(incoming_batch1));
  router()->OnRouteMessagesReceived(kRouteId, std::move(incoming_batch2));
  router()->OnRouteMessagesReceived(kRouteId, std::move(incoming_batch3));
  // As each |observer| goes out-of-scope, its destructor will ensure all
  // expected messages have been received.
}

TEST_F(MediaRouterMojoImplTest, PresentationConnectionStateChangedCallback) {
  MediaRoute::Id route_id("route-id");
  const GURL presentation_url("http://www.example.com/presentation.html");
  const std::string kPresentationId("pid");
  blink::mojom::PresentationInfo connection(presentation_url, kPresentationId);
  base::MockCallback<content::PresentationConnectionStateChangedCallback>
      callback;
  std::unique_ptr<PresentationConnectionStateSubscription> subscription =
      router()->AddPresentationConnectionStateChangedCallback(route_id,
                                                              callback.Get());

  {
    base::RunLoop run_loop;
    content::PresentationConnectionStateChangeInfo closed_info(
        PresentationConnectionState::CLOSED);
    closed_info.close_reason =
        blink::mojom::PresentationConnectionCloseReason::WENT_AWAY;
    closed_info.message = "Foo";

    EXPECT_CALL(callback, Run(StateChangeInfoEquals(closed_info)))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    router()->OnPresentationConnectionClosed(
        route_id,
        mojom::MediaRouter::PresentationConnectionCloseReason::WENT_AWAY,
        "Foo");
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
        route_id, mojom::MediaRouter::PresentationConnectionState::TERMINATED);
    run_loop.Run();

    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&callback));
  }
}

TEST_F(MediaRouterMojoImplTest,
       PresentationConnectionStateChangedCallbackRemoved) {
  MediaRoute::Id route_id("route-id");
  base::MockCallback<content::PresentationConnectionStateChangedCallback>
      callback;
  std::unique_ptr<PresentationConnectionStateSubscription> subscription =
      router()->AddPresentationConnectionStateChangedCallback(route_id,
                                                              callback.Get());

  // Callback has been removed, so we don't expect it to be called anymore.
  subscription.reset();
  EXPECT_TRUE(router()->presentation_connection_state_callbacks_.empty());

  EXPECT_CALL(callback, Run(_)).Times(0);
  router()->OnPresentationConnectionStateChanged(
      route_id, mojom::MediaRouter::PresentationConnectionState::TERMINATED);
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaRouterMojoImplTest, SearchSinks) {
  TestSearchSinks();
}

TEST_F(MediaRouterMojoImplTest, GetMediaController) {
  MockMediaController mock_controller;
  mojo::Remote<mojom::MediaController> controller_remote;
  mojo::PendingRemote<mojom::MediaStatusObserver> observer_remote;
  MockMediaStatusObserver mock_observer(
      observer_remote.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::MediaStatusObserver> observer_remote_held_by_controller;
  router()->OnRoutesUpdated(MediaRouteProviderId::EXTENSION,
                            {CreateMediaRoute()}, "", {});

  EXPECT_CALL(mock_extension_provider_,
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
  // |mock_extension_provider_| has already been registered during setup.
  RegisterWiredDisplayProvider();

  // Set |mock_extension_provider_| to have one sink with ID |kSinkId|.
  ProvideTestSink(MediaRouteProviderId::EXTENSION, kSinkId);
  mock_extension_provider_.SetRouteToReturn(CreateMediaRoute());

  // Set |mock_wired_display_provider_| to have one sink with ID |kSinkId2|.
  ProvideTestSink(MediaRouteProviderId::WIRED_DISPLAY, kSinkId2);
  mock_wired_display_provider_.SetRouteToReturn(CreateMediaRoute2());

  // A request to |kSinkId| should only be sent to |mock_extension_provider_|.
  EXPECT_CALL(mock_extension_provider_,
              CreateRouteInternal(_, kSinkId, _, _, _, _, _, _))
      .WillOnce(
          WithArg<7>(Invoke(&mock_extension_provider_,
                            &MockMediaRouteProvider::RouteRequestSuccess)));
  EXPECT_CALL(mock_wired_display_provider_,
              CreateRouteInternal(_, kSinkId, _, _, _, _, _, _))
      .Times(0);
  router()->CreateRoute(kSource, kSinkId, url::Origin::Create(GURL(kOrigin)),
                        nullptr, base::DoNothing(),
                        base::TimeDelta::FromMilliseconds(kTimeoutMillis),
                        true);

  // A request to |kSinkId2| should only be sent to
  // |mock_wired_display_provider_|.
  EXPECT_CALL(mock_extension_provider_,
              CreateRouteInternal(_, kSinkId2, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(mock_wired_display_provider_,
              CreateRouteInternal(_, kSinkId2, _, _, _, _, _, _))
      .WillOnce(
          WithArg<7>(Invoke(&mock_wired_display_provider_,
                            &MockMediaRouteProvider::RouteRequestSuccess)));
  router()->CreateRoute(kSource, kSinkId2, url::Origin::Create(GURL(kOrigin)),
                        nullptr, base::DoNothing(),
                        base::TimeDelta::FromMilliseconds(kTimeoutMillis),
                        true);
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaRouterMojoImplTest, SendRouteRequestsToMultipleProviders) {
  // Register |mock_wired_display_provider_| with the router.
  // |mock_extension_provider_| has already been registered during setup.
  RegisterWiredDisplayProvider();

  // Set |mock_extension_provider_| to have one route with ID |kRouteId|.
  ProvideTestRoute(MediaRouteProviderId::EXTENSION, kRouteId);

  // Set |mock_wired_display_provider_| to have one route with ID |kRouteId2|.
  ProvideTestRoute(MediaRouteProviderId::WIRED_DISPLAY, kRouteId2);

  // A request for |kRouteId| should only be sent to |mock_extension_provider_|.
  EXPECT_CALL(mock_extension_provider_, DetachRoute(kRouteId));
  EXPECT_CALL(mock_wired_display_provider_, DetachRoute(kRouteId)).Times(0);
  router()->DetachRoute(kRouteId);

  // A request for |kRouteId2| should only be sent to
  // |mock_wired_display_provider_|.
  EXPECT_CALL(mock_extension_provider_, TerminateRouteInternal(kRouteId2, _))
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
  // Sinks for the extension MRP.
  MediaSinkInternal sink1a;
  MediaSinkInternal sink1b;
  sink1a.set_sink(MediaSink("sink1a", "", SinkIconType::CAST,
                            MediaRouteProviderId::EXTENSION));
  sink1b.set_sink(MediaSink("sink1b", "", SinkIconType::CAST,
                            MediaRouteProviderId::EXTENSION));
  // Sinks for the wired display MRP.
  MediaSinkInternal sink2a;
  MediaSinkInternal sink2b;
  sink2a.set_sink(MediaSink("sink2a", "", SinkIconType::CAST,
                            MediaRouteProviderId::WIRED_DISPLAY));
  sink2b.set_sink(MediaSink("sink2b", "", SinkIconType::CAST,
                            MediaRouteProviderId::WIRED_DISPLAY));
  RegisterWiredDisplayProvider();
  MockMediaSinksObserver observer(router(), source,
                                  url::Origin::Create(GURL(kOrigin)));
  observer.Init();

  // Have the extension MRP report sinks.
  EXPECT_CALL(observer, OnSinksReceived(UnorderedElementsAre(sink1a.sink(),
                                                             sink1b.sink())));
  router()->OnSinksReceived(MediaRouteProviderId::EXTENSION, kSource,
                            {sink1a, sink1b}, {});

  // Have the wired display MRP report sinks.
  EXPECT_CALL(observer,
              OnSinksReceived(UnorderedElementsAre(
                  sink1a.sink(), sink1b.sink(), sink2a.sink(), sink2b.sink())));
  router()->OnSinksReceived(MediaRouteProviderId::WIRED_DISPLAY, kSource,
                            {sink2a, sink2b}, {});

  // Have the extension MRP report an empty list of sinks.
  EXPECT_CALL(observer, OnSinksReceived(UnorderedElementsAre(sink2a.sink(),
                                                             sink2b.sink())));
  router()->OnSinksReceived(MediaRouteProviderId::EXTENSION, kSource, {}, {});

  // Have the wired display MRP report an empty list of sinks.
  EXPECT_CALL(observer, OnSinksReceived(IsEmpty()));
  router()->OnSinksReceived(MediaRouteProviderId::WIRED_DISPLAY, kSource, {},
                            {});
}

TEST_F(MediaRouterMojoImplTest, ObserveRoutesFromMultipleProviders) {
  const MediaSource source(kSource);
  // Routes for the extension MRP.
  const MediaRoute route1a("route1a", source, "sink 1a", "", true, true);
  const MediaRoute route1b("route1b", source, "sink 1b", "", true, true);
  // Routes for the wired display MRP.
  const MediaRoute route2a("route2a", source, "sink 2a", "", true, true);
  const MediaRoute route2b("route2b", source, "sink 2b", "", true, true);
  RegisterWiredDisplayProvider();
  MockMediaRoutesObserver observer(router(), kSource);

  // Have the extension MRP report routes.
  EXPECT_CALL(observer,
              OnRoutesUpdated(UnorderedElementsAre(route1a, route1b),
                              UnorderedElementsAre(route1a.media_route_id())));
  router()->OnRoutesUpdated(MediaRouteProviderId::EXTENSION, {route1a, route1b},
                            kSource, {route1a.media_route_id()});

  // Have the wired display MRP report routes.
  EXPECT_CALL(
      observer,
      OnRoutesUpdated(UnorderedElementsAre(route1a, route1b, route2a, route2b),
                      UnorderedElementsAre(route1a.media_route_id(),
                                           route2a.media_route_id())));
  router()->OnRoutesUpdated(MediaRouteProviderId::WIRED_DISPLAY,
                            {route2a, route2b}, kSource,
                            {route2a.media_route_id()});

  // Have the extension MRP report an empty list of routes.
  EXPECT_CALL(observer,
              OnRoutesUpdated(UnorderedElementsAre(route2a, route2b),
                              UnorderedElementsAre(route2a.media_route_id())));
  router()->OnRoutesUpdated(MediaRouteProviderId::EXTENSION, {}, kSource, {});

  // Have the wired display MRP report an empty list of routes.
  EXPECT_CALL(observer, OnRoutesUpdated(IsEmpty(), IsEmpty()));
  router()->OnRoutesUpdated(MediaRouteProviderId::WIRED_DISPLAY, {}, kSource,
                            {});
}

}  // namespace media_router
