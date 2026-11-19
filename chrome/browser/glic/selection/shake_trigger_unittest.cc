// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/shake_trigger.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/selection/wiggle_detector_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

namespace glic {

namespace {

class TestShakeTrigger : public ShakeTrigger {
 public:
  using ShakeTrigger::ShakeTrigger;

  bool trigger_region_capture_called() const {
    return trigger_region_capture_count_ > 0;
  }
  int trigger_region_capture_count() const {
    return trigger_region_capture_count_;
  }

 protected:
  void TriggerRegionCapture() override {
    ++trigger_region_capture_count_;
    ShakeTrigger::TriggerRegionCapture();
  }

 private:
  int trigger_region_capture_count_ = 0;
};

}  // namespace

class ShakeTriggerTest : public ChromeRenderViewHostTestHarness,
                         public ShakeTriggerClient {
 public:
  ShakeTriggerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    trigger_ = std::make_unique<TestShakeTrigger>(web_contents(), *this);
  }

  void TearDown() override {
    trigger_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  bool IsTextSelectionSharingEnabled() const override { return true; }
  bool IsSidePanelOpen() const override { return side_panel_open_; }

 protected:
  // Recreates the trigger, which reads some features when it's created.
  void RecreateTrigger() {
    trigger_ = std::make_unique<TestShakeTrigger>(web_contents(), *this);
  }

  // Sends a mouse move a frame after the previous event.
  void SimulateMouseMove(float x,
                         float y,
                         int modifiers = blink::WebInputEvent::kNoModifiers) {
    Wait(base::Milliseconds(16));
    blink::WebMouseEvent event(
        blink::WebInputEvent::Type::kMouseMove, modifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    event.SetPositionInWidget(x, y);
    trigger_->OnInputEvent(event);
  }

  void SimulateMouseEvent(blink::WebInputEvent::Type type) {
    Wait(base::Milliseconds(16));
    blink::WebMouseEvent event(
        type, blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    trigger_->OnInputEvent(event);
  }

  // Lets time pass before the next event.
  void Wait(base::TimeDelta delta) { task_environment()->FastForwardBy(delta); }

  void SimulateMouseShake() {
    SimulateMouseMove(0.0f, 0.0f);
    SimulateMouseMove(20.0f, 0.0f);
    SimulateMouseMove(0.0f, 0.0f);
    SimulateMouseMove(20.0f, 0.0f);
    SimulateMouseMove(0.0f, 0.0f);
    SimulateMouseMove(20.0f, 0.0f);
  }

  bool side_panel_open_ = true;
  std::unique_ptr<TestShakeTrigger> trigger_;
};

TEST_F(ShakeTriggerTest, ShakeTriggerSucceedsWhenFeatureAndPrefEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kGlicShakeTrigger},
      /*disabled_features=*/{});
  profile()->GetPrefs()->SetBoolean(prefs::kGlicShakeTriggerEnabled, true);

  EXPECT_FALSE(trigger_->trigger_region_capture_called());
  SimulateMouseShake();
  EXPECT_TRUE(trigger_->trigger_region_capture_called());
}

TEST_F(ShakeTriggerTest, ShakeTriggerDisabledByFeatureFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlicShakeTrigger);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicShakeTriggerEnabled, true);

  EXPECT_FALSE(trigger_->trigger_region_capture_called());
  SimulateMouseShake();
  EXPECT_FALSE(trigger_->trigger_region_capture_called());
}

TEST_F(ShakeTriggerTest, ShakeTriggerDisabledByPref) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kGlicShakeTrigger},
      /*disabled_features=*/{});
  profile()->GetPrefs()->SetBoolean(prefs::kGlicShakeTriggerEnabled, false);

  EXPECT_FALSE(trigger_->trigger_region_capture_called());
  SimulateMouseShake();
  EXPECT_FALSE(trigger_->trigger_region_capture_called());
}

TEST_F(ShakeTriggerTest, ShakeTriggerDisabledWhenSidePanelClosed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kGlicShakeTrigger},
      /*disabled_features=*/{});
  profile()->GetPrefs()->SetBoolean(prefs::kGlicShakeTriggerEnabled, true);
  side_panel_open_ = false;

  EXPECT_FALSE(trigger_->IsEnabled());

  EXPECT_FALSE(trigger_->trigger_region_capture_called());
  SimulateMouseShake();
  EXPECT_FALSE(trigger_->trigger_region_capture_called());
}

TEST_F(ShakeTriggerTest,
       ShakeTriggerSucceedsWhenSidePanelClosedIfOnlyOnSidePanelFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::kGlicShakeTrigger,
                             {{"only_on_side_panel", "false"}}}},
      /*disabled_features=*/{});
  profile()->GetPrefs()->SetBoolean(prefs::kGlicShakeTriggerEnabled, true);
  side_panel_open_ = false;

  EXPECT_TRUE(trigger_->IsEnabled());

  EXPECT_FALSE(trigger_->trigger_region_capture_called());
  SimulateMouseShake();
  EXPECT_TRUE(trigger_->trigger_region_capture_called());
}

TEST_F(ShakeTriggerTest, ShakeTriggerSucceedsWhenSidePanelOpen) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kGlicShakeTrigger},
      /*disabled_features=*/{});
  profile()->GetPrefs()->SetBoolean(prefs::kGlicShakeTriggerEnabled, true);
  side_panel_open_ = true;

  EXPECT_TRUE(trigger_->IsEnabled());

  EXPECT_FALSE(trigger_->trigger_region_capture_called());
  SimulateMouseShake();
  EXPECT_TRUE(trigger_->trigger_region_capture_called());
}

TEST_F(ShakeTriggerTest, MouseDownResetsShake) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicShakeTrigger);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicShakeTriggerEnabled, true);

  SimulateMouseMove(0.0f, 0.0f);
  SimulateMouseMove(20.0f, 0.0f);
  SimulateMouseMove(0.0f, 0.0f);
  SimulateMouseMove(20.0f, 0.0f);
  SimulateMouseMove(0.0f, 0.0f);
  SimulateMouseEvent(blink::WebInputEvent::Type::kMouseDown);
  SimulateMouseMove(20.0f, 0.0f);

  EXPECT_FALSE(trigger_->trigger_region_capture_called());
}

TEST_F(ShakeTriggerTest, UsesCurrentTime) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicShakeTrigger);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicShakeTriggerEnabled, true);

  // The events all carry the same timestamp, but seconds pass between them.
  for (float x : {0.0f, 20.0f, 0.0f, 20.0f, 0.0f, 20.0f}) {
    Wait(base::Seconds(2));
    SimulateMouseMove(x, 0.0f);
  }

  EXPECT_FALSE(trigger_->trigger_region_capture_called());
}

class ShakeTriggerHarnessTest : public ShakeTriggerTest {
 public:
  ShakeTriggerHarnessTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicShakeTrigger, {{"cooldown", "1s"},
                                      {"ignore_after_click", "300ms"},
                                      {"pause_threshold", "500ms"},
                                      {"ignore_after_pause", "100ms"}});
  }

  void SetUp() override {
    ShakeTriggerTest::SetUp();
    profile()->GetPrefs()->SetBoolean(prefs::kGlicShakeTriggerEnabled, true);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ShakeTriggerHarnessTest, CooldownAfterTrigger) {
  SimulateMouseShake();
  EXPECT_EQ(trigger_->trigger_region_capture_count(), 1);

  // Within the 1s cooldown.
  Wait(base::Milliseconds(450));
  SimulateMouseShake();
  EXPECT_EQ(trigger_->trigger_region_capture_count(), 1);

  Wait(base::Milliseconds(450));
  SimulateMouseShake();
  EXPECT_EQ(trigger_->trigger_region_capture_count(), 2);
}

TEST_F(ShakeTriggerHarnessTest, IgnoresMovesRightAfterClick) {
  SimulateMouseEvent(blink::WebInputEvent::Type::kMouseUp);
  // Within 300ms of the mouse up.
  SimulateMouseShake();
  EXPECT_FALSE(trigger_->trigger_region_capture_called());

  Wait(base::Milliseconds(250));
  SimulateMouseShake();
  EXPECT_TRUE(trigger_->trigger_region_capture_called());
}

TEST_F(ShakeTriggerHarnessTest, IgnoresMovesRightAfterIdle) {
  SimulateMouseMove(0.0f, 0.0f);
  Wait(base::Milliseconds(500));
  // Within 100ms of moving again.
  SimulateMouseShake();
  EXPECT_FALSE(trigger_->trigger_region_capture_called());

  Wait(base::Milliseconds(100));
  SimulateMouseShake();
  EXPECT_TRUE(trigger_->trigger_region_capture_called());
}

TEST_F(ShakeTriggerHarnessTest, DragIsNotAShake) {
  for (float x : {0.0f, 20.0f, 0.0f, 20.0f, 0.0f, 20.0f}) {
    SimulateMouseMove(x, 0.0f, blink::WebInputEvent::kLeftButtonDown);
  }
  EXPECT_FALSE(trigger_->trigger_region_capture_called());
}

TEST_F(ShakeTriggerTest, CountsWithoutTriggering) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kGlicShakeTrigger,
      {{"should_trigger", "false"}, {"should_count", "true"}});
  RecreateTrigger();

  EXPECT_FALSE(trigger_->IsEnabled());
  EXPECT_TRUE(trigger_->ShouldObserveMouseMoves());
  SimulateMouseShake();
  EXPECT_FALSE(trigger_->trigger_region_capture_called());
}

TEST_F(ShakeTriggerTest, NoMouseMovesNeededWhenOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlicShakeTrigger);
  RecreateTrigger();

  EXPECT_FALSE(trigger_->ShouldObserveMouseMoves());
}

TEST_F(ShakeTriggerHarnessTest, NoWiggleMetricsWithoutCountOnly) {
  base::HistogramTester histogram_tester;
  SimulateMouseShake();
  EXPECT_TRUE(trigger_->trigger_region_capture_called());
  histogram_tester.ExpectTotalCount("Glic.Wiggle.Detected", 0);
  histogram_tester.ExpectTotalCount("Glic.Wiggle.ActiveMouseMinutes", 0);
}

class ShakeTriggerCountOnlyTest : public ShakeTriggerTest {
 public:
  ShakeTriggerCountOnlyTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicShakeTrigger,
        {{"should_trigger", "false"}, {"should_count", "true"}});
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ShakeTriggerCountOnlyTest, RecordsDetection) {
  SimulateMouseShake();
  EXPECT_FALSE(trigger_->trigger_region_capture_called());
  histogram_tester_.ExpectUniqueSample("Glic.Wiggle.Detected",
                                       WiggleDetectorType::kLegacyShake, 1);
  histogram_tester_.ExpectUniqueSample("Glic.Consumer.Wiggle.Detected",
                                       WiggleDetectorType::kLegacyShake, 1);
}

TEST_F(ShakeTriggerCountOnlyTest, RecordsActiveMouseMinutes) {
  SimulateMouseMove(0.0f, 0.0f);
  SimulateMouseMove(1.0f, 0.0f);
  histogram_tester_.ExpectUniqueSample("Glic.Wiggle.ActiveMouseMinutes", true,
                                       1);

  Wait(base::Seconds(30));
  SimulateMouseMove(2.0f, 0.0f);
  histogram_tester_.ExpectTotalCount("Glic.Wiggle.ActiveMouseMinutes", 1);

  Wait(base::Seconds(30));
  SimulateMouseMove(3.0f, 0.0f);
  histogram_tester_.ExpectTotalCount("Glic.Wiggle.ActiveMouseMinutes", 2);
}

TEST_F(ShakeTriggerCountOnlyTest, ActiveMouseMinutesSharedAcrossTabs) {
  std::unique_ptr<content::WebContents> second_web_contents =
      CreateTestWebContents();
  TestShakeTrigger second_trigger(second_web_contents.get(), *this);

  SimulateMouseMove(0.0f, 0.0f);
  histogram_tester_.ExpectTotalCount("Glic.Wiggle.ActiveMouseMinutes", 1);

  Wait(base::Seconds(30));
  blink::WebMouseEvent event(
      blink::WebInputEvent::Type::kMouseMove,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.SetPositionInWidget(1.0f, 0.0f);
  second_trigger.OnInputEvent(event);
  histogram_tester_.ExpectTotalCount("Glic.Wiggle.ActiveMouseMinutes", 1);

  Wait(base::Seconds(30));
  second_trigger.OnInputEvent(event);
  histogram_tester_.ExpectTotalCount("Glic.Wiggle.ActiveMouseMinutes", 2);
}

TEST_F(ShakeTriggerCountOnlyTest, DetectionFollowedUp) {
  SimulateMouseShake();
  task_environment()->FastForwardBy(base::Seconds(5));
  trigger_->OnFollowUpAction();
  histogram_tester_.ExpectUniqueSample("Glic.Wiggle.FollowedUp.LegacyShake",
                                       true, 1);

  task_environment()->FastForwardBy(base::Seconds(10));
  histogram_tester_.ExpectTotalCount("Glic.Wiggle.FollowedUp.LegacyShake", 1);
}

TEST_F(ShakeTriggerCountOnlyTest, DetectionNotFollowedUp) {
  SimulateMouseShake();
  task_environment()->FastForwardBy(base::Seconds(10));
  histogram_tester_.ExpectUniqueSample("Glic.Wiggle.FollowedUp.LegacyShake",
                                       false, 1);

  trigger_->OnFollowUpAction();
  histogram_tester_.ExpectTotalCount("Glic.Wiggle.FollowedUp.LegacyShake", 1);
}

TEST_F(ShakeTriggerCountOnlyTest, OverlappingFollowUpWindows) {
  SimulateMouseShake();
  Wait(base::Seconds(4));
  SimulateMouseShake();

  // Only the first window has ended.
  Wait(base::Seconds(7));
  histogram_tester_.ExpectUniqueSample("Glic.Wiggle.FollowedUp.LegacyShake",
                                       false, 1);

  trigger_->OnFollowUpAction();
  histogram_tester_.ExpectBucketCount("Glic.Wiggle.FollowedUp.LegacyShake",
                                      true, 1);

  Wait(base::Seconds(10));
  histogram_tester_.ExpectTotalCount("Glic.Wiggle.FollowedUp.LegacyShake", 2);
}

TEST_F(ShakeTriggerCountOnlyTest, PendingDetectionRecordedOnDestruction) {
  SimulateMouseShake();
  trigger_.reset();
  histogram_tester_.ExpectUniqueSample("Glic.Wiggle.FollowedUp.LegacyShake",
                                       false, 1);
}

// With the trigger on, its own region capture would count as a follow-up.
TEST_F(ShakeTriggerTest, NoFollowUpWhileTriggerEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(features::kGlicShakeTrigger,
                                                  {{"should_count", "true"}});
  RecreateTrigger();
  profile()->GetPrefs()->SetBoolean(prefs::kGlicShakeTriggerEnabled, true);
  base::HistogramTester histogram_tester;

  SimulateMouseShake();
  EXPECT_TRUE(trigger_->trigger_region_capture_called());
  histogram_tester.ExpectUniqueSample("Glic.Wiggle.Detected",
                                      WiggleDetectorType::kLegacyShake, 1);

  trigger_->OnFollowUpAction();
  Wait(base::Seconds(10));
  histogram_tester.ExpectTotalCount("Glic.Wiggle.FollowedUp.LegacyShake", 0);
}

}  // namespace glic
