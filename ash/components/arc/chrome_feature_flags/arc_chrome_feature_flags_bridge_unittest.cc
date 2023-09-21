// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/chrome_feature_flags/arc_chrome_feature_flags_bridge.h"

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/fake_chrome_feature_flags_instance.h"
#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcChromeFeatureFlagsBridgeTest : public testing::Test {
 protected:
  ArcChromeFeatureFlagsBridgeTest()
      : bridge_(ArcChromeFeatureFlagsBridge::GetForBrowserContextForTesting(
            &context_)) {}
  ArcChromeFeatureFlagsBridgeTest(const ArcChromeFeatureFlagsBridgeTest&) =
      delete;
  ArcChromeFeatureFlagsBridgeTest& operator=(
      const ArcChromeFeatureFlagsBridgeTest&) = delete;
  ~ArcChromeFeatureFlagsBridgeTest() override = default;

  void TearDown() override {
    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->chrome_feature_flags()
        ->CloseInstance(&instance_);
    bridge_->Shutdown();
  }

  void Connect() {
    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->chrome_feature_flags()
        ->SetInstance(&instance_);
  }

  ArcChromeFeatureFlagsBridge* bridge() { return bridge_; }
  FakeChromeFeatureFlagsInstance* instance() { return &instance_; }
  base::test::ScopedFeatureList* scoped_feature_list() {
    return &scoped_feature_list_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  user_prefs::TestBrowserContextWithPrefs context_;
  FakeChromeFeatureFlagsInstance instance_;
  base::test::ScopedFeatureList scoped_feature_list_;
  const raw_ptr<ArcChromeFeatureFlagsBridge, ExperimentalAsh> bridge_;
};

TEST_F(ArcChromeFeatureFlagsBridgeTest, ConstructDestruct) {
  Connect();
  EXPECT_NE(nullptr, bridge());
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyQsRevamp_Enabled) {
  scoped_feature_list()->InitAndEnableFeature(ash::features::kQsRevamp);
  Connect();
  EXPECT_TRUE(instance()->flags_called_value()->qs_revamp);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyQsRevamp_Disabled) {
  scoped_feature_list()->InitAndDisableFeature(ash::features::kQsRevamp);
  Connect();
  EXPECT_FALSE(instance()->flags_called_value()->qs_revamp);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyJelly_Enabled) {
  scoped_feature_list()->InitAndEnableFeature(chromeos::features::kJelly);
  Connect();
  EXPECT_TRUE(instance()->flags_called_value()->jelly_colors);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyJelly_Disabled) {
  scoped_feature_list()->InitAndDisableFeature(chromeos::features::kJelly);
  Connect();
  EXPECT_FALSE(instance()->flags_called_value()->jelly_colors);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyTouchscreenEmulation_Enabled) {
  scoped_feature_list()->InitAndEnableFeature(kTouchscreenEmulation);
  Connect();
  EXPECT_TRUE(instance()->flags_called_value()->touchscreen_emulation);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyTouchscreenEmulation_Disabled) {
  scoped_feature_list()->InitAndDisableFeature(kTouchscreenEmulation);
  Connect();
  EXPECT_FALSE(instance()->flags_called_value()->touchscreen_emulation);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest,
       NotifyTrackpadScrollTouchscreenEmulation_Enabled) {
  scoped_feature_list()->InitAndEnableFeature(
      kTrackpadScrollTouchscreenEmulation);
  Connect();
  EXPECT_TRUE(
      instance()->flags_called_value()->trackpad_scroll_touchscreen_emulation);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest,
       NotifyTrackpadScrollTouchscreenEmulation_Disabled) {
  scoped_feature_list()->InitAndDisableFeature(
      kTrackpadScrollTouchscreenEmulation);
  Connect();
  EXPECT_FALSE(
      instance()->flags_called_value()->trackpad_scroll_touchscreen_emulation);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyRoundedWindowCompat_Enabled) {
  scoped_feature_list()->InitAndEnableFeature(kRoundedWindowCompat);
  Connect();
  EXPECT_EQ(instance()->flags_called_value()->rounded_window_compat_strategy,
            mojom::RoundedWindowCompatStrategy::kLeftRightBottomGesture);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest,
       NotifyRoundedWindowCompat_EnabledBottomOnly) {
  scoped_feature_list()->InitAndEnableFeatureWithParameters(
      kRoundedWindowCompat, {{kRoundedWindowCompatStrategy,
                              kRoundedWindowCompatStrategy_BottomOnlyGesture}});
  Connect();
  EXPECT_EQ(instance()->flags_called_value()->rounded_window_compat_strategy,
            mojom::RoundedWindowCompatStrategy::kBottomOnlyGesture);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest,
       NotifyRoundedWindowCompat_EnabledLeftRightBottom) {
  scoped_feature_list()->InitAndEnableFeatureWithParameters(
      kRoundedWindowCompat,
      {{kRoundedWindowCompatStrategy,
        kRoundedWindowCompatStrategy_LeftRightBottomGesture}});
  Connect();
  EXPECT_EQ(instance()->flags_called_value()->rounded_window_compat_strategy,
            mojom::RoundedWindowCompatStrategy::kLeftRightBottomGesture);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyRoundedWindowCompat_Disabled) {
  scoped_feature_list()->InitAndDisableFeature(kRoundedWindowCompat);
  Connect();
  EXPECT_EQ(instance()->flags_called_value()->rounded_window_compat_strategy,
            mojom::RoundedWindowCompatStrategy::kDisabled);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyRoundedWindows_Enabled) {
  // Currently we need to flip Jelly flag to enable the rounded windows flag.
  scoped_feature_list()->InitWithFeaturesAndParameters(
      {{chromeos::features::kJelly, {}},
       {chromeos::features::kRoundedWindows,
        {{chromeos::features::kRoundedWindowsRadius, "8"}}}},
      {});
  Connect();
  EXPECT_EQ(instance()->flags_called_value()->rounded_window_radius, 8);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyRoundedWindows_Disabled) {
  scoped_feature_list()->InitAndDisableFeature(
      chromeos::features::kRoundedWindows);
  Connect();
  EXPECT_EQ(instance()->flags_called_value()->rounded_window_radius, 0);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyXdgMode_Enabled) {
  scoped_feature_list()->InitAndEnableFeature(kXdgMode);
  Connect();
  EXPECT_TRUE(instance()->flags_called_value()->xdg_mode);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyXdgMode_Disabled) {
  scoped_feature_list()->InitAndDisableFeature(kXdgMode);
  Connect();
  EXPECT_FALSE(instance()->flags_called_value()->xdg_mode);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyPipDoubleTapToResize_Enabled) {
  scoped_feature_list()->InitAndEnableFeature(
      ash::features::kPipDoubleTapToResize);
  Connect();
  EXPECT_TRUE(instance()->flags_called_value()->enable_pip_double_tap);
}

TEST_F(ArcChromeFeatureFlagsBridgeTest, NotifyPipDoubleTapToResize_Disabled) {
  scoped_feature_list()->InitAndDisableFeature(
      ash::features::kPipDoubleTapToResize);
  Connect();
  EXPECT_FALSE(instance()->flags_called_value()->enable_pip_double_tap);
}

}  // namespace
}  // namespace arc
