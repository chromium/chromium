// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/event_dispatcher.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/shared_types.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/move_mouse_tool_request.h"
#include "chrome/browser/actor/tools/type_tool_request.h"
#include "chrome/browser/actor/tools/wait_tool_request.h"
#include "chrome/browser/actor/ui/actor_ui_metrics_types.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/test_support/mock_actor_ui_state_manager.h"
#include "chrome/browser/actor/ui/ui_event.h"
#include "chrome/common/actor.mojom-shared.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor::ui {

namespace {

using ::actor::mojom::ActionResultPtr;
using base::test::TestFuture;
using testing::_;
using testing::AllOf;
using testing::Field;
using testing::Return;
using testing::VariantWith;
using testing::WithArgs;

constexpr std::string_view kModelPageTargetTypeHistogram =
    "Actor.EventDispatcher.ModelPageTargetType";
constexpr std::string_view kComputedTargetResultHistogram =
    "Actor.EventDispatcher.ComputedTargetResult";
constexpr std::string_view kRendererResolvedTargetResultHistogram =
    "Actor.EventDispatcher.RendererResolvedTargetResult";
constexpr std::string_view kMouseMoveDurationHistogram =
    "Actor.EventDispatcher.MouseMove.Duration";
constexpr std::string_view kMouseMoveFailureHistogram =
    "Actor.EventDispatcher.MouseMove.Failure";
constexpr std::string_view kMouseClickDurationHistogram =
    "Actor.EventDispatcher.MouseClick.Duration";
constexpr std::string_view kMouseClickFailureHistogram =
    "Actor.EventDispatcher.MouseClick.Failure";

class EventDispatcherTest : public ::testing::Test {
 public:
  EventDispatcherTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kGlicActorSplitValidateAndExecute);
  }

  void SetUp() override {
    mock_state_manager_ = std::make_unique<MockActorUiStateManager>();
    dispatcher_ = NewUiEventDispatcher(mock_state_manager_.get());
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histograms_;
  std::unique_ptr<MockActorUiStateManager> mock_state_manager_;
  std::unique_ptr<UiEventDispatcher> dispatcher_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(EventDispatcherTest, NoEventsToDispatch) {
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(_, _)).Times(0);
  WaitToolRequest tr(base::Microseconds(1000));
  TestFuture<ActionResultPtr> success;
  dispatcher_->OnPostTool(tr, success.GetCallback());
  EXPECT_TRUE(IsOk(*success.Get()));
}

TEST_F(EventDispatcherTest, SingleMouseMove_Point) {
  EXPECT_CALL(
      *mock_state_manager_,
      OnUiEvent(
          VariantWith<MouseMove>(AllOf(
              Field(&MouseMove::tab_handle, tabs::TabHandle(123)),
              Field(&MouseMove::target, gfx::Point(100, 200)),
              Field(&MouseMove::target_source, TargetSource::kToolRequest))),
          _))
      .Times(1)
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        std::move(callback).Run(MakeOkResult());
      }));
  MoveMouseToolRequest tr(tabs::TabHandle(123),
                          PageTarget(gfx::Point(100, 200)));
  TestFuture<ActionResultPtr> result;
  dispatcher_->OnPreTool(tr, result.GetCallback());
  EXPECT_TRUE(IsOk(*result.Get()));
  histograms_.ExpectBucketCount(kModelPageTargetTypeHistogram,
                                ModelPageTargetType::kDomNode, 0);
  histograms_.ExpectBucketCount(kModelPageTargetTypeHistogram,
                                ModelPageTargetType::kPoint, 1);
}

TEST_F(EventDispatcherTest, SingleMouseMove_DomNode) {
  // TODO(crbug.com/434038099): Update test when DomNode conversion is
  // implemented.
  EXPECT_CALL(*mock_state_manager_,
              OnUiEvent(VariantWith<MouseMove>(AllOf(
                            Field(&MouseMove::tab_handle, tabs::TabHandle(123)),
                            Field(&MouseMove::target, std::nullopt),
                            Field(&MouseMove::target_source,
                                  TargetSource::kUnresolvableInApc))),
                        _))
      .Times(1)
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        std::move(callback).Run(MakeOkResult());
      }));
  MoveMouseToolRequest tr(tabs::TabHandle(123),
                          PageTarget(DomNode(100, "abcdefg")));
  TestFuture<ActionResultPtr> result;
  dispatcher_->OnPreTool(tr, result.GetCallback());
  EXPECT_TRUE(IsOk(*result.Get()));
  histograms_.ExpectBucketCount(kModelPageTargetTypeHistogram,
                                ModelPageTargetType::kDomNode, 1);
  histograms_.ExpectBucketCount(kModelPageTargetTypeHistogram,
                                ModelPageTargetType::kPoint, 0);
  histograms_.ExpectBucketCount(kComputedTargetResultHistogram,
                                ComputedTargetResult::kMissingActorTabData, 1);
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
  TestFuture<ActionResultPtr> result1, result2;
  dispatcher_->OnPreTool(tr1, result1.GetCallback());
  dispatcher_->OnPreTool(tr2, result2.GetCallback());
  EXPECT_TRUE(IsOk(*result1.Get()));
  EXPECT_TRUE(IsOk(*result2.Get()));
}

TEST_F(EventDispatcherTest, TwoUiEvents) {
  EXPECT_CALL(
      *mock_state_manager_,
      OnUiEvent(
          VariantWith<MouseMove>((AllOf(
              Field(&MouseMove::tab_handle, tabs::TabHandle(867)),
              Field(&MouseMove::target, gfx::Point(10, 50)),
              Field(&MouseMove::target_source, TargetSource::kToolRequest)))),
          _))
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        std::move(callback).Run(MakeOkResult());
      }));
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<MouseClick>(_), _))
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        std::move(callback).Run(MakeOkResult());
      }));
  ClickToolRequest tr(tabs::TabHandle(867), PageTarget(gfx::Point(10, 50)),
                      mojom::ClickType::kLeft, mojom::ClickCount::kSingle);
  TestFuture<ActionResultPtr> result;
  dispatcher_->OnPreTool(tr, result.GetCallback());
  EXPECT_TRUE(IsOk(*result.Get()));
}

TEST_F(EventDispatcherTest, TwoUiEventsWithFirstOneFailing) {
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<MouseMove>(_), _))
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        task_environment_.FastForwardBy(base::Microseconds(50));
        std::move(callback).Run(
            MakeResult(actor::mojom::ActionResultCode::kActorUiError));
      }));
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<MouseClick>(_), _))
      .Times(0);
  ClickToolRequest tr(tabs::TabHandle(123), PageTarget(gfx::Point(10, 50)),
                      mojom::ClickType::kLeft, mojom::ClickCount::kSingle);
  TestFuture<ActionResultPtr> result;
  dispatcher_->OnPreTool(tr, result.GetCallback());
  EXPECT_EQ(result.Get()->code,
            ::actor::mojom::ActionResultCode::kActorUiError);

  // MouseMove duration shouldn't be recorded as it failed.
  histograms_.ExpectTotalCount(kMouseMoveDurationHistogram, 0);
  histograms_.ExpectBucketCount(kMouseMoveFailureHistogram, true, 1);
  // MouseClick histograms shouldn't be recorded as it was never sent.
  histograms_.ExpectTotalCount(kMouseClickDurationHistogram, 0);
  histograms_.ExpectTotalCount(kMouseClickFailureHistogram, 0);
}

TEST_F(EventDispatcherTest, TwoUiEventsWithSecondOneFailing) {
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<MouseMove>(_), _))
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        task_environment_.FastForwardBy(base::Microseconds(50));
        std::move(callback).Run(MakeOkResult());
      }));
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<MouseClick>(_), _))
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        task_environment_.FastForwardBy(base::Microseconds(50));
        std::move(callback).Run(
            MakeResult(actor::mojom::ActionResultCode::kActorUiError));
      }));
  ClickToolRequest tr(tabs::TabHandle(123), PageTarget(gfx::Point(10, 50)),
                      mojom::ClickType::kLeft, mojom::ClickCount::kSingle);
  TestFuture<ActionResultPtr> result;
  dispatcher_->OnPreTool(tr, result.GetCallback());
  EXPECT_EQ(result.Get()->code,
            ::actor::mojom::ActionResultCode::kActorUiError);

  // MouseMove duration should be recorded as it was completed.
  histograms_.ExpectBucketCount(kMouseMoveDurationHistogram,
                                base::Microseconds(50).InMicroseconds(), 1);
  histograms_.ExpectTotalCount(kMouseMoveFailureHistogram, 0);
  // MouseClick duration shouldn't be recorded as it failed.
  histograms_.ExpectTotalCount(kMouseClickDurationHistogram, 0);
  histograms_.ExpectBucketCount(kMouseClickFailureHistogram, true, 1);
}

TEST_F(EventDispatcherTest, AllUiEventsLogDurationHistograms) {
  // Trigger synchronous events.
  {
    // This generates both StartTask and TaskStateChanged events.
    UiEventDispatcher::ChangeTaskState change_state{
        .task_id = TaskId(1),
        .old_state = ActorTask::State::kCreated,
        .new_state = ActorTask::State::kActing};
    EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<StartTask>(_)));
    EXPECT_CALL(*mock_state_manager_,
                OnUiEvent(VariantWith<TaskStateChanged>(_)));
    dispatcher_->OnActorTaskSyncChange(change_state);
    // This generates a StoppedActingOnTab event.
    UiEventDispatcher::RemoveTab remove_tab{.task_id = TaskId(1),
                                            .handle = tabs::TabHandle(123)};
    EXPECT_CALL(*mock_state_manager_,
                OnUiEvent(VariantWith<StoppedActingOnTab>(_)));
    dispatcher_->OnActorTaskSyncChange(remove_tab);
  }
  // Trigger asynchronous events.
  {
    // This generates both MouseMove and MouseClick events.
    EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<MouseMove>(_), _))
        .WillOnce(WithArgs<1>([](UiCompleteCallback callback) {
          std::move(callback).Run(MakeOkResult());
        }));
    EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<MouseClick>(_), _))
        .WillOnce(WithArgs<1>([](UiCompleteCallback callback) {
          std::move(callback).Run(MakeOkResult());
        }));
    ClickToolRequest click_tr(
        tabs::TabHandle(867), PageTarget(gfx::Point(10, 50)),
        mojom::ClickType::kLeft, mojom::ClickCount::kSingle);
    TestFuture<ActionResultPtr> click_result;
    dispatcher_->OnPreTool(click_tr, click_result.GetCallback());
    ASSERT_TRUE(click_result.Get());
    EXPECT_TRUE(IsOk(*click_result.Get()));

    // An AddTab change generates a StartingToActOnTab event.
    EXPECT_CALL(*mock_state_manager_,
                OnUiEvent(VariantWith<StartingToActOnTab>(_), _))
        .WillOnce(WithArgs<1>([](UiCompleteCallback callback) {
          std::move(callback).Run(MakeOkResult());
        }));
    UiEventDispatcher::AddTab add_tab{.task_id = TaskId(1),
                                      .handle = tabs::TabHandle(456)};
    TestFuture<ActionResultPtr> add_tab_result;
    dispatcher_->OnActorTaskAsyncChange(add_tab, add_tab_result.GetCallback());
    ASSERT_TRUE(add_tab_result.Get());
    EXPECT_TRUE(IsOk(*add_tab_result.Get()));
  }
  // Ensure all SyncUiEvent Durations are logged.
  histograms_.ExpectTotalCount("Actor.EventDispatcher.StartTask.Duration", 1);
  histograms_.ExpectTotalCount(
      "Actor.EventDispatcher.TaskStateChanged.Duration", 1);
  histograms_.ExpectTotalCount(
      "Actor.EventDispatcher.StoppedActingOnTab.Duration", 1);
  // Ensure all AsyncUiEvent Durations are logged.
  histograms_.ExpectTotalCount("Actor.EventDispatcher.MouseMove.Duration", 1);
  histograms_.ExpectTotalCount("Actor.EventDispatcher.MouseClick.Duration", 1);
  histograms_.ExpectTotalCount(
      "Actor.EventDispatcher.StartingToActOnTab.Duration", 1);
}

TEST_F(EventDispatcherTest, TypeCausesMouseMove) {
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<MouseMove>(_), _))
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        std::move(callback).Run(MakeOkResult());
      }));
  TypeToolRequest tr(tabs::TabHandle(456), PageTarget(gfx::Point(300, 400)),
                     "some text to type",
                     /*follow_by_enter=*/true, TypeToolRequest::Mode::kReplace);
  TestFuture<ActionResultPtr> result;
  dispatcher_->OnPreTool(tr, result.GetCallback());
  EXPECT_TRUE(IsOk(*result.Get()));
}

TEST_F(EventDispatcherTest, SyncActorTaskChange_OneEvent) {
  EXPECT_CALL(
      *mock_state_manager_,
      OnUiEvent(VariantWith<TaskStateChanged>(AllOf(
          Field(&TaskStateChanged::task_id, TaskId(999)),
          Field(&TaskStateChanged::state, ActorTask::State::kPausedByActor)))))
      .Times(1);
  dispatcher_->OnActorTaskSyncChange(UiEventDispatcher::ChangeTaskState{
      .task_id = TaskId(999),
      .old_state = ActorTask::State::kActing,
      .new_state = ActorTask::State::kPausedByActor});
}

TEST_F(EventDispatcherTest, SyncActorTaskChange_NewTask) {
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<StartTask>(Field(
                                        &StartTask::task_id, TaskId(222)))))
      .Times(1);
  EXPECT_CALL(*mock_state_manager_,
              OnUiEvent(VariantWith<TaskStateChanged>(AllOf(
                  Field(&TaskStateChanged::task_id, TaskId(222)),
                  Field(&TaskStateChanged::state, ActorTask::State::kActing)))))
      .Times(1);
  dispatcher_->OnActorTaskSyncChange(UiEventDispatcher::ChangeTaskState{
      .task_id = TaskId(222),
      .old_state = ActorTask::State::kCreated,
      .new_state = ActorTask::State::kActing});
}

TEST_F(EventDispatcherTest, SyncActor_StopTask) {
  EXPECT_CALL(
      *mock_state_manager_,
      OnUiEvent(VariantWith<StopTask>(AllOf(
          Field(&StopTask::task_id, TaskId(1)),
          Field(&StopTask::final_state, ActorTask::State::kCancelled),
          Field(&StopTask::title, ""),
          Field(&StopTask::last_acted_on_tab_handle, tabs::TabHandle(5309))))))
      .Times(1);
  dispatcher_->OnActorTaskSyncChange(UiEventDispatcher::StopTask{
      .task_id = TaskId(1),
      .final_state = ActorTask::State::kCancelled,
      .title = "",
      .last_acted_on_tab_handle = tabs::TabHandle(5309)});
}

TEST_F(EventDispatcherTest, SyncActor_RemoveTab) {
  EXPECT_CALL(*mock_state_manager_,
              OnUiEvent(VariantWith<StoppedActingOnTab>(Field(
                  &StoppedActingOnTab::tab_handle, tabs::TabHandle(5309)))))
      .Times(1);
  dispatcher_->OnActorTaskSyncChange(UiEventDispatcher::RemoveTab{
      .task_id = TaskId(867), .handle = tabs::TabHandle(5309)});
}

TEST_F(EventDispatcherTest, AsyncActorTaskChange_OneEvent) {
  EXPECT_CALL(*mock_state_manager_,
              OnUiEvent(VariantWith<StartingToActOnTab>(_), _))
      .Times(1)
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        std::move(callback).Run(MakeOkResult());
      }));
  TestFuture<ActionResultPtr> result;
  dispatcher_->OnActorTaskAsyncChange(
      UiEventDispatcher::AddTab{.task_id = TaskId(992),
                                .handle = tabs::TabHandle(998)},
      result.GetCallback());
  EXPECT_TRUE(IsOk(*result.Get()));
}

/* Verifies that with GlicActorSplitValidateAndExecute enabled, the event
 * dispatcher prioritizes renderer-resolved targets in ActorTabData over
 * explicit points or DOM calculations. */

class EventDispatcherRendererResolvedTest : public EventDispatcherTest {
 public:
  EventDispatcherRendererResolvedTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        {features::kGlicActorSplitValidateAndExecute,
         features::kGlicActorUiMagicCursor},
        {});
  }
};

TEST_F(EventDispatcherRendererResolvedTest, MissingActorTabData) {
  tabs::TabHandle invalid_handle(999);
  EXPECT_CALL(*mock_state_manager_,
              OnUiEvent(VariantWith<MouseMove>(AllOf(
                            Field(&MouseMove::tab_handle, invalid_handle),
                            Field(&MouseMove::target, std::nullopt),
                            Field(&MouseMove::target_source,
                                  TargetSource::kUnresolvableFromRenderer))),
                        _))
      .Times(1)
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        std::move(callback).Run(MakeOkResult());
      }));

  MoveMouseToolRequest tr(invalid_handle, PageTarget(gfx::Point(100, 200)));

  TestFuture<ActionResultPtr> result;
  dispatcher_->OnPreTool(tr, result.GetCallback());
  EXPECT_TRUE(IsOk(*result.Get()));

  histograms_.ExpectBucketCount(
      kRendererResolvedTargetResultHistogram,
      RendererResolvedTargetResult::kMissingActorTabData, 1);
}

TEST_F(EventDispatcherRendererResolvedTest, TargetHasNoValue) {
  ::actor::TestTabState tab_state;

  EXPECT_CALL(
      *mock_state_manager_,
      OnUiEvent(VariantWith<MouseMove>(AllOf(
                    Field(&MouseMove::tab_handle, tab_state.tab.GetHandle()),
                    Field(&MouseMove::target, std::nullopt),
                    Field(&MouseMove::target_source,
                          TargetSource::kUnresolvableFromRenderer))),
                _))
      .Times(1)
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        std::move(callback).Run(MakeOkResult());
      }));

  MoveMouseToolRequest tr(tab_state.tab.GetHandle(),
                          PageTarget(gfx::Point(100, 200)));

  TestFuture<ActionResultPtr> result;
  dispatcher_->OnPreTool(tr, result.GetCallback());
  EXPECT_TRUE(IsOk(*result.Get()));

  histograms_.ExpectBucketCount(
      kRendererResolvedTargetResultHistogram,
      RendererResolvedTargetResult::kRendererResolvedTargetHasNoValue, 1);
}

TEST_F(EventDispatcherRendererResolvedTest, TargetSuccess_PriorityOverDomNode) {
  ::actor::TestTabState tab_state;
  tab_state.tab_data->SetLastRendererResolvedTarget(gfx::Point(55, 66));

  EXPECT_CALL(
      *mock_state_manager_,
      OnUiEvent(VariantWith<MouseMove>(AllOf(
                    Field(&MouseMove::tab_handle, tab_state.tab.GetHandle()),
                    Field(&MouseMove::target, gfx::Point(55, 66)),
                    Field(&MouseMove::target_source,
                          TargetSource::kRendererResolved))),
                _))
      .Times(1)
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        std::move(callback).Run(MakeOkResult());
      }));

  MoveMouseToolRequest tr(tab_state.tab.GetHandle(),
                          PageTarget(DomNode(100, "abcdefg")));

  TestFuture<ActionResultPtr> result;
  dispatcher_->OnPreTool(tr, result.GetCallback());
  EXPECT_TRUE(IsOk(*result.Get()));

  histograms_.ExpectBucketCount(kRendererResolvedTargetResultHistogram,
                                RendererResolvedTargetResult::kSuccess, 1);
  histograms_.ExpectBucketCount(kModelPageTargetTypeHistogram,
                                ModelPageTargetType::kDomNode, 0);
}

TEST_F(EventDispatcherRendererResolvedTest, TargetSuccess_PriorityOverPoint) {
  ::actor::TestTabState tab_state;
  tab_state.tab_data->SetLastRendererResolvedTarget(gfx::Point(55, 66));

  EXPECT_CALL(
      *mock_state_manager_,
      OnUiEvent(VariantWith<MouseMove>(AllOf(
                    Field(&MouseMove::tab_handle, tab_state.tab.GetHandle()),
                    Field(&MouseMove::target, gfx::Point(55, 66)),
                    Field(&MouseMove::target_source,
                          TargetSource::kRendererResolved))),
                _))
      .Times(1)
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        std::move(callback).Run(MakeOkResult());
      }));

  MoveMouseToolRequest tr(tab_state.tab.GetHandle(),
                          PageTarget(gfx::Point(100, 200)));

  TestFuture<ActionResultPtr> result;
  dispatcher_->OnPreTool(tr, result.GetCallback());
  EXPECT_TRUE(IsOk(*result.Get()));

  histograms_.ExpectBucketCount(kRendererResolvedTargetResultHistogram,
                                RendererResolvedTargetResult::kSuccess, 1);
  histograms_.ExpectBucketCount(kModelPageTargetTypeHistogram,
                                ModelPageTargetType::kPoint, 0);
}

// TODO(crbug.com/425784083): improve unit testing

}  // namespace

}  // namespace actor::ui
