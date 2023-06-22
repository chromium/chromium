// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/onboarding_nudge_controller.h"

#include <memory>

#include "ash/system/phonehub/phone_hub_tray.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/fake_phone_hub_manager.h"
#include "ui/views/widget/widget.h"

namespace ash {
class OnboardingNudgeControllerTest : public AshTestBase {
 public:
  OnboardingNudgeControllerTest() = default;
  OnboardingNudgeControllerTest(const OnboardingNudgeControllerTest&) = delete;
  OnboardingNudgeControllerTest& operator=(
      const OnboardingNudgeControllerTest&) = delete;
  ~OnboardingNudgeControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    phone_hub_tray_ =
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()->phone_hub_tray();
    phone_hub_tray_->SetPhoneHubManager(&phone_hub_manager_);
    controller_ = std::make_unique<OnboardingNudgeController>(
        /*phone_hub_tray=*/phone_hub_tray_,
        /*stop_animation_callback=*/base::DoNothing(),
        /*start_animation_callback=*/base::DoNothing());
  }

  OnboardingNudgeController* GetController() { return controller_.get(); }

 private:
  phonehub::FakePhoneHubManager phone_hub_manager_;
  raw_ptr<PhoneHubTray, ExperimentalAsh> phone_hub_tray_ = nullptr;
  std::unique_ptr<OnboardingNudgeController> controller_;
};

TEST_F(OnboardingNudgeControllerTest, OnboardingNudgeControllerExists) {
  OnboardingNudgeController* controller = GetController();
  ASSERT_TRUE(controller);
}

}  // namespace ash
