// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/nearby_share/nearby_share_detailed_view_impl.h"

#include "ash/public/cpp/test/test_nearby_share_delegate.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/shell.h"
#include "ash/style/switch.h"
#include "ash/system/tray/fake_detailed_view_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {

class NearbyShareDetailedViewImplTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    test_delegate_ = static_cast<TestNearbyShareDelegate*>(
        Shell::Get()->nearby_share_delegate());
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kQuickShareV2);
  }

  void TearDown() override {
    detailed_view_.ClearAndDelete();
    widget_.reset();
    AshTestBase::TearDown();
  }

  views::Button* GetSettingsButton() const {
    CHECK(detailed_view_);
    return detailed_view_->settings_button_;
  }

  Switch* GetQuickShareToggle() const {
    CHECK(detailed_view_);
    return detailed_view_->toggle_switch_;
  }

  size_t GetCloseBubbleCallCount() const {
    return detailed_view_delegate_.close_bubble_call_count();
  }

 protected:
  void SetUpDetailedView() {
    std::unique_ptr<NearbyShareDetailedViewImpl> detailed_view =
        std::make_unique<NearbyShareDetailedViewImpl>(&detailed_view_delegate_);
    detailed_view_ = detailed_view.get();
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    widget_->SetContentsView(detailed_view.release()->GetAsView());
  }

  raw_ptr<TestNearbyShareDelegate, DanglingUntriaged> test_delegate_ = nullptr;

 private:
  raw_ptr<NearbyShareDetailedViewImpl> detailed_view_ = nullptr;
  FakeDetailedViewDelegate detailed_view_delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(NearbyShareDetailedViewImplTest,
       ShowNearbyShareSettings_OnClickSettingsButton) {
  SetUpDetailedView();
  views::Button* settings_button = GetSettingsButton();
  EXPECT_EQ(0, GetSystemTrayClient()->show_nearby_share_settings_count());
  LeftClickOn(settings_button);
  EXPECT_EQ(1, GetSystemTrayClient()->show_nearby_share_settings_count());
  EXPECT_EQ(1u, GetCloseBubbleCallCount());
}

TEST_F(NearbyShareDetailedViewImplTest,
       QuickShareV2_QuickShareToggledOff_WhenDisabled) {
  test_delegate_->SetEnabled(false);
  SetUpDetailedView();
  Switch* quick_share_toggle = GetQuickShareToggle();
  EXPECT_FALSE(quick_share_toggle->GetIsOn());
}

TEST_F(NearbyShareDetailedViewImplTest,
       QuickShareV2_QuickShareToggledOn_WhenEnabled) {
  test_delegate_->SetEnabled(true);
  SetUpDetailedView();
  Switch* quick_share_toggle = GetQuickShareToggle();
  EXPECT_TRUE(quick_share_toggle->GetIsOn());
}

TEST_F(NearbyShareDetailedViewImplTest, QuickShareV2_ToggleQuickShareOn) {
  test_delegate_->SetEnabled(false);
  SetUpDetailedView();
  Switch* quick_share_toggle = GetQuickShareToggle();
  EXPECT_FALSE(quick_share_toggle->GetIsOn());

  LeftClickOn(quick_share_toggle);
  EXPECT_TRUE(quick_share_toggle->GetIsOn());
  EXPECT_TRUE(test_delegate_->IsEnabled());
}

TEST_F(NearbyShareDetailedViewImplTest, QuickShareV2_ToggleQuickShareOff) {
  test_delegate_->SetEnabled(true);
  SetUpDetailedView();
  Switch* quick_share_toggle = GetQuickShareToggle();
  EXPECT_TRUE(quick_share_toggle->GetIsOn());

  LeftClickOn(quick_share_toggle);
  EXPECT_FALSE(quick_share_toggle->GetIsOn());
  EXPECT_FALSE(test_delegate_->IsEnabled());
}

}  // namespace ash
