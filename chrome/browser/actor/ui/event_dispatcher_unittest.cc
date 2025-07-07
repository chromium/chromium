// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/event_dispatcher.h"

#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/shared_types.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/move_mouse_tool_request.h"
#include "chrome/browser/actor/tools/wait_tool_request.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/mock_actor_ui_state_manager.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
namespace actor::ui {
namespace {
using base::test::TestFuture;
using testing::_;
using testing::Return;
using testing::VariantWith;
using testing::WithArgs;

class EventDispatcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    profile_ =
        TestingProfile::Builder()
            .AddTestingFactory(ActorKeyedServiceFactory::GetInstance(),
                               base::BindRepeating(
                                   &EventDispatcherTest::BuildActorKeyedService,
                                   base::Unretained(this)))
            .Build();
    dispatcher_ = NewUiEventDispatcher();
  }

  std::unique_ptr<KeyedService> BuildActorKeyedService(
      content::BrowserContext* context) {
    std::unique_ptr<MockActorUiStateManager> mock_state_manager =
        std::make_unique<MockActorUiStateManager>();
    mock_state_manager_ = mock_state_manager.get();

    return std::make_unique<ActorKeyedService>(profile_.get(),
                                               std::move(mock_state_manager));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<UiEventDispatcher> dispatcher_;
  raw_ptr<MockActorUiStateManager> mock_state_manager_;
};

TEST_F(EventDispatcherTest, NoActorKeyedServiceForProfile) {
  std::unique_ptr<TestingProfile> broken_profile =
      TestingProfile::Builder()
          .AddTestingFactory(
              ActorKeyedServiceFactory::GetInstance(),
              base::BindOnce(
                  [](content::BrowserContext*)
                      -> std::unique_ptr<KeyedService> { return nullptr; }))
          .Build();
  MoveMouseToolRequest tr(tabs::TabHandle(123),
                          PageTarget(gfx::Point(100, 200)));
  TestFuture<mojom::ActionResultPtr> result;
  dispatcher_->OnPreTool(broken_profile.get(), tr, result.GetCallback());
  EXPECT_EQ(result.Get()->code, mojom::ActionResultCode::kError);
}

TEST_F(EventDispatcherTest, NoUiStateManager) {
  std::unique_ptr<TestingProfile> broken_profile =
      TestingProfile::Builder()
          .AddTestingFactory(
              ActorKeyedServiceFactory::GetInstance(),
              base::BindOnce([](content::BrowserContext*)
                                 -> std::unique_ptr<KeyedService> {
                return std::make_unique<ActorKeyedService>(
                    /*profile=*/nullptr, /*ui_state_manager=*/nullptr);
              }))
          .Build();
  MoveMouseToolRequest tr(tabs::TabHandle(123),
                          PageTarget(gfx::Point(100, 200)));
  TestFuture<mojom::ActionResultPtr> result;
  dispatcher_->OnPreTool(broken_profile.get(), tr, result.GetCallback());
  EXPECT_EQ(result.Get()->code, mojom::ActionResultCode::kError);
}

TEST_F(EventDispatcherTest, NoEventsToDispatch) {
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(_, _)).Times(0);
  WaitToolRequest tr(base::Microseconds(1000));
  TestFuture<mojom::ActionResultPtr> success;
  dispatcher_->OnPostTool(profile_.get(), tr, success.GetCallback());
  EXPECT_TRUE(IsOk(*success.Get()));
}

TEST_F(EventDispatcherTest, SingleUiEvent) {
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<MouseMove>(_), _))
      .Times(1)
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        std::move(callback).Run(MakeOkResult());
      }));
  MoveMouseToolRequest tr(tabs::TabHandle(123),
                          PageTarget(gfx::Point(100, 200)));
  TestFuture<mojom::ActionResultPtr> result;
  dispatcher_->OnPreTool(profile_.get(), tr, result.GetCallback());
  EXPECT_TRUE(IsOk(*result.Get()));
}

TEST_F(EventDispatcherTest, TwoToolRequests) {
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<MouseMove>(_), _))
      .Times(2)
      .WillRepeatedly(WithArgs<1>([&](UiCompleteCallback callback) {
        std::move(callback).Run(MakeOkResult());
      }));
  MoveMouseToolRequest tr1(tabs::TabHandle(123),
                           PageTarget(gfx::Point(100, 200)));
  MoveMouseToolRequest tr2(tabs::TabHandle(456),
                           PageTarget(gfx::Point(300, 400)));
  TestFuture<mojom::ActionResultPtr> result1, result2;
  dispatcher_->OnPreTool(profile_.get(), tr1, result1.GetCallback());
  dispatcher_->OnPreTool(profile_.get(), tr2, result2.GetCallback());
  EXPECT_TRUE(IsOk(*result1.Get()));
  EXPECT_TRUE(IsOk(*result2.Get()));
}

TEST_F(EventDispatcherTest, TwoUiEvents) {
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<MouseMove>(_), _))
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        std::move(callback).Run(MakeOkResult());
      }));
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<MouseClick>(_), _))
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        std::move(callback).Run(MakeOkResult());
      }));
  ClickToolRequest tr(tabs::TabHandle(123), PageTarget(gfx::Point(10, 50)),
                      MouseClickType::kLeft, MouseClickCount::kSingle);
  TestFuture<mojom::ActionResultPtr> result;
  dispatcher_->OnPreTool(profile_.get(), tr, result.GetCallback());
  EXPECT_TRUE(IsOk(*result.Get()));
}

TEST_F(EventDispatcherTest, TwoUiEventsWithFirstOneFailing) {
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<MouseMove>(_), _))
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        std::move(callback).Run(MakeErrorResult());
      }));
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<MouseClick>(_), _))
      .Times(0);
  ClickToolRequest tr(tabs::TabHandle(123), PageTarget(gfx::Point(10, 50)),
                      MouseClickType::kLeft, MouseClickCount::kSingle);
  TestFuture<mojom::ActionResultPtr> result;
  dispatcher_->OnPreTool(profile_.get(), tr, result.GetCallback());
  EXPECT_EQ(result.Get()->code, mojom::ActionResultCode::kError);
}

// TODO(crbug.com/425784083): improve unit testing

}  // namespace
}  // namespace actor::ui
