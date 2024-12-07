// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/nearby_share/nearby_share_detailed_view_impl.h"

#include "ash/public/cpp/test/test_nearby_share_delegate.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/shell.h"
#include "ash/style/switch.h"
#include "ash/system/tray/fake_detailed_view_delegate.h"
#include "ash/system/tray/hover_highlight_view.h"
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
    return detailed_view_->quick_share_toggle_;
  }

  HoverHighlightView* GetYourDevicesRow() const {
    CHECK(detailed_view_);
    return detailed_view_->your_devices_row_;
  }

  HoverHighlightView* GetContactsRow() const {
    CHECK(detailed_view_);
    return detailed_view_->contacts_row_;
  }

  HoverHighlightView* GetHiddenRow() const {
    CHECK(detailed_view_);
    return detailed_view_->hidden_row_;
  }

  Switch* GetEveryoneToggle() const {
    CHECK(detailed_view_);
    return detailed_view_->everyone_toggle_;
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

TEST_F(NearbyShareDetailedViewImplTest, QuickShareV2_ToggleYourDevices) {
  test_delegate_->SetEnabled(true);
  test_delegate_->SetVisibility(
      ::nearby_share::mojom::Visibility::kAllContacts);
  SetUpDetailedView();
  EXPECT_EQ(::nearby_share::mojom::Visibility::kAllContacts,
            test_delegate_->GetVisibility());

  HoverHighlightView* your_devices_row = GetYourDevicesRow();
  LeftClickOn(your_devices_row);
  EXPECT_EQ(::nearby_share::mojom::Visibility::kYourDevices,
            test_delegate_->GetVisibility());
}

TEST_F(NearbyShareDetailedViewImplTest, QuickShareV2_ToggleContacts) {
  test_delegate_->SetEnabled(true);
  test_delegate_->SetVisibility(
      ::nearby_share::mojom::Visibility::kYourDevices);
  SetUpDetailedView();
  EXPECT_EQ(::nearby_share::mojom::Visibility::kYourDevices,
            test_delegate_->GetVisibility());

  HoverHighlightView* contacts_row = GetContactsRow();
  LeftClickOn(contacts_row);
  EXPECT_EQ(::nearby_share::mojom::Visibility::kAllContacts,
            test_delegate_->GetVisibility());
}

TEST_F(NearbyShareDetailedViewImplTest, QuickShareV2_ToggleHidden) {
  test_delegate_->SetEnabled(true);
  test_delegate_->SetVisibility(
      ::nearby_share::mojom::Visibility::kYourDevices);
  SetUpDetailedView();
  EXPECT_EQ(::nearby_share::mojom::Visibility::kYourDevices,
            test_delegate_->GetVisibility());

  HoverHighlightView* hidden_row = GetHiddenRow();
  LeftClickOn(hidden_row);
  EXPECT_EQ(::nearby_share::mojom::Visibility::kNoOne,
            test_delegate_->GetVisibility());
}

TEST_F(NearbyShareDetailedViewImplTest,
       QuickShareV2_ToggleEveryoneVisibilityOn) {
  test_delegate_->SetEnabled(true);
  test_delegate_->set_is_high_visibility_on(false);
  SetUpDetailedView();
  Switch* toggle = GetEveryoneToggle();
  LeftClickOn(toggle);
  EXPECT_EQ(TestNearbyShareDelegate::Method::kEnableHighVisibility,
            test_delegate_->method_calls()[0]);
}

TEST_F(NearbyShareDetailedViewImplTest,
       QuickShareV2_ToggleEveryoneVisibilityOff) {
  test_delegate_->SetEnabled(true);
  test_delegate_->set_is_high_visibility_on(true);
  SetUpDetailedView();
  Switch* toggle = GetEveryoneToggle();
  LeftClickOn(toggle);
  EXPECT_EQ(TestNearbyShareDelegate::Method::kDisableHighVisibility,
            test_delegate_->method_calls()[0]);
}

TEST_F(
    NearbyShareDetailedViewImplTest,
    QuickShareV2_EveryoneVisibilityToggleEnabled_OnEnableHighVisibilityRequestActive) {
  test_delegate_->SetEnabled(true);
  test_delegate_->set_is_high_visibility_on(false);
  test_delegate_->set_is_enable_high_visibility_request_active(true);
  SetUpDetailedView();
  Switch* toggle = GetEveryoneToggle();
  EXPECT_TRUE(toggle->GetIsOn());
}

TEST_F(
    NearbyShareDetailedViewImplTest,
    QuickShareV2_NoEffect_OnEveryoneVisibilityToggleClick_WhileEnableHighVisibilityRequestActive) {
  test_delegate_->SetEnabled(true);
  test_delegate_->set_is_high_visibility_on(false);
  test_delegate_->set_is_enable_high_visibility_request_active(true);
  SetUpDetailedView();
  Switch* toggle = GetEveryoneToggle();
  LeftClickOn(toggle);

  // Expect no calls, including En/DisableHighVisibility, have been made to
  // NearbyShareDelegate.
  EXPECT_EQ(0u, test_delegate_->method_calls().size());
}

TEST_F(
    NearbyShareDetailedViewImplTest,
    QuickShareV2_NoEffect_OnQuickShareToggleClick_WhileEnableHighVisibilityRequestActive) {
  test_delegate_->SetEnabled(true);
  test_delegate_->set_is_high_visibility_on(false);
  test_delegate_->set_is_enable_high_visibility_request_active(true);
  SetUpDetailedView();
  Switch* toggle = GetQuickShareToggle();
  LeftClickOn(toggle);

  // Expect no calls, including En/DisableHighVisibility, have been made to
  // NearbyShareDelegate.
  EXPECT_EQ(0u, test_delegate_->method_calls().size());
}

}  // namespace ash
