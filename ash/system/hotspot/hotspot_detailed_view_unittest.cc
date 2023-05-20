// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/hotspot/hotspot_detailed_view.h"

#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/rounded_container.h"
#include "ash/style/switch.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr char16_t kHotspotTitle[] = u"Chrome device hotspot";

}  // namespace

using hotspot_config::mojom::HotspotAllowStatus;
using hotspot_config::mojom::HotspotInfo;
using hotspot_config::mojom::HotspotState;

class FakeHotspotDetailedViewDelegate : public HotspotDetailedView::Delegate {
 public:
  FakeHotspotDetailedViewDelegate() = default;
  ~FakeHotspotDetailedViewDelegate() override = default;

  // HotspotDetailedView::Delegate:
  void OnToggleClicked(bool new_state) override {
    last_toggle_state_ = new_state;
  }

  bool last_toggle_state_ = false;
};

// This class exists to stub out the CloseBubble() call. This allows tests to
// directly construct the detailed view, without depending on the entire quick
// settings bubble and view hierarchy.
class FakeDetailedViewDelegate : public DetailedViewDelegate {
 public:
  FakeDetailedViewDelegate()
      : DetailedViewDelegate(/*tray_controller=*/nullptr) {}
  ~FakeDetailedViewDelegate() override = default;

  // DetailedViewDelegate:
  void CloseBubble() override { ++close_bubble_count_; }

  int close_bubble_count_ = 0;
};

class HotspotDetailedViewTest : public AshTestBase {
 public:
  HotspotDetailedViewTest() = default;
  ~HotspotDetailedViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    auto hotspot_detailed_view = std::make_unique<HotspotDetailedView>(
        &detailed_view_delegate_, &hotspot_detailed_view_delegate_);
    hotspot_detailed_view_ = hotspot_detailed_view.get();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    widget_->SetContentsView(hotspot_detailed_view.release());
  }

  void TearDown() override {
    widget_.reset();

    AshTestBase::TearDown();
  }

  void UpdateHotspotView(HotspotState state,
                         HotspotAllowStatus allow_status,
                         uint32_t client_count = 0) {
    auto hotspot_info = HotspotInfo::New();
    hotspot_info->state = state;
    hotspot_info->allow_status = allow_status;
    hotspot_info->client_count = client_count;
    hotspot_detailed_view_->UpdateViewForHotspot(std::move(hotspot_info));
  }

  views::Button* GetSettingsButton() {
    return FindViewById<views::Button*>(
        HotspotDetailedView::HotspotDetailedViewChildId::kSettingsButton);
  }

  HoverHighlightView* GetEntryRow() {
    return FindViewById<HoverHighlightView*>(
        HotspotDetailedView::HotspotDetailedViewChildId::kEntryRow);
  }

  Switch* GetToggleButton() {
    return FindViewById<Switch*>(
        HotspotDetailedView::HotspotDetailedViewChildId::kToggle);
  }

  views::ImageView* GetExtraIcon() {
    return FindViewById<views::ImageView*>(
        HotspotDetailedView::HotspotDetailedViewChildId::kExtraIcon);
  }

  void AssertTextLabel(const std::u16string& expected_text) {
    HoverHighlightView* entry_row = GetEntryRow();
    ASSERT_TRUE(entry_row->text_label());
    EXPECT_EQ(expected_text, entry_row->text_label()->GetText());
  }

  void AssertSubtextLabel(const std::u16string& expected_text) {
    HoverHighlightView* entry_row = GetEntryRow();
    if (expected_text.empty()) {
      EXPECT_FALSE(entry_row->sub_text_label());
      return;
    }
    ASSERT_TRUE(entry_row->sub_text_label());
    EXPECT_TRUE(entry_row->sub_text_label()->GetVisible());
    EXPECT_EQ(expected_text, entry_row->sub_text_label()->GetText());
  }

  void AssertEntryRowEnabled(bool expected_enabled) {
    HoverHighlightView* entry_row = GetEntryRow();
    ASSERT_TRUE(entry_row);
    if (expected_enabled) {
      EXPECT_TRUE(entry_row->GetEnabled());
      return;
    }
    EXPECT_FALSE(entry_row->GetEnabled());
  }

  void AssertToggleOn(bool expected_toggle_on) {
    Switch* toggle = GetToggleButton();
    ASSERT_TRUE(toggle);
    if (expected_toggle_on) {
      EXPECT_TRUE(toggle->GetIsOn());
      return;
    }
    EXPECT_FALSE(toggle->GetIsOn());
  }

 protected:
  template <class T>
  T FindViewById(HotspotDetailedView::HotspotDetailedViewChildId id) {
    return static_cast<T>(
        hotspot_detailed_view_->GetViewByID(static_cast<int>(id)));
  }

  std::unique_ptr<views::Widget> widget_;
  FakeHotspotDetailedViewDelegate hotspot_detailed_view_delegate_;
  FakeDetailedViewDelegate detailed_view_delegate_;
  raw_ptr<HotspotDetailedView, ExperimentalAsh> hotspot_detailed_view_ =
      nullptr;
};

TEST_F(HotspotDetailedViewTest, PressingSettingsButtonOpensSettings) {
  ASSERT_TRUE(hotspot_detailed_view_);
  views::Button* settings_button = GetSettingsButton();
  ASSERT_TRUE(settings_button);

  // Clicking the button at the lock screen does nothing.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  LeftClickOn(settings_button);
  EXPECT_EQ(0, GetSystemTrayClient()->show_hotspot_subpage_count());
  EXPECT_EQ(0, detailed_view_delegate_.close_bubble_count_);

  // Clicking the button in an active user session opens OS settings.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  LeftClickOn(settings_button);
  EXPECT_EQ(1, GetSystemTrayClient()->show_hotspot_subpage_count());
  EXPECT_EQ(1, detailed_view_delegate_.close_bubble_count_);
}

TEST_F(HotspotDetailedViewTest, HotspotEnabledUI) {
  UpdateHotspotView(HotspotState::kEnabled, HotspotAllowStatus::kAllowed);

  ASSERT_TRUE(hotspot_detailed_view_);
  AssertTextLabel(kHotspotTitle);
  AssertSubtextLabel(u"On, no devices connected");
  AssertEntryRowEnabled(/*expected_enabled=*/true);
  AssertToggleOn(/*expected_toggle_on=*/true);
  views::ImageView* extra_icon = GetExtraIcon();
  EXPECT_FALSE(extra_icon->GetVisible());

  UpdateHotspotView(HotspotState::kEnabled, HotspotAllowStatus::kAllowed, 1);
  AssertSubtextLabel(u"1 device connected");

  UpdateHotspotView(HotspotState::kEnabled, HotspotAllowStatus::kAllowed, 2);
  AssertSubtextLabel(u"2 devices connected");
}

TEST_F(HotspotDetailedViewTest, HotspotEnablingUI) {
  UpdateHotspotView(HotspotState::kEnabling, HotspotAllowStatus::kAllowed);

  ASSERT_TRUE(hotspot_detailed_view_);
  AssertTextLabel(kHotspotTitle);
  AssertSubtextLabel(u"Enabling…");
  AssertEntryRowEnabled(/*expected_enabled=*/false);
  AssertToggleOn(/*expected_toggle_on=*/true);
  views::ImageView* extra_icon = GetExtraIcon();
  EXPECT_FALSE(extra_icon->GetVisible());
}

TEST_F(HotspotDetailedViewTest, HotspotDisablingUI) {
  UpdateHotspotView(HotspotState::kDisabling, HotspotAllowStatus::kAllowed);

  ASSERT_TRUE(hotspot_detailed_view_);
  AssertTextLabel(kHotspotTitle);
  AssertSubtextLabel(u"Disabling…");
  AssertEntryRowEnabled(/*expected_enabled=*/false);
  AssertToggleOn(/*expected_toggle_on=*/false);
  views::ImageView* extra_icon = GetExtraIcon();
  EXPECT_FALSE(extra_icon->GetVisible());
}

TEST_F(HotspotDetailedViewTest, HotspotDisabledAndAllowedUI) {
  UpdateHotspotView(HotspotState::kDisabled, HotspotAllowStatus::kAllowed);

  ASSERT_TRUE(hotspot_detailed_view_);
  AssertTextLabel(kHotspotTitle);
  AssertSubtextLabel(std::u16string());
  AssertEntryRowEnabled(/*expected_enabled=*/true);
  AssertToggleOn(/*expected_toggle_on=*/false);
  views::ImageView* extra_icon = GetExtraIcon();
  EXPECT_FALSE(extra_icon->GetVisible());
}

TEST_F(HotspotDetailedViewTest, HotspotDisabledAndNoMobileNetworkUI) {
  UpdateHotspotView(HotspotState::kDisabled,
                    HotspotAllowStatus::kDisallowedNoMobileData);

  ASSERT_TRUE(hotspot_detailed_view_);
  AssertTextLabel(kHotspotTitle);
  AssertSubtextLabel(u"Connect to mobile data to use hotspot");
  AssertEntryRowEnabled(/*expected_enabled=*/false);
  AssertToggleOn(/*expected_toggle_on=*/false);
  views::ImageView* extra_icon = GetExtraIcon();
  EXPECT_FALSE(extra_icon->GetVisible());
}

TEST_F(HotspotDetailedViewTest,
       HotspotDisabledAndMobileNetworkNotSuppportedUI) {
  UpdateHotspotView(HotspotState::kDisabled,
                    HotspotAllowStatus::kDisallowedReadinessCheckFail);

  ASSERT_TRUE(hotspot_detailed_view_);
  AssertTextLabel(kHotspotTitle);
  AssertSubtextLabel(std::u16string());
  AssertEntryRowEnabled(/*expected_enabled=*/false);
  AssertToggleOn(/*expected_toggle_on=*/false);
  views::ImageView* extra_icon = GetExtraIcon();
  EXPECT_TRUE(extra_icon->GetVisible());
  EXPECT_EQ(u"Your mobile network doesn't support hotspot",
            extra_icon->GetTooltipText());
}

TEST_F(HotspotDetailedViewTest, HotspotDisabledAndBlockedByPolicyUI) {
  UpdateHotspotView(HotspotState::kDisabled,
                    HotspotAllowStatus::kDisallowedByPolicy);

  ASSERT_TRUE(hotspot_detailed_view_);
  AssertTextLabel(kHotspotTitle);
  AssertSubtextLabel(std::u16string());
  AssertEntryRowEnabled(/*expected_enabled=*/false);
  AssertToggleOn(/*expected_toggle_on=*/false);
  views::ImageView* extra_icon = GetExtraIcon();
  EXPECT_TRUE(extra_icon->GetVisible());
  EXPECT_EQ(u"This setting is managed by your administrator",
            extra_icon->GetTooltipText());
}

TEST_F(HotspotDetailedViewTest, PressingEntryRowNotifiesDelegate) {
  ASSERT_TRUE(hotspot_detailed_view_);
  HoverHighlightView* entry_row = GetEntryRow();
  EXPECT_FALSE(hotspot_detailed_view_delegate_.last_toggle_state_);

  LeftClickOn(entry_row);
  EXPECT_TRUE(hotspot_detailed_view_delegate_.last_toggle_state_);
}

TEST_F(HotspotDetailedViewTest, PressingToggleNotifiesDelegate) {
  ASSERT_TRUE(hotspot_detailed_view_);
  Switch* toggle = GetToggleButton();
  EXPECT_FALSE(toggle->GetIsOn());
  EXPECT_FALSE(hotspot_detailed_view_delegate_.last_toggle_state_);

  LeftClickOn(toggle);
  EXPECT_TRUE(toggle->GetIsOn());
  EXPECT_TRUE(hotspot_detailed_view_delegate_.last_toggle_state_);
}

}  // namespace ash
