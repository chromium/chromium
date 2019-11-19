// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind_helpers.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/media/android/router/media_router_android.h"
#include "chrome/browser/media/android/router/media_router_android_bridge.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using blink::mojom::PresentationConnectionState;
using testing::_;
using testing::Expectation;
using testing::Return;

namespace media_router {

class MockMediaRouterAndroidBridge : public MediaRouterAndroidBridge {
 public:
  MockMediaRouterAndroidBridge() : MediaRouterAndroidBridge(nullptr) {}
  ~MockMediaRouterAndroidBridge() override = default;

  MOCK_METHOD7(CreateRoute,
               void(const MediaSource::Id&,
                    const MediaSink::Id&,
                    const std::string&,
                    const url::Origin&,
                    int,
                    bool,
                    int));
  MOCK_METHOD5(JoinRoute,
               void(const MediaSource::Id&,
                    const std::string&,
                    const url::Origin&,
                    int,
                    int));
  MOCK_METHOD1(TerminateRoute, void(const MediaRoute::Id&));
  MOCK_METHOD2(SendRouteMessage,
               void(const MediaRoute::Id&, const std::string&));
  MOCK_METHOD1(DetachRoute, void(const MediaRoute::Id&));
  MOCK_METHOD1(StartObservingMediaSinks, bool(const MediaSource::Id&));
  MOCK_METHOD1(StopObservingMediaSinks, void(const MediaSource::Id&));
};

class MediaRouterAndroidTest : public testing::Test {
 public:
  void SetUp() override {
    mock_bridge_ = new MockMediaRouterAndroidBridge();
    router_.reset(new MediaRouterAndroid(nullptr));
    router_->SetMediaRouterBridgeForTest(mock_bridge_);
  }

 protected:
  // For the checks that MediaRouter calls are running on the UI thread.
  // Needs to be the first member variable to be destroyed last.
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<MediaRouterAndroid> router_;
  MockMediaRouterAndroidBridge* mock_bridge_;
};

TEST_F(MediaRouterAndroidTest, DetachRoute) {
  base::MockCallback<content::PresentationConnectionStateChangedCallback>
      callback;
  content::PresentationConnectionStateChangeInfo change_info_closed(
      PresentationConnectionState::CLOSED);
  change_info_closed.close_reason =
      blink::mojom::PresentationConnectionCloseReason::CLOSED;
  change_info_closed.message = "Route closed normally";
  EXPECT_CALL(callback, Run(StateChangeInfoEquals(change_info_closed)));

  Expectation createRouteExpectation =
      EXPECT_CALL(*mock_bridge_, CreateRoute(_, _, _, _, _, _, 1))
          .WillOnce(Return());
  EXPECT_CALL(*mock_bridge_, DetachRoute("route"))
      .After(createRouteExpectation)
      .WillOnce(Return());

  router_->CreateRoute("source", "sink", url::Origin(), nullptr,
                       base::DoNothing(), base::TimeDelta(), false);
  router_->OnRouteCreated("route", "sink", 1, false);

  EXPECT_NE(nullptr, router_->FindRouteBySource("source"));

  std::unique_ptr<PresentationConnectionStateSubscription> subscription =
      router_->AddPresentationConnectionStateChangedCallback("route",
                                                             callback.Get());
  router_->DetachRoute("route");

  EXPECT_EQ(nullptr, router_->FindRouteBySource("source"));
}

TEST_F(MediaRouterAndroidTest, OnRouteTerminated) {
  Expectation createRouteExpectation =
      EXPECT_CALL(*mock_bridge_, CreateRoute(_, _, _, _, _, _, 1))
          .WillOnce(Return());

  router_->CreateRoute("source", "sink", url::Origin(), nullptr,
                       base::DoNothing(), base::TimeDelta(), false);
  router_->OnRouteCreated("route", "sink", 1, false);

  EXPECT_NE(nullptr, router_->FindRouteBySource("source"));

  // Route termination on Android results in the PresentationConnectionPtr
  // directly being messaged, and therefore there is no
  // PresentationConnectionStateChangedCallback that can be intercepted for
  // test verification purposes.
  router_->OnRouteTerminated("route");

  EXPECT_EQ(nullptr, router_->FindRouteBySource("source"));
}

TEST_F(MediaRouterAndroidTest, OnRouteClosed) {
  base::MockCallback<content::PresentationConnectionStateChangedCallback>
      callback;
  content::PresentationConnectionStateChangeInfo change_info_closed(
      PresentationConnectionState::CLOSED);
  change_info_closed.close_reason =
      blink::mojom::PresentationConnectionCloseReason::CLOSED;
  change_info_closed.message = "Remove route";
  EXPECT_CALL(callback, Run(StateChangeInfoEquals(change_info_closed)));

  Expectation createRouteExpectation =
      EXPECT_CALL(*mock_bridge_, CreateRoute(_, _, _, _, _, _, 1))
          .WillOnce(Return());

  router_->CreateRoute("source", "sink", url::Origin(), nullptr,
                       base::DoNothing(), base::TimeDelta(), false);
  router_->OnRouteCreated("route", "sink", 1, false);

  EXPECT_NE(nullptr, router_->FindRouteBySource("source"));

  std::unique_ptr<PresentationConnectionStateSubscription> subscription =
      router_->AddPresentationConnectionStateChangedCallback("route",
                                                             callback.Get());
  router_->OnRouteClosed("route", base::nullopt);

  EXPECT_EQ(nullptr, router_->FindRouteBySource("source"));
}

TEST_F(MediaRouterAndroidTest, OnRouteClosedWithError) {
  base::MockCallback<content::PresentationConnectionStateChangedCallback>
      callback;
  content::PresentationConnectionStateChangeInfo change_info_closed(
      PresentationConnectionState::CLOSED);
  change_info_closed.close_reason =
      blink::mojom::PresentationConnectionCloseReason::CONNECTION_ERROR;
  change_info_closed.message = "Some failure";
  EXPECT_CALL(callback, Run(StateChangeInfoEquals(change_info_closed)));

  Expectation createRouteExpectation =
      EXPECT_CALL(*mock_bridge_, CreateRoute(_, _, _, _, _, _, 1))
          .WillOnce(Return());

  router_->CreateRoute("source", "sink", url::Origin(), nullptr,
                       base::DoNothing(), base::TimeDelta(), false);
  router_->OnRouteCreated("route", "sink", 1, false);

  EXPECT_NE(nullptr, router_->FindRouteBySource("source"));

  std::unique_ptr<PresentationConnectionStateSubscription> subscription =
      router_->AddPresentationConnectionStateChangedCallback("route",
                                                             callback.Get());
  router_->OnRouteClosed("route", "Some failure");

  EXPECT_EQ(nullptr, router_->FindRouteBySource("source"));
}

}  // namespace media_router
