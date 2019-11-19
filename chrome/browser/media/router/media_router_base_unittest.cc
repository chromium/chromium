// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_base.h"

#include "base/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/media/router/test/mock_media_router.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::mojom::PresentationConnectionState;
using testing::_;
using testing::SaveArg;

namespace media_router {

// MockMediaRouterBase inherits from MediaRouter but overrides some of its
// methods with mock methods, so we must override them again.
class MockMediaRouterBase : public MockMediaRouter {
 public:
  MockMediaRouterBase() {}
  ~MockMediaRouterBase() override {}

  std::unique_ptr<PresentationConnectionStateSubscription>
  AddPresentationConnectionStateChangedCallback(
      const MediaRoute::Id& route_id,
      const content::PresentationConnectionStateChangedCallback& callback)
      override {
    return MediaRouterBase::AddPresentationConnectionStateChangedCallback(
        route_id, callback);
  }

  void OnIncognitoProfileShutdown() override {
    MediaRouterBase::OnIncognitoProfileShutdown();
  }

  std::vector<MediaRoute> GetCurrentRoutes() const override {
    return MediaRouterBase::GetCurrentRoutes();
  }
};

class MediaRouterBaseTest : public testing::Test {
 public:
  void SetUp() override {
    EXPECT_CALL(router_, RegisterMediaRoutesObserver(_))
        .WillOnce(SaveArg<0>(&routes_observer_));
    router_.Initialize();
  }

  void TearDown() override { router_.Shutdown(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  MockMediaRouterBase router_;
  MediaRoutesObserver* routes_observer_;
};

TEST_F(MediaRouterBaseTest, CreatePresentationIds) {
  std::string id1 = MediaRouterBase::CreatePresentationId();
  std::string id2 = MediaRouterBase::CreatePresentationId();
  EXPECT_NE(id1, "");
  EXPECT_NE(id2, "");
  EXPECT_NE(id1, id2);
}

TEST_F(MediaRouterBaseTest, NotifyCallbacks) {
  MediaRoute::Id route_id1("id1");
  MediaRoute::Id route_id2("id2");
  base::MockCallback<content::PresentationConnectionStateChangedCallback>
      callback1;
  base::MockCallback<content::PresentationConnectionStateChangedCallback>
      callback2;
  std::unique_ptr<PresentationConnectionStateSubscription> subscription1 =
      router_.AddPresentationConnectionStateChangedCallback(route_id1,
                                                            callback1.Get());
  std::unique_ptr<PresentationConnectionStateSubscription> subscription2 =
      router_.AddPresentationConnectionStateChangedCallback(route_id2,
                                                            callback2.Get());

  content::PresentationConnectionStateChangeInfo change_info_connected(
      PresentationConnectionState::CONNECTED);
  content::PresentationConnectionStateChangeInfo change_info_terminated(
      PresentationConnectionState::TERMINATED);
  content::PresentationConnectionStateChangeInfo change_info_closed(
      PresentationConnectionState::CLOSED);
  change_info_closed.close_reason =
      blink::mojom::PresentationConnectionCloseReason::WENT_AWAY;
  change_info_closed.message = "Test message";

  EXPECT_CALL(callback1, Run(StateChangeInfoEquals(change_info_connected)));
  router_.NotifyPresentationConnectionStateChange(
      route_id1, PresentationConnectionState::CONNECTED);

  EXPECT_CALL(callback2, Run(StateChangeInfoEquals(change_info_connected)));
  router_.NotifyPresentationConnectionStateChange(
      route_id2, PresentationConnectionState::CONNECTED);

  EXPECT_CALL(callback1, Run(StateChangeInfoEquals(change_info_closed)));
  router_.NotifyPresentationConnectionClose(
      route_id1, change_info_closed.close_reason, change_info_closed.message);

  // After removing a subscription, the corresponding callback should no longer
  // be called.
  subscription1.reset();
  router_.NotifyPresentationConnectionStateChange(
      route_id1, PresentationConnectionState::TERMINATED);

  EXPECT_CALL(callback2, Run(StateChangeInfoEquals(change_info_terminated)));
  router_.NotifyPresentationConnectionStateChange(
      route_id2, PresentationConnectionState::TERMINATED);

  subscription2.reset();
  router_.NotifyPresentationConnectionStateChange(
      route_id2, PresentationConnectionState::TERMINATED);
}

TEST_F(MediaRouterBaseTest, GetCurrentRoutes) {
  MediaSource source1("source_1");
  MediaSource source2("source_1");
  MediaRoute route1("route_1", source1, "sink_1", "", false, false);
  MediaRoute route2("route_2", source2, "sink_2", "", true, false);
  std::vector<MediaRoute> routes = {route1, route2};
  std::vector<MediaRoute::Id> joinable_route_ids = {"route_1"};

  EXPECT_TRUE(router_.GetCurrentRoutes().empty());
  routes_observer_->OnRoutesUpdated(routes, joinable_route_ids);
  std::vector<MediaRoute> current_routes = router_.GetCurrentRoutes();
  ASSERT_EQ(current_routes.size(), 2u);
  EXPECT_EQ(current_routes[0], route1);
  EXPECT_EQ(current_routes[1], route2);

  routes_observer_->OnRoutesUpdated(std::vector<MediaRoute>(),
                                    std::vector<MediaRoute::Id>());
  EXPECT_TRUE(router_.GetCurrentRoutes().empty());
}

}  // namespace media_router
