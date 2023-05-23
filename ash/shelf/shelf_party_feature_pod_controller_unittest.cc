// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_party_feature_pod_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shell.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// Tests are parameterized by feature QsRevamp.
class ShelfPartyFeaturePodControllerTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  ShelfPartyFeaturePodControllerTest() {
    if (IsQsRevampEnabled()) {
      feature_list_.InitWithFeatures(
          {features::kShelfParty, features::kQsRevamp}, {});
    } else {
      feature_list_.InitWithFeatures({features::kShelfParty},
                                     {features::kQsRevamp});
    }
  }

  bool IsQsRevampEnabled() const { return GetParam(); }

  // AshTestBase:
  void TearDown() override {
    tile_.reset();
    button_.reset();
    controller_.reset();
    AshTestBase::TearDown();
  }

  void CreateButton() {
    controller_ = std::make_unique<ShelfPartyFeaturePodController>();
    if (IsQsRevampEnabled()) {
      tile_ = controller_->CreateTile();
    } else {
      button_ = base::WrapUnique(controller_->CreateButton());
    }
  }

  bool IsButtonVisible() {
    return IsQsRevampEnabled() ? tile_->GetVisible() : button_->GetVisible();
  }

  bool IsButtonToggled() {
    return IsQsRevampEnabled() ? tile_->IsToggled() : button_->IsToggled();
  }

  void PressIcon() { controller_->OnIconPressed(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ShelfPartyFeaturePodController> controller_;
  std::unique_ptr<FeaturePodButton> button_;
  std::unique_ptr<FeatureTile> tile_;
};

INSTANTIATE_TEST_SUITE_P(QsRevamp,
                         ShelfPartyFeaturePodControllerTest,
                         testing::Bool());

TEST_P(ShelfPartyFeaturePodControllerTest, ButtonVisibility) {
  auto* session_controller = GetSessionControllerClient();
  // The button is visible in an active session.
  CreateButton();
  EXPECT_TRUE(IsButtonVisible());

  // The button is not visible at the lock screen.
  session_controller->LockScreen();
  CreateButton();
  EXPECT_FALSE(IsButtonVisible());

  // The button is not visible when enterprise managed.
  session_controller->set_is_enterprise_managed(true);
  session_controller->SetSessionState(session_manager::SessionState::ACTIVE);
  CreateButton();
  EXPECT_FALSE(IsButtonVisible());
}

TEST_P(ShelfPartyFeaturePodControllerTest, PressIconTogglesShelfParty) {
  auto* shelf_model = Shell::Get()->shelf_controller()->model();
  CreateButton();
  ASSERT_FALSE(shelf_model->in_shelf_party());

  // Pressing the icon enables shelf party.
  PressIcon();
  EXPECT_TRUE(shelf_model->in_shelf_party());

  // Pressing the icon again disables shelf party.
  PressIcon();
  EXPECT_FALSE(shelf_model->in_shelf_party());
}

TEST_P(ShelfPartyFeaturePodControllerTest, ShelfPartyToggled) {
  auto* shelf_model = Shell::Get()->shelf_controller()->model();
  CreateButton();
  ASSERT_FALSE(shelf_model->in_shelf_party());
  EXPECT_FALSE(IsButtonToggled());

  // Toggles the shelf party from `shelf_model` to enable shelf party.
  shelf_model->ToggleShelfParty();
  EXPECT_TRUE(shelf_model->in_shelf_party());
  EXPECT_TRUE(IsButtonToggled());

  // Toggles again to disable shelf party.
  shelf_model->ToggleShelfParty();
  EXPECT_FALSE(shelf_model->in_shelf_party());
  EXPECT_FALSE(IsButtonToggled());
}

}  // namespace ash
