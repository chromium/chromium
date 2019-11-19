// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/accessibility_feature_pod_controller.h"

#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"

namespace ash {

// Tests manually control their session state.
class AccessibilityFeaturePodControllerTest : public NoSessionAshTestBase {
 public:
  AccessibilityFeaturePodControllerTest() = default;
  ~AccessibilityFeaturePodControllerTest() override = default;

  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    tray_model_ = std::make_unique<UnifiedSystemTrayModel>(nullptr);
    tray_controller_ =
        std::make_unique<UnifiedSystemTrayController>(tray_model_.get());
  }

  void TearDown() override {
    button_.reset();
    controller_.reset();
    tray_controller_.reset();
    tray_model_.reset();
    NoSessionAshTestBase::TearDown();
  }

 protected:
  void SetUpButton() {
    controller_ =
        std::make_unique<AccessibilityFeaturePodController>(tray_controller());
    button_.reset(controller_->CreateButton());
  }

  UnifiedSystemTrayController* tray_controller() {
    return tray_controller_.get();
  }

  FeaturePodButton* button() { return button_.get(); }

 private:
  std::unique_ptr<UnifiedSystemTrayModel> tray_model_;
  std::unique_ptr<UnifiedSystemTrayController> tray_controller_;
  std::unique_ptr<AccessibilityFeaturePodController> controller_;
  std::unique_ptr<FeaturePodButton> button_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityFeaturePodControllerTest);
};

TEST_F(AccessibilityFeaturePodControllerTest, ButtonVisibilityNotLoggedIn) {
  SetUpButton();
  // If not logged in, it should be always visible.
  EXPECT_TRUE(button()->GetVisible());
}

TEST_F(AccessibilityFeaturePodControllerTest, ButtonVisibilityLoggedIn) {
  CreateUserSessions(1);
  SetUpButton();
  // If logged in, it's not visible by default.
  EXPECT_FALSE(button()->GetVisible());
}

}  // namespace ash
