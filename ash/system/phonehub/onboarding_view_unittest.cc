// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/onboarding_view.h"

#include <memory>

#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/components/phonehub/fake_onboarding_ui_tracker.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {

class OnboardingViewTest : public AshTestBase {
 public:
  OnboardingViewTest() = default;
  ~OnboardingViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(chromeos::features::kPhoneHub);
    AshTestBase::SetUp();

    onboarding_view_ =
        std::make_unique<OnboardingView>(&fake_onboarding_ui_tracker_);
  }

  void TearDown() override {
    onboarding_view_.reset();
    AshTestBase::TearDown();
  }

 protected:
  OnboardingView* onboarding_view() { return onboarding_view_.get(); }

  views::Button* get_started_button() {
    return static_cast<views::Button*>(onboarding_view_->GetViewByID(
        PhoneHubViewID::kOnboardingGetStartedButton));
  }

  views::Button* dismiss_button() {
    return static_cast<views::Button*>(onboarding_view_->GetViewByID(
        PhoneHubViewID::kOnboardingDismissButton));
  }

  chromeos::phonehub::FakeOnboardingUiTracker* fake_onboarding_ui_tracker() {
    return &fake_onboarding_ui_tracker_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  chromeos::phonehub::FakeOnboardingUiTracker fake_onboarding_ui_tracker_;
  std::unique_ptr<OnboardingView> onboarding_view_ = nullptr;
};

TEST_F(OnboardingViewTest, PressGetStartedButton) {
  EXPECT_EQ(0u, fake_onboarding_ui_tracker()->handle_get_started_call_count());

  // Simulates a mouse press on the Get started button.
  onboarding_view()->ButtonPressed(
      get_started_button(),
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::PointF(), gfx::PointF(),
                     base::TimeTicks(), 0, 0));

  // Pressing Get started button should invoke the |HandleGetStarted| call.
  EXPECT_EQ(1u, fake_onboarding_ui_tracker()->handle_get_started_call_count());
}

TEST_F(OnboardingViewTest, PressDismissButton) {
  fake_onboarding_ui_tracker()->SetShouldShowOnboardingUi(true);

  // Simulates a mouse press on the Dismiss button.
  onboarding_view()->ButtonPressed(
      dismiss_button(), ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::PointF(),
                                       gfx::PointF(), base::TimeTicks(), 0, 0));

  // Pressing Dismiss button should disable the ability to show onboarding UI
  // again.
  EXPECT_FALSE(fake_onboarding_ui_tracker()->ShouldShowOnboardingUi());
}

}  // namespace ash
