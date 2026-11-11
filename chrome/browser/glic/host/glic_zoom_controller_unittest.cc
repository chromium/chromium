// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_zoom_controller.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic_pwc_policy_delegate.h"
#include "chrome/browser/glic/host/glic_webui.mojom.h"
#include "chrome/browser/pwc/privileged_web_contents.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/page_zoom.h"

namespace glic {

class GlicZoomControllerTest : public testing::Test {
 public:
  GlicZoomControllerTest() : profile_(std::make_unique<TestingProfile>()) {}

  void SetUp() override {
    pwc_ = CreatePwc();
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        pwc_->web_contents(), GURL("https://gemini.google.com/glic"));
    controller_ = std::make_unique<GlicZoomController>(pwc_.get(), prefs());
  }

  void TearDown() override {
    controller_.reset();
    pwc_.reset();
  }

  std::unique_ptr<pwc::PrivilegedWebContents> CreatePwc() {
    return pwc::PrivilegedWebContents::Create(
        pwc::PrivilegedComponent::kGlic, profile_.get(),
        std::make_unique<GlicPwcPolicyDelegate>(profile_.get()));
  }

  PrefService* prefs() { return profile_->GetPrefs(); }
  pwc::PrivilegedWebContents* pwc() { return pwc_.get(); }
  content::WebContents* web_contents() { return pwc_->web_contents(); }
  GlicZoomController& controller() { return *controller_; }
  TestingProfile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<pwc::PrivilegedWebContents> pwc_;
  std::unique_ptr<GlicZoomController> controller_;
};

TEST_F(GlicZoomControllerTest, DefaultZoomIs100Percent) {
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 1.0);
  double zoom_level = content::HostZoomMap::GetZoomLevel(web_contents());
  EXPECT_DOUBLE_EQ(blink::ZoomLevelToZoomFactor(zoom_level), 1.0);
}

TEST_F(GlicZoomControllerTest, ZoomInStepsThroughFactorsAndClamps) {
  base::HistogramTester histograms;
  base::UserActionTester user_actions;

  // Initial: 1.0
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 1.0);

  // 1.0 -> 1.1
  controller().Zoom(mojom::ZoomAction::kZoomIn, ZoomSource::kHotkey);
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 1.1);
  EXPECT_EQ(prefs()->GetInteger(prefs::kGlicZoomLevel), 110);
  EXPECT_DOUBLE_EQ(blink::ZoomLevelToZoomFactor(
                       content::HostZoomMap::GetZoomLevel(web_contents())),
                   1.1);

  // 1.1 -> 1.25
  controller().Zoom(mojom::ZoomAction::kZoomIn, ZoomSource::kHotkey);
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 1.25);
  EXPECT_EQ(prefs()->GetInteger(prefs::kGlicZoomLevel), 125);

  // 1.25 -> 1.5
  controller().Zoom(mojom::ZoomAction::kZoomIn, ZoomSource::kHotkey);
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 1.5);
  EXPECT_EQ(prefs()->GetInteger(prefs::kGlicZoomLevel), 150);

  // 1.5 -> 1.75
  controller().Zoom(mojom::ZoomAction::kZoomIn, ZoomSource::kHotkey);
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 1.75);
  EXPECT_EQ(prefs()->GetInteger(prefs::kGlicZoomLevel), 175);

  // 1.75 -> 2.0
  controller().Zoom(mojom::ZoomAction::kZoomIn, ZoomSource::kHotkey);
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 2.0);
  EXPECT_EQ(prefs()->GetInteger(prefs::kGlicZoomLevel), 200);

  // At 2.0: should clamp and record ZoomInAtMax
  controller().Zoom(mojom::ZoomAction::kZoomIn, ZoomSource::kHotkey);
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 2.0);
  EXPECT_EQ(prefs()->GetInteger(prefs::kGlicZoomLevel), 200);

  EXPECT_EQ(user_actions.GetActionCount("Glic.ZoomIn"), 5);
  EXPECT_EQ(user_actions.GetActionCount("Glic.ZoomInAtMax"), 1);
  histograms.ExpectBucketCount("Glic.ZoomAction", GlicZoomAction::kZoomIn, 5);
  histograms.ExpectBucketCount("Glic.Consumer.ZoomAction",
                               GlicZoomAction::kZoomIn, 5);
  histograms.ExpectTotalCount("Glic.Enterprise.ZoomAction", 0);
  histograms.ExpectBucketCount("Glic.ZoomAction", GlicZoomAction::kZoomInAtMax,
                               1);
  histograms.ExpectBucketCount("Glic.Consumer.ZoomAction",
                               GlicZoomAction::kZoomInAtMax, 1);
  histograms.ExpectTotalCount("Glic.Enterprise.ZoomAction", 0);
  histograms.ExpectBucketCount("Glic.ZoomAction.Hotkey",
                               GlicZoomAction::kZoomIn, 5);
  histograms.ExpectBucketCount("Glic.Consumer.ZoomAction.Hotkey",
                               GlicZoomAction::kZoomIn, 5);
  histograms.ExpectTotalCount("Glic.Enterprise.ZoomAction.Hotkey", 0);
}

TEST_F(GlicZoomControllerTest, ZoomOutStepsThroughFactorsAndClamps) {
  base::HistogramTester histograms;
  base::UserActionTester user_actions;

  // Set initial zoom to 200%
  prefs()->SetInteger(prefs::kGlicZoomLevel, 200);
  controller().ApplyZoom();
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 2.0);

  // 2.0 -> 1.75
  controller().Zoom(mojom::ZoomAction::kZoomOut, ZoomSource::kScroll);
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 1.75);
  EXPECT_EQ(prefs()->GetInteger(prefs::kGlicZoomLevel), 175);

  // 1.75 -> 1.5
  controller().Zoom(mojom::ZoomAction::kZoomOut, ZoomSource::kScroll);
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 1.5);

  // 1.5 -> 1.25
  controller().Zoom(mojom::ZoomAction::kZoomOut, ZoomSource::kScroll);
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 1.25);

  // 1.25 -> 1.1
  controller().Zoom(mojom::ZoomAction::kZoomOut, ZoomSource::kScroll);
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 1.1);

  // 1.1 -> 1.0
  controller().Zoom(mojom::ZoomAction::kZoomOut, ZoomSource::kScroll);
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 1.0);
  EXPECT_EQ(prefs()->GetInteger(prefs::kGlicZoomLevel), 100);

  // At 1.0: should clamp and record ZoomOutAtMin
  controller().Zoom(mojom::ZoomAction::kZoomOut, ZoomSource::kScroll);
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 1.0);
  EXPECT_EQ(prefs()->GetInteger(prefs::kGlicZoomLevel), 100);

  EXPECT_EQ(user_actions.GetActionCount("Glic.ZoomOut"), 5);
  EXPECT_EQ(user_actions.GetActionCount("Glic.ZoomOutAtMin"), 1);
  histograms.ExpectBucketCount("Glic.ZoomAction", GlicZoomAction::kZoomOut, 5);
  histograms.ExpectBucketCount("Glic.Consumer.ZoomAction",
                               GlicZoomAction::kZoomOut, 5);
  histograms.ExpectTotalCount("Glic.Enterprise.ZoomAction", 0);
  histograms.ExpectBucketCount("Glic.ZoomAction", GlicZoomAction::kZoomOutAtMin,
                               1);
  histograms.ExpectBucketCount("Glic.Consumer.ZoomAction",
                               GlicZoomAction::kZoomOutAtMin, 1);
  histograms.ExpectTotalCount("Glic.Enterprise.ZoomAction", 0);
  histograms.ExpectBucketCount("Glic.ZoomAction.Scroll",
                               GlicZoomAction::kZoomOut, 5);
  histograms.ExpectBucketCount("Glic.Consumer.ZoomAction.Scroll",
                               GlicZoomAction::kZoomOut, 5);
  histograms.ExpectTotalCount("Glic.Enterprise.ZoomAction.Scroll", 0);
}

TEST_F(GlicZoomControllerTest, ZoomReset) {
  base::HistogramTester histograms;
  base::UserActionTester user_actions;

  prefs()->SetInteger(prefs::kGlicZoomLevel, 150);
  controller().ApplyZoom();
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 1.5);

  controller().Zoom(mojom::ZoomAction::kReset, ZoomSource::kHotkeyWithShift);
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 1.0);
  EXPECT_EQ(prefs()->GetInteger(prefs::kGlicZoomLevel), 100);
  EXPECT_DOUBLE_EQ(blink::ZoomLevelToZoomFactor(
                       content::HostZoomMap::GetZoomLevel(web_contents())),
                   1.0);

  EXPECT_EQ(user_actions.GetActionCount("Glic.ZoomReset"), 1);
  histograms.ExpectBucketCount("Glic.ZoomAction", GlicZoomAction::kReset, 1);
  histograms.ExpectBucketCount("Glic.Consumer.ZoomAction",
                               GlicZoomAction::kReset, 1);
  histograms.ExpectTotalCount("Glic.Enterprise.ZoomAction", 0);
  histograms.ExpectBucketCount("Glic.ZoomAction.HotkeyWithShift",
                               GlicZoomAction::kReset, 1);
  histograms.ExpectBucketCount("Glic.Consumer.ZoomAction.HotkeyWithShift",
                               GlicZoomAction::kReset, 1);
  histograms.ExpectTotalCount("Glic.Enterprise.ZoomAction.HotkeyWithShift", 0);
}

TEST_F(GlicZoomControllerTest, ZoomIsIsolatedFromHost) {
  auto* rfh = web_contents()->GetPrimaryMainFrame();
  auto* zoom_map = content::HostZoomMap::Get(rfh->GetSiteInstance());

  // Verify that gemini.google.com host zoom is currently 0.0 (default 100%)
  EXPECT_DOUBLE_EQ(
      zoom_map->GetZoomLevelForHostAndScheme("https", "gemini.google.com"),
      0.0);

  // Zoom Glic in to 1.5
  controller().Zoom(mojom::ZoomAction::kZoomIn, ZoomSource::kHotkey);
  controller().Zoom(mojom::ZoomAction::kZoomIn, ZoomSource::kHotkey);
  controller().Zoom(mojom::ZoomAction::kZoomIn, ZoomSource::kHotkey);
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 1.5);

  // Verify that the guest PrivilegedWebContents exposes a zoom override.
  std::optional<double> override_zoom =
      pwc()->GetOverrideZoomLevel(web_contents());
  ASSERT_TRUE(override_zoom.has_value());
  EXPECT_DOUBLE_EQ(blink::ZoomLevelToZoomFactor(*override_zoom), 1.5);
  EXPECT_DOUBLE_EQ(blink::ZoomLevelToZoomFactor(
                       content::HostZoomMap::GetZoomLevel(web_contents())),
                   1.5);

  // Verify that gemini.google.com host zoom is STILL 0.0 (unaffected!)
  EXPECT_DOUBLE_EQ(
      zoom_map->GetZoomLevelForHostAndScheme("https", "gemini.google.com"),
      0.0);

  // Conversely, zooming gemini.google.com in a regular browser tab (200%) must
  // not affect Glic—even when Glic is reset back to 100%.
  std::unique_ptr<content::WebContents> tab_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile()));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      tab_contents.get(), GURL("https://gemini.google.com/app"));
  zoom_map->SetZoomLevelForHost("gemini.google.com",
                                blink::ZoomFactorToZoomLevel(2.0));
  EXPECT_DOUBLE_EQ(blink::ZoomLevelToZoomFactor(
                       content::HostZoomMap::GetZoomLevel(tab_contents.get())),
                   2.0);
  EXPECT_DOUBLE_EQ(blink::ZoomLevelToZoomFactor(
                       content::HostZoomMap::GetZoomLevel(web_contents())),
                   1.5);

  controller().Zoom(mojom::ZoomAction::kReset, ZoomSource::kHotkey);
  EXPECT_DOUBLE_EQ(blink::ZoomLevelToZoomFactor(
                       content::HostZoomMap::GetZoomLevel(web_contents())),
                   1.0);
  EXPECT_DOUBLE_EQ(blink::ZoomLevelToZoomFactor(
                       content::HostZoomMap::GetZoomLevel(tab_contents.get())),
                   2.0);
}

TEST_F(GlicZoomControllerTest, ZoomReappliedOnNavigation) {
  // Set zoom to 1.25
  controller().Zoom(mojom::ZoomAction::kZoomIn, ZoomSource::kHotkey);
  controller().Zoom(mojom::ZoomAction::kZoomIn, ZoomSource::kHotkey);
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 1.25);

  // Simulate navigation to another URL or reload
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://gemini.google.com/glic/reload"));

  // After navigation commits, the PrivilegedWebContents zoom override remains
  // active.
  ASSERT_TRUE(pwc()->GetOverrideZoomLevel(web_contents()).has_value());
  EXPECT_DOUBLE_EQ(blink::ZoomLevelToZoomFactor(
                       content::HostZoomMap::GetZoomLevel(web_contents())),
                   1.25);
}

TEST_F(GlicZoomControllerTest,
       ReadsPersistedZoomFromPrefOnCreationAndAppliesBeforeFirstNavigation) {
  // Simulate a previous session having saved zoom level to 150% in prefs.
  prefs()->SetInteger(prefs::kGlicZoomLevel, 150);

  std::unique_ptr<pwc::PrivilegedWebContents> new_pwc = CreatePwc();

  // Create controller BEFORE the first navigation (matching production
  // GlicNoWebviewContentsManager construction order).
  GlicZoomController new_controller(new_pwc.get(), prefs());
  EXPECT_DOUBLE_EQ(new_controller.GetCurrentZoomFactor(), 1.5);

  // Override zoom is already populated before any navigation starts.
  ASSERT_TRUE(
      new_pwc->GetOverrideZoomLevel(new_pwc->web_contents()).has_value());
  EXPECT_DOUBLE_EQ(
      blink::ZoomLevelToZoomFactor(
          content::HostZoomMap::GetZoomLevel(new_pwc->web_contents())),
      1.5);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      new_pwc->web_contents(), GURL("https://gemini.google.com/glic"));
  EXPECT_DOUBLE_EQ(
      blink::ZoomLevelToZoomFactor(
          content::HostZoomMap::GetZoomLevel(new_pwc->web_contents())),
      1.5);

  // Zooming further from this restored state updates the pref to 175%.
  new_controller.Zoom(mojom::ZoomAction::kZoomIn, ZoomSource::kHotkey);
  EXPECT_DOUBLE_EQ(new_controller.GetCurrentZoomFactor(), 1.75);
  EXPECT_EQ(prefs()->GetInteger(prefs::kGlicZoomLevel), 175);
}

TEST_F(GlicZoomControllerTest, InvalidPrefClamped) {
  // Test out-of-range pref values (e.g. corrupted prefs).
  prefs()->SetInteger(prefs::kGlicZoomLevel, 50);
  GlicZoomController controller_low(nullptr, prefs());
  EXPECT_DOUBLE_EQ(controller_low.GetCurrentZoomFactor(), 1.0);

  prefs()->SetInteger(prefs::kGlicZoomLevel, 500);
  GlicZoomController controller_high(nullptr, prefs());
  EXPECT_DOUBLE_EQ(controller_high.GetCurrentZoomFactor(), 2.0);

  prefs()->SetInteger(prefs::kGlicZoomLevel, 0);
  GlicZoomController controller_zero(nullptr, prefs());
  EXPECT_DOUBLE_EQ(controller_zero.GetCurrentZoomFactor(), 1.0);

  prefs()->SetInteger(prefs::kGlicZoomLevel, -10);
  GlicZoomController controller_negative(nullptr, prefs());
  EXPECT_DOUBLE_EQ(controller_negative.GetCurrentZoomFactor(), 1.0);
}

TEST_F(GlicZoomControllerTest, UnalignedPrefStepping) {
  // Test stepping from unaligned zoom percentages (e.g. 199% and 101%).
  prefs()->SetInteger(prefs::kGlicZoomLevel, 199);
  controller().ApplyZoom();
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 1.99);

  // Zooming in from 199% must step to 200%.
  controller().Zoom(mojom::ZoomAction::kZoomIn, ZoomSource::kHotkey);
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 2.0);
  EXPECT_EQ(prefs()->GetInteger(prefs::kGlicZoomLevel), 200);

  // Zooming out from 101% must step to 100%.
  prefs()->SetInteger(prefs::kGlicZoomLevel, 101);
  controller().ApplyZoom();
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 1.01);

  controller().Zoom(mojom::ZoomAction::kZoomOut, ZoomSource::kHotkey);
  EXPECT_DOUBLE_EQ(controller().GetCurrentZoomFactor(), 1.0);
  EXPECT_EQ(prefs()->GetInteger(prefs::kGlicZoomLevel), 100);
}

TEST_F(GlicZoomControllerTest, NotifiesCallbackOnZoomChange) {
  int change_count = 0;
  std::unique_ptr<pwc::PrivilegedWebContents> test_pwc = CreatePwc();
  GlicZoomController test_controller(
      test_pwc.get(), prefs(),
      base::BindRepeating([](int* count) { (*count)++; },
                          base::Unretained(&change_count)));

  // Initial factor is 1.0 (100%).
  // Zooming in steps to 1.1 -> callback should fire.
  test_controller.Zoom(mojom::ZoomAction::kZoomIn, ZoomSource::kHotkey);
  EXPECT_EQ(change_count, 1);

  // Zooming in again steps to 1.25 -> callback should fire.
  test_controller.Zoom(mojom::ZoomAction::kZoomIn, ZoomSource::kHotkey);
  EXPECT_EQ(change_count, 2);

  // Zoom reset back to 1.0 -> factor changes from 1.25 to 1.0, callback fires.
  test_controller.Zoom(mojom::ZoomAction::kReset, ZoomSource::kHotkey);
  EXPECT_EQ(change_count, 3);

  // Redundant reset when already at 1.0 -> factor doesn't change, callback does
  // NOT fire.
  test_controller.Zoom(mojom::ZoomAction::kReset, ZoomSource::kHotkey);
  EXPECT_EQ(change_count, 3);

  // Zoom out while already at 1.0 (min) -> clamps at min, no change, callback
  // does NOT fire.
  test_controller.Zoom(mojom::ZoomAction::kZoomOut, ZoomSource::kHotkey);
  EXPECT_EQ(change_count, 3);
}

}  // namespace glic
