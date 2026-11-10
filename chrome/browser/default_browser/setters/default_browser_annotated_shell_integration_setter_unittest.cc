// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/setters/default_browser_annotated_shell_integration_setter.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/default_browser/default_browser_setter.h"
#include "chrome/browser/ui/views/default_browser/annotated_setter_bubble_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace default_browser {

namespace {

using ControllerOutcome = AnnotatedSetterBubbleController::Outcome;
using RecordedOutcome =
    DefaultBrowserAnnotatedShellIntegrationSetter::GuidanceOutcome;

constexpr char kOutcomeHistogram[] =
    "DefaultBrowser.AnnotatedShellIntegration.Outcome";

// Outlives the fakes, which the setter owns.
struct FakeState {
  DefaultBrowserSetterCompletionCallback shell_integration_complete;
  std::optional<bool> shell_integration_can_pin_to_taskbar;
  int guidance_start_count = 0;
  std::optional<base::TimeDelta> guidance_find_timeout;
  std::optional<bool> guidance_can_pin_to_taskbar;
  AnnotatedSetterBubbleController::CompletionCallback guidance_complete;
  bool guidance_alive = false;
};

class FakeShellIntegrationSetter : public DefaultBrowserSetter {
 public:
  explicit FakeShellIntegrationSetter(FakeState& state) : state_(state) {}

  DefaultBrowserSetterType GetType() const override {
    return DefaultBrowserSetterType::kShellIntegration;
  }
  void Execute(DefaultBrowserSetterCompletionCallback on_complete,
               const ExecuteParams& params) override {
    state_->shell_integration_complete = std::move(on_complete);
    state_->shell_integration_can_pin_to_taskbar = params.can_pin_to_taskbar;
  }

 private:
  const raw_ref<FakeState> state_;
};

class FakeBubbleController : public AnnotatedSetterBubbleController {
 public:
  static std::unique_ptr<AnnotatedSetterBubbleController> Create(
      FakeState* state,
      base::TimeDelta find_timeout,
      bool can_pin_to_taskbar,
      CompletionCallback on_complete) {
    return std::make_unique<FakeBubbleController>(
        *state, find_timeout, can_pin_to_taskbar, std::move(on_complete));
  }

  FakeBubbleController(FakeState& state,
                       base::TimeDelta find_timeout,
                       bool can_pin_to_taskbar,
                       CompletionCallback on_complete)
      : state_(state) {
    ++state_->guidance_start_count;
    state_->guidance_find_timeout = find_timeout;
    state_->guidance_can_pin_to_taskbar = can_pin_to_taskbar;
    state_->guidance_complete = std::move(on_complete);
    state_->guidance_alive = true;
  }

  ~FakeBubbleController() override { state_->guidance_alive = false; }

 private:
  const raw_ref<FakeState> state_;
};

class DefaultBrowserAnnotatedShellIntegrationSetterTest : public testing::Test {
 protected:
  void Execute(bool can_pin_to_taskbar) {
    DefaultBrowserSetter::ExecuteParams params;
    params.can_pin_to_taskbar = can_pin_to_taskbar;
    setter_.Execute(result_.GetCallback(), params);
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histograms_;
  FakeState state_;
  base::test::TestFuture<DefaultBrowserState> result_;
  DefaultBrowserAnnotatedShellIntegrationSetter setter_{
      std::make_unique<FakeShellIntegrationSetter>(state_),
      base::BindRepeating(&FakeBubbleController::Create,
                          base::Unretained(&state_))};
};

TEST_F(DefaultBrowserAnnotatedShellIntegrationSetterTest, Type) {
  EXPECT_EQ(setter_.GetType(),
            DefaultBrowserSetterType::kAnnotatedShellIntegration);
}

TEST_F(DefaultBrowserAnnotatedShellIntegrationSetterTest,
       RunsTheFlowWithGuidance) {
  Execute(/*can_pin_to_taskbar=*/true);

  EXPECT_TRUE(state_.guidance_alive);
  EXPECT_EQ(state_.guidance_start_count, 1);
  EXPECT_EQ(state_.guidance_find_timeout, kFindSettingsTimeout.Get());
  EXPECT_EQ(state_.guidance_can_pin_to_taskbar, true);
  EXPECT_EQ(state_.shell_integration_can_pin_to_taskbar, true);
  EXPECT_TRUE(state_.shell_integration_complete);
}

TEST_F(DefaultBrowserAnnotatedShellIntegrationSetterTest,
       ReportsTheFlowResultAndStopsTheGuidance) {
  Execute(/*can_pin_to_taskbar=*/false);

  std::move(state_.shell_integration_complete)
      .Run(DefaultBrowserState::IS_DEFAULT);

  EXPECT_EQ(result_.Get(), DefaultBrowserState::IS_DEFAULT);
  EXPECT_FALSE(state_.guidance_alive);
  histograms_.ExpectUniqueSample(kOutcomeHistogram,
                                 RecordedOutcome::kFlowFinishedDefaultSet, 1);
}

TEST_F(DefaultBrowserAnnotatedShellIntegrationSetterTest,
       ReportsFlowFinishedDefaultNotSetWhenDefaultNotSet) {
  Execute(/*can_pin_to_taskbar=*/false);

  std::move(state_.shell_integration_complete)
      .Run(DefaultBrowserState::NOT_DEFAULT);

  EXPECT_EQ(result_.Get(), DefaultBrowserState::NOT_DEFAULT);
  EXPECT_FALSE(state_.guidance_alive);
  histograms_.ExpectUniqueSample(
      kOutcomeHistogram, RecordedOutcome::kFlowFinishedDefaultNotSet, 1);
}

TEST_F(DefaultBrowserAnnotatedShellIntegrationSetterTest,
       GuidanceEndingLeavesTheFlowRunning) {
  Execute(/*can_pin_to_taskbar=*/false);

  std::move(state_.guidance_complete).Run(ControllerOutcome::kDismissed);
  EXPECT_FALSE(state_.guidance_alive);
  EXPECT_FALSE(result_.IsReady());

  std::move(state_.shell_integration_complete)
      .Run(DefaultBrowserState::NOT_DEFAULT);
  EXPECT_EQ(result_.Get(), DefaultBrowserState::NOT_DEFAULT);
  // The flow finishing after the guidance ended does not record again.
  histograms_.ExpectUniqueSample(kOutcomeHistogram, RecordedOutcome::kDismissed,
                                 1);
}

TEST_F(DefaultBrowserAnnotatedShellIntegrationSetterTest,
       GuidanceOutcomesAreRecorded) {
  const std::pair<ControllerOutcome, RecordedOutcome> kCases[] = {
      {ControllerOutcome::kSettingsWindowClosed,
       RecordedOutcome::kSettingsWindowClosed},
      {ControllerOutcome::kSettingsWindowNotFound,
       RecordedOutcome::kSettingsWindowNotFound},
      {ControllerOutcome::kButtonNotFound, RecordedOutcome::kButtonNotFound},
      {ControllerOutcome::kDismissed, RecordedOutcome::kDismissed},
  };
  for (const auto& [controller_outcome, recorded_outcome] : kCases) {
    base::HistogramTester histograms;
    Execute(/*can_pin_to_taskbar=*/false);
    ASSERT_TRUE(state_.guidance_alive);

    std::move(state_.guidance_complete).Run(controller_outcome);

    histograms.ExpectUniqueSample(kOutcomeHistogram, recorded_outcome, 1);
  }
}

TEST_F(DefaultBrowserAnnotatedShellIntegrationSetterTest,
       DestructionWhileGuidingRecordsAbandoned) {
  FakeState state;
  auto setter = std::make_unique<DefaultBrowserAnnotatedShellIntegrationSetter>(
      std::make_unique<FakeShellIntegrationSetter>(state),
      base::BindRepeating(&FakeBubbleController::Create,
                          base::Unretained(&state)));
  base::test::TestFuture<DefaultBrowserState> result;
  setter->Execute(result.GetCallback(), DefaultBrowserSetter::ExecuteParams());
  ASSERT_TRUE(state.guidance_alive);

  setter.reset();

  EXPECT_FALSE(state.guidance_alive);
  histograms_.ExpectUniqueSample(kOutcomeHistogram, RecordedOutcome::kAbandoned,
                                 1);
}

TEST_F(DefaultBrowserAnnotatedShellIntegrationSetterTest,
       ConcurrentExecuteSharesRunningGuidance) {
  Execute(/*can_pin_to_taskbar=*/false);
  ASSERT_TRUE(state_.guidance_alive);
  ASSERT_EQ(state_.guidance_start_count, 1);

  base::test::TestFuture<DefaultBrowserState> second_result;
  DefaultBrowserSetter::ExecuteParams params;
  params.can_pin_to_taskbar = true;
  setter_.Execute(second_result.GetCallback(), params);

  EXPECT_TRUE(state_.guidance_alive);
  EXPECT_EQ(state_.guidance_start_count, 1);

  std::move(state_.shell_integration_complete)
      .Run(DefaultBrowserState::IS_DEFAULT);
  EXPECT_EQ(second_result.Get(), DefaultBrowserState::IS_DEFAULT);
  EXPECT_FALSE(state_.guidance_alive);
  histograms_.ExpectUniqueSample(kOutcomeHistogram,
                                 RecordedOutcome::kFlowFinishedDefaultSet, 1);
}

}  // namespace

}  // namespace default_browser
