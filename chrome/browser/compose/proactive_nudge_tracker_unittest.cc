// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/proactive_nudge_tracker.h"

#include "base/test/task_environment.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/compose/core/browser/config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace compose {

class MockProactiveNudgeTrackerDelegate
    : public ProactiveNudgeTracker::Delegate {
 public:
  MOCK_METHOD(void,
              ShowProactiveNudge,
              (autofill::FormGlobalId, autofill::FieldGlobalId),
              (override));
};

class ProactiveNudgeTrackerTest : public testing::Test {
 public:
  ProactiveNudgeTrackerTest() = default;

  ProactiveNudgeTrackerTest(const ProactiveNudgeTrackerTest&) = delete;
  ProactiveNudgeTrackerTest& operator=(const ProactiveNudgeTrackerTest&) =
      delete;

  ~ProactiveNudgeTrackerTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    nudge_tracker_ = std::make_unique<ProactiveNudgeTracker>(&delegate_);
  }

  MockProactiveNudgeTrackerDelegate& delegate() { return delegate_; }
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }
  ProactiveNudgeTracker& nudge_tracker() { return *nudge_tracker_; }

  autofill::FormFieldData CreateTestFormFieldData() {
    autofill::FormFieldData f;
    f.set_host_frame(autofill::test::MakeLocalFrameToken());
    f.set_renderer_id(autofill::FieldRendererId(123));
    f.set_value(u"FormFieldDataInitialValue");
    return f;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;

  testing::NiceMock<MockProactiveNudgeTrackerDelegate> delegate_;

  std::unique_ptr<ProactiveNudgeTracker> nudge_tracker_;
};

TEST_F(ProactiveNudgeTrackerTest, TestWait) {
  auto field = CreateTestFormFieldData();
  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id()))
      .Times(1);

  EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
  // Should not nudge if nudge is requested too soon.
  EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));

  task_environment().FastForwardBy(base::Seconds(4));
  EXPECT_TRUE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
}

TEST_F(ProactiveNudgeTrackerTest, TestFocusChangePreventsNudge) {
  auto field = CreateTestFormFieldData();
  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id()))
      .Times(0);

  EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
  nudge_tracker().FocusChangedInPage();

  task_environment().FastForwardBy(base::Seconds(4));
  EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
}

TEST_F(ProactiveNudgeTrackerTest, TestTrackingDifferentFormField) {
  auto field = CreateTestFormFieldData();
  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id()))
      .Times(0);

  auto field2 = CreateTestFormFieldData();
  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field2.renderer_form_id(), field2.global_id()))
      .Times(1);

  EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
  EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field2));

  task_environment().FastForwardBy(base::Seconds(4));
  EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
}

TEST_F(ProactiveNudgeTrackerTest, TestFocusChangeInUninitializedState) {
  auto field = CreateTestFormFieldData();
  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id()))
      .Times(0);

  nudge_tracker().FocusChangedInPage();
  task_environment().FastForwardBy(base::Seconds(4));
}

TEST_F(ProactiveNudgeTrackerTest, TestNoNudgeDelay) {
  compose::Config& config = compose::GetMutableConfigForTesting();
  config.proactive_nudge_delay = base::Milliseconds(0);

  auto field = CreateTestFormFieldData();
  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id()))
      .Times(0);

  EXPECT_TRUE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));

  // Wait just in case the timer could be pending.
  task_environment().FastForwardBy(base::Seconds(4));
}

}  // namespace compose
