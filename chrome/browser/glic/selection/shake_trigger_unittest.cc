// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/shake_trigger.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

namespace glic {

namespace {

class TestShakeTrigger : public ShakeTrigger {
 public:
  using ShakeTrigger::ShakeTrigger;

  bool trigger_region_capture_called() const {
    return trigger_region_capture_called_;
  }

 protected:
  void TriggerRegionCapture() override {
    trigger_region_capture_called_ = true;
    ShakeTrigger::TriggerRegionCapture();
  }

 private:
  bool trigger_region_capture_called_ = false;
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
  void SimulateMouseMove(float x, float y) {
    blink::WebMouseEvent event(
        blink::WebInputEvent::Type::kMouseMove,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    event.SetPositionInWidget(x, y);
    trigger_->OnInputEvent(event);
  }

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

TEST_F(ShakeTriggerTest, ContinuousMoveDoesNotTriggerShake) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kGlicShakeTrigger},
      /*disabled_features=*/{});
  profile()->GetPrefs()->SetBoolean(prefs::kGlicShakeTriggerEnabled, true);

  // Move continuously in the positive X direction.
  SimulateMouseMove(0.0f, 0.0f);
  SimulateMouseMove(20.0f, 0.0f);
  SimulateMouseMove(40.0f, 0.0f);
  SimulateMouseMove(60.0f, 0.0f);
  SimulateMouseMove(80.0f, 0.0f);
  SimulateMouseMove(100.0f, 0.0f);

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

}  // namespace glic
