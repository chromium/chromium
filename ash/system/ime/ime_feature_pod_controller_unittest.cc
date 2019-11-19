// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/ime/ime_feature_pod_controller.h"

#include <vector>

#include "ash/shell.h"
#include "ash/ime/ime_controller.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"


namespace ash {

// Tests manually control their session state.
class IMEFeaturePodControllerTest : public NoSessionAshTestBase {
 public:
  IMEFeaturePodControllerTest() = default;
  ~IMEFeaturePodControllerTest() override = default;

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
        std::make_unique<IMEFeaturePodController>(tray_controller());
    button_.reset(controller_->CreateButton());
  }

  UnifiedSystemTrayController* tray_controller() {
    return tray_controller_.get();
  }

  FeaturePodButton* button() { return button_.get(); }

  // Creates |count| simulated active IMEs.
  void SetActiveIMECount(int count) {
    available_imes_.resize(count);
    for (int i = 0; i < count; ++i)
      available_imes_[i].id = base::NumberToString(i);
    RefreshImeController();
  }

  void RefreshImeController() {
    std::vector<mojom::ImeInfoPtr> available_ime_ptrs;
    for (const auto& ime : available_imes_)
      available_ime_ptrs.push_back(ime.Clone());

    std::vector<mojom::ImeMenuItemPtr> menu_item_ptrs;
    for (const auto& item : menu_items_)
      menu_item_ptrs.push_back(item.Clone());

    Shell::Get()->ime_controller()->RefreshIme(current_ime_.id,
                                              std::move(available_ime_ptrs),
                                              std::move(menu_item_ptrs));
  }

 private:
  std::unique_ptr<UnifiedSystemTrayModel> tray_model_;
  std::unique_ptr<UnifiedSystemTrayController> tray_controller_;
  std::unique_ptr<IMEFeaturePodController> controller_;
  std::unique_ptr<FeaturePodButton> button_;

  // IMEs
  mojom::ImeInfo current_ime_;
  std::vector<mojom::ImeInfo> available_imes_;
  std::vector<mojom::ImeMenuItem> menu_items_;

  DISALLOW_COPY_AND_ASSIGN(IMEFeaturePodControllerTest);
};

// Tests that if the pod button is hidden if less than 2 IMEs are present.
TEST_F(IMEFeaturePodControllerTest, ButtonVisibilityIMECount) {
  SetUpButton();

  SetActiveIMECount(0);
  EXPECT_FALSE(button()->GetVisible());
  SetActiveIMECount(1);
  EXPECT_FALSE(button()->GetVisible());
  SetActiveIMECount(2);
  EXPECT_TRUE(button()->GetVisible());
}

TEST_F(IMEFeaturePodControllerTest, ButtonVisibilityImeMenuActive) {
  SetUpButton();
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);

  SetActiveIMECount(0);
  EXPECT_FALSE(button()->GetVisible());
  SetActiveIMECount(1);
  EXPECT_FALSE(button()->GetVisible());
  SetActiveIMECount(2);
  EXPECT_FALSE(button()->GetVisible());
}

TEST_F(IMEFeaturePodControllerTest, ButtonVisibilityPolicy) {
  SetUpButton();

  Shell::Get()->ime_controller()->SetImesManagedByPolicy(true);

  SetActiveIMECount(0);
  EXPECT_TRUE(button()->GetVisible());
  SetActiveIMECount(1);
  EXPECT_TRUE(button()->GetVisible());
  SetActiveIMECount(2);
  EXPECT_TRUE(button()->GetVisible());
}

}  // namespace ash
