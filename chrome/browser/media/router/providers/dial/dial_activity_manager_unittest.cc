// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/dial/dial_activity_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/media/router/discovery/dial/dial_app_discovery_service.h"
#include "chrome/browser/media/router/providers/dial/dial_internal_message_util.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Not;

namespace media_router {

TEST(DialActivityTest, From) {
  std::string presentation_id = "presentationId";
  MediaSinkInternal sink = CreateDialSink(1);
  MediaSource::Id source_id =
      "cast-dial:YouTube?clientId=152127444812943594&dialPostData=foo";
  url::Origin origin = url::Origin::Create(GURL("https://www.youtube.com/"));

  auto activity = DialActivity::From(presentation_id, sink, source_id, origin);
  ASSERT_TRUE(activity);

  GURL expected_app_launch_url(sink.dial_data().app_url.spec() + "/YouTube");
  const DialLaunchInfo& launch_info = activity->launch_info;
  EXPECT_EQ("YouTube", launch_info.app_name);
  EXPECT_EQ("152127444812943594", launch_info.client_id);
  EXPECT_EQ("foo", launch_info.post_data);
  EXPECT_EQ(expected_app_launch_url, launch_info.app_launch_url);

  const MediaRoute& route = activity->route;
  EXPECT_EQ(presentation_id, route.presentation_id());
  EXPECT_EQ(source_id, route.media_source().id());
  EXPECT_EQ(sink.sink().id(), route.media_sink_id());
  EXPECT_EQ("YouTube", route.description());
  EXPECT_TRUE(route.is_local());
  EXPECT_FALSE(route.is_local_presentation());
  EXPECT_EQ(RouteControllerType::kNone, route.controller_type());
}

class DialActivityManagerTest : public testing::Test {
 public:
  DialActivityManagerTest()
      : manager_(&app_discovery_service_, &loader_factory_) {}
  ~DialActivityManagerTest() override = default;

  void TestLaunchApp(const DialActivity& activity,
                     const std::optional<std::string>& launch_parameter,
                     const std::optional<GURL>& app_instance_url) {
    manager_.SetExpectedRequest(activity.launch_info.app_launch_url, "POST",
                                launch_parameter ? *launch_parameter : "foo");
    LaunchApp(activity.route.media_route_id(), launch_parameter);

    // |GetRoutes()| returns the route even though it is still launching.
    EXPECT_FALSE(manager_.GetRoutes().empty());

    // Pending launch request, no-op.
    EXPECT_CALL(manager_, OnFetcherCreated()).Times(0);
    LaunchApp(activity.route.media_route_id(), std::nullopt);
    LaunchApp(activity.route.media_route_id(), "bar");

    auto response_head = network::mojom::URLResponseHead::New();
    if (app_instance_url) {
      response_head->headers =
          base::MakeRefCounted<net::HttpResponseHeaders>("");
      response_head->headers->AddHeader("LOCATION", app_instance_url->spec());
    }
    loader_factory_.AddResponse(activity.launch_info.app_launch_url,
                                std::move(response_head), "",
                                network::URLLoaderCompletionStatus());
    EXPECT_CALL(*this, OnAppLaunchResult(true));
    base::RunLoop().RunUntilIdle();

    auto routes = manager_.GetRoutes();
    EXPECT_EQ(1u, routes.size());
    EXPECT_EQ(routes[0], activity.route);

    // App already launched, no-op.
    EXPECT_CALL(manager_, OnFetcherCreated()).Times(0);
    LaunchApp(activity.route.media_route_id(), std::nullopt);
  }

  void LaunchApp(const MediaRoute::Id& route_id,
                 const std::optional<std::string>& launch_parameter) {
    CustomDialLaunchMessageBody message(true, launch_parameter);
    manager_.LaunchApp(
        route_id, message,
        base::BindOnce(&DialActivityManagerTest::OnAppLaunchResult,
                       base::Unretained(this)));
  }

  MOCK_METHOD1(OnAppLaunchResult, void(bool));

  void StopApp(const MediaRoute::Id& route_id) {
    manager_.StopApp(route_id,
                     base::BindOnce(&DialActivityManagerTest::OnStopAppResult,
                                    base::Unretained(this)));
  }

  MOCK_METHOD2(OnStopAppResult,
               void(const std::optional<std::string>&,
                    mojom::RouteRequestResultCode));

  std::unique_ptr<DialActivity> FailToStopApp() {
    auto activity =
        DialActivity::From(presentation_id_, sink_, source_id_, origin_);
    CHECK(activity);
    manager_.AddActivity(*activity);

    TestLaunchApp(*activity, std::nullopt, std::nullopt);

    GURL app_instance_url =
        GURL(activity->launch_info.app_launch_url.spec() + "/run");
    manager_.SetExpectedRequest(app_instance_url, "DELETE", std::nullopt);
    StopApp(activity->route.media_route_id());

    loader_factory_.AddResponse(
        app_instance_url, network::mojom::URLResponseHead::New(), "",
        network::URLLoaderCompletionStatus(net::HTTP_SERVICE_UNAVAILABLE));
    EXPECT_CALL(*this,
                OnStopAppResult(_, Not(mojom::RouteRequestResultCode::OK)));
    base::RunLoop().RunUntilIdle();
    return activity;
  }

 protected:
  base::test::TaskEnvironment environment_;
  std::string presentation_id_{"presentationId"};
  MediaSinkInternal sink_{CreateDialSink(1)};
  MediaSource::Id source_id_{
      "cast-dial:YouTube?clientId=152127444812943594&dialPostData=foo"};
  url::Origin origin_{url::Origin::Create(GURL{"https://www.youtube.com/"})};
  network::TestURLLoaderFactory loader_factory_;
  NiceMock<MockDialAppDiscoveryService> app_discovery_service_;
  TestDialActivityManager manager_;
};

TEST_F(DialActivityManagerTest, AddActivity) {
  auto activity =
      DialActivity::From(presentation_id_, sink_, source_id_, origin_);
  ASSERT_TRUE(activity);

  EXPECT_TRUE(manager_.GetRoutes().empty());

  manager_.AddActivity(*activity);

  // |GetRoutes()| returns the route even though it is still launching.
  EXPECT_FALSE(manager_.GetRoutes().empty());
  EXPECT_TRUE(manager_.GetActivity(activity->route.media_route_id()));
}

TEST_F(DialActivityManagerTest, GetActivityBySinkId) {
  auto activity =
      DialActivity::From(presentation_id_, sink_, source_id_, origin_);
  manager_.AddActivity(*activity);
  EXPECT_TRUE(manager_.GetActivityBySinkId(sink_.id()));
  EXPECT_FALSE(manager_.GetActivityBySinkId("wrong-sink-id"));
}

TEST_F(DialActivityManagerTest, GetActivityToJoin) {
  auto activity =
      DialActivity::From(presentation_id_, sink_, source_id_, origin_);
  manager_.AddActivity(*activity);
  EXPECT_TRUE(manager_.GetActivityToJoin(presentation_id_,
                                         MediaSource(source_id_), origin_));
  EXPECT_FALSE(manager_.GetActivityToJoin(
      presentation_id_, MediaSource("wrong-source-id"), origin_));
  EXPECT_FALSE(manager_.GetActivityToJoin("wrong-presentation-id",
                                          MediaSource(source_id_), origin_));
}

TEST_F(DialActivityManagerTest, LaunchApp) {
  auto activity =
      DialActivity::From(presentation_id_, sink_, source_id_, origin_);
  ASSERT_TRUE(activity);
  manager_.AddActivity(*activity);

  GURL app_instance_url =
      GURL(sink_.dial_data().app_url.spec() + "/YouTube/app_instance");
  TestLaunchApp(*activity, std::nullopt, app_instance_url);
}

TEST_F(DialActivityManagerTest, LaunchAppLaunchParameter) {
  auto activity =
      DialActivity::From(presentation_id_, sink_, source_id_, origin_);
  ASSERT_TRUE(activity);
  manager_.AddActivity(*activity);

  GURL app_instance_url =
      GURL(sink_.dial_data().app_url.spec() + "/YouTube/app_instance");
  TestLaunchApp(*activity, "bar", app_instance_url);
}

TEST_F(DialActivityManagerTest, LaunchAppFails) {
  auto activity =
      DialActivity::From(presentation_id_, sink_, source_id_, origin_);
  ASSERT_TRUE(activity);
  manager_.AddActivity(*activity);

  manager_.SetExpectedRequest(activity->launch_info.app_launch_url, "POST",
                              "foo");
  LaunchApp(activity->route.media_route_id(), std::nullopt);

  loader_factory_.AddResponse(
      activity->launch_info.app_launch_url,
      network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::HTTP_SERVICE_UNAVAILABLE));
  EXPECT_CALL(*this, OnAppLaunchResult(false));
  base::RunLoop().RunUntilIdle();

  // Activity is removed on failure.
  EXPECT_TRUE(manager_.GetRoutes().empty());
  EXPECT_FALSE(manager_.GetActivity(activity->route.media_route_id()));
}

TEST_F(DialActivityManagerTest, StopApp) {
  auto activity =
      DialActivity::From(presentation_id_, sink_, source_id_, origin_);
  ASSERT_TRUE(activity);
  manager_.AddActivity(*activity);

  GURL app_instance_url =
      GURL(sink_.dial_data().app_url.spec() + "/YouTube/app_instance");
  TestLaunchApp(*activity, std::nullopt, app_instance_url);

  auto can_stop = manager_.CanStopApp(activity->route.media_route_id());
  EXPECT_EQ(can_stop.second, mojom::RouteRequestResultCode::OK);
  manager_.SetExpectedRequest(app_instance_url, "DELETE", std::nullopt);
  StopApp(activity->route.media_route_id());
  testing::Mock::VerifyAndClearExpectations(this);

  // // The result should not be OK because there is a pending request.
  can_stop = manager_.CanStopApp(activity->route.media_route_id());
  EXPECT_NE(can_stop.second, mojom::RouteRequestResultCode::OK);

  loader_factory_.AddResponse(app_instance_url,
                              network::mojom::URLResponseHead::New(), "",
                              network::URLLoaderCompletionStatus());
  EXPECT_CALL(*this, OnStopAppResult(testing::Eq(std::nullopt),
                                     mojom::RouteRequestResultCode::OK));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(manager_.GetRoutes().empty());
}

TEST_F(DialActivityManagerTest, StopAppUseFallbackURL) {
  auto activity =
      DialActivity::From(presentation_id_, sink_, source_id_, origin_);
  ASSERT_TRUE(activity);
  manager_.AddActivity(*activity);

  TestLaunchApp(*activity, std::nullopt, std::nullopt);

  GURL app_instance_url =
      GURL(activity->launch_info.app_launch_url.spec() + "/run");
  manager_.SetExpectedRequest(app_instance_url, "DELETE", std::nullopt);
  StopApp(activity->route.media_route_id());

  loader_factory_.AddResponse(app_instance_url,
                              network::mojom::URLResponseHead::New(), "",
                              network::URLLoaderCompletionStatus());
  EXPECT_CALL(*this, OnStopAppResult(testing::Eq(std::nullopt),
                                     mojom::RouteRequestResultCode::OK));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(manager_.GetRoutes().empty());
}

TEST_F(DialActivityManagerTest, StopAppFails) {
  auto activity = FailToStopApp();
  app_discovery_service_.PassCallback().Run(
      sink_.sink().id(), "YouTube",
      DialAppInfoResult(
          CreateParsedDialAppInfoPtr("YouTube", DialAppState::kRunning),
          DialAppInfoResultCode::kOk));

  // Even if we fail to terminate an app, we remove it from the list of routes
  // tracked by DialActivityManager. This is done because manually stopping Cast
  // session from DIAL device is not reflected on Chrome side.
  EXPECT_FALSE(manager_.GetActivity(activity->route.media_route_id()));
  EXPECT_TRUE(manager_.GetRoutes().empty());
}

TEST_F(DialActivityManagerTest, TryToStopAppThatIsAlreadyStopped) {
  auto activity = FailToStopApp();
  app_discovery_service_.PassCallback().Run(
      sink_.sink().id(), "YouTube",
      DialAppInfoResult(
          CreateParsedDialAppInfoPtr("YouTube", DialAppState::kStopped),
          DialAppInfoResultCode::kOk));

  // |manager_|, upon learning that the app's state is already |kStopped|,
  // should remove the route.
  EXPECT_TRUE(manager_.GetRoutes().empty());
}

}  // namespace media_router
