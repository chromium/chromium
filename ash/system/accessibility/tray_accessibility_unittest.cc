// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/tray_accessibility.h"
#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/accessibility_observer.h"
#include "ash/magnifier/docked_magnifier_controller_impl.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"

namespace ash {
namespace {

using FeatureType = AccessibilityControllerImpl::FeatureType;

void SetFeatureEnabled(FeatureType feature, bool enabled) {
  if (feature == FeatureType::kDictation)
    Shell::Get()->accessibility_controller()->dictation().SetDialogAccepted();
  Shell::Get()->accessibility_controller()->GetFeature(feature).SetEnabled(
      enabled);
}

FeatureType kListedFeautures[] = {
    FeatureType::kSpokenFeedback,      FeatureType::kSelectToSpeak,
    FeatureType::kDictation,           FeatureType::kHighContrast,
    FeatureType::kFullscreenMagnifier, FeatureType::kDockedMagnifier,
    FeatureType::kAutoclick,           FeatureType::kVirtualKeyboard,
    FeatureType::kSwitchAccess,        FeatureType::kLargeCursor,
    FeatureType::kMonoAudio,           FeatureType::kCaretHighlight,
    FeatureType::kCursorHighlight,     FeatureType::kFocusHighlight,
    FeatureType::kStickyKeys};
}  // namespace

class TrayAccessibilityTest : public AshTestBase, public AccessibilityObserver {
 protected:
  TrayAccessibilityTest() = default;
  ~TrayAccessibilityTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->accessibility_controller()->AddObserver(this);
  }

  void TearDown() override {
    Shell::Get()->accessibility_controller()->RemoveObserver(this);
    AshTestBase::TearDown();
  }

  void CreateDetailedMenu() {
    delegate_ = std::make_unique<DetailedViewDelegate>(nullptr);
    detailed_menu_ =
        std::make_unique<tray::AccessibilityDetailedView>(delegate_.get());
  }

  void CloseDetailMenu() {
    detailed_menu_.reset();
    delegate_.reset();
  }

  void ClickView(HoverHighlightView* view) {
    detailed_menu_->OnViewClicked(view);
  }

  void ClickFeatureOnDetailMenu(FeatureType feature) {
    ClickView(detailed_menu_->feature_views_[feature]);
  }

  bool CheckFeaturesHiddenOnDetailMenu(
      const std::set<FeatureType>& expected_hidden) const {
    std::set<FeatureType> actual_hidden;
    for (auto feature : kListedFeautures)
      if (!detailed_menu_->feature_views_[feature])
        actual_hidden.insert(feature);
    return actual_hidden == expected_hidden;
  }

  // In material design we show the help button but theme it as disabled if
  // it is not possible to load the help page.
  bool IsHelpAvailableOnDetailMenu() {
    return detailed_menu_->help_view_->GetState() ==
           views::Button::STATE_NORMAL;
  }

  // In material design we show the settings button but theme it as disabled if
  // it is not possible to load the settings page.
  bool IsSettingsAvailableOnDetailMenu() {
    return detailed_menu_->settings_view_->GetState() ==
           views::Button::STATE_NORMAL;
  }

  // An item is enabled on the detailed menu if it is marked checked for
  // accessibility and the detailed_menu_'s local state, |enabled_state|, is
  // enabled. Check that the checked state and detailed_menu_'s local state are
  // the same.
  bool IsEnabledOnDetailMenu(bool enabled_state, views::View* view) const {
    // Sometimes views are not created because of a conflicting feature.
    if (!view)
      return enabled_state;

    ui::AXNodeData node_data;
    view->GetAccessibleNodeData(&node_data);
    bool checked_for_accessibility =
        node_data.GetCheckedState() == ax::mojom::CheckedState::kTrue;
    DCHECK(enabled_state == checked_for_accessibility);
    return enabled_state && checked_for_accessibility;
  }

  bool IsFeatureEnabledOnDetailMenu(FeatureType feature) const {
    return IsEnabledOnDetailMenu(detailed_menu_->features_enabled_[feature],
                                 detailed_menu_->feature_views_[feature]);
  }

  bool CheckFeaturesEnabledOnDetailMenu(
      const std::set<FeatureType>& expected_enabled) {
    std::set<FeatureType> actual_enabled;
    for (auto feature : kListedFeautures)
      if (IsFeatureEnabledOnDetailMenu(feature))
        actual_enabled.insert(feature);
    return actual_enabled == expected_enabled;
  }

  const char* GetDetailedViewClassName() {
    return detailed_menu_->GetClassName();
  }

 private:
  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override {
    // UnifiedAccessibilityDetailedViewController calls
    // AccessibilityDetailedView::OnAccessibilityStatusChanged. Spoof that
    // by calling it directly here.
    if (detailed_menu_)
      detailed_menu_->OnAccessibilityStatusChanged();
  }

  std::unique_ptr<DetailedViewDelegate> delegate_;
  std::unique_ptr<tray::AccessibilityDetailedView> detailed_menu_;

  DISALLOW_COPY_AND_ASSIGN(TrayAccessibilityTest);
};

TEST_F(TrayAccessibilityTest, CheckMenuVisibilityOnDetailMenu) {
  // Except help & settings, others should be kept the same
  // in LOGIN | NOT LOGIN | LOCKED. https://crbug.com/632107.
  CreateDetailedMenu();

  EXPECT_TRUE(CheckFeaturesHiddenOnDetailMenu({}));
  CloseDetailMenu();

  // Simulate screen lock.
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  CreateDetailedMenu();
  EXPECT_TRUE(CheckFeaturesHiddenOnDetailMenu({}));
  EXPECT_FALSE(IsHelpAvailableOnDetailMenu());
  EXPECT_FALSE(IsSettingsAvailableOnDetailMenu());
  CloseDetailMenu();
  UnblockUserSession();

  // Simulate adding multiprofile user.
  BlockUserSession(BLOCKED_BY_USER_ADDING_SCREEN);
  CreateDetailedMenu();
  EXPECT_TRUE(CheckFeaturesHiddenOnDetailMenu({}));
  EXPECT_FALSE(IsHelpAvailableOnDetailMenu());
  EXPECT_FALSE(IsSettingsAvailableOnDetailMenu());
  CloseDetailMenu();
  UnblockUserSession();
}

TEST_F(TrayAccessibilityTest, ClickDetailMenu) {
  AccessibilityControllerImpl* accessibility_controller =
      Shell::Get()->accessibility_controller();

  for (auto feature : kListedFeautures) {
    SCOPED_TRACE(base::StringPrintf("Testing feature #[%d]", feature));
    if (feature == FeatureType::kDictation)
      Shell::Get()->accessibility_controller()->dictation().SetDialogAccepted();

    // Confirms that the check item toggles the given feature.
    EXPECT_FALSE(accessibility_controller->GetFeature(feature).enabled());

    CreateDetailedMenu();
    ClickFeatureOnDetailMenu(feature);
    EXPECT_TRUE(accessibility_controller->GetFeature(feature).enabled());

    CreateDetailedMenu();
    ClickFeatureOnDetailMenu(feature);
    EXPECT_FALSE(accessibility_controller->GetFeature(feature).enabled());
  }
}

// Trivial test to increase code coverage.
TEST_F(TrayAccessibilityTest, GetClassName) {
  CreateDetailedMenu();
  EXPECT_EQ(tray::AccessibilityDetailedView::kClassName,
            GetDetailedViewClassName());
}

class TrayAccessibilityLoginScreenTest : public TrayAccessibilityTest {
 protected:
  TrayAccessibilityLoginScreenTest() { set_start_session(false); }
  ~TrayAccessibilityLoginScreenTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(TrayAccessibilityLoginScreenTest);
};

TEST_F(TrayAccessibilityLoginScreenTest, CheckMarksOnDetailMenu) {
  // At first, all of the check is unchecked.
  CreateDetailedMenu();
  EXPECT_TRUE(CheckFeaturesEnabledOnDetailMenu({}));
  CloseDetailMenu();

  for (auto feature : kListedFeautures) {
    // Switch Access is currently not available on the login screen; see
    // crbug/1108808
    if (feature == FeatureType::kSwitchAccess)
      continue;

    SetFeatureEnabled(feature, true);
    CreateDetailedMenu();
    EXPECT_TRUE(CheckFeaturesEnabledOnDetailMenu({feature}));
    CloseDetailMenu();

    SetFeatureEnabled(feature, false);
    CreateDetailedMenu();
    EXPECT_TRUE(CheckFeaturesEnabledOnDetailMenu({}));
    CloseDetailMenu();
  }

  // Enabling all of the a11y features.
  for (auto feature : kListedFeautures)
    SetFeatureEnabled(feature, true);
  CreateDetailedMenu();

  // The latest screen magnifier will disable the other one, in our case
  // fullscreen magnifier gets disabled.
  std::set<FeatureType> expected_enabled_features;
  for (auto feature : kListedFeautures)
    if (feature != FeatureType::kFullscreenMagnifier &&
        feature != FeatureType::kFocusHighlight) {
      expected_enabled_features.insert(feature);
    }
  // Focus highlighting can't be on when spoken feedback is on.
  EXPECT_TRUE(

      CheckFeaturesEnabledOnDetailMenu(expected_enabled_features));
  CloseDetailMenu();

  // Disabling all of the a11y features.
  for (auto feature : kListedFeautures)
    SetFeatureEnabled(feature, false);
  CreateDetailedMenu();

  EXPECT_TRUE(CheckFeaturesEnabledOnDetailMenu({}));
  CloseDetailMenu();

  // Enabling autoclick.
  SetFeatureEnabled(FeatureType::kAutoclick, true);
  CreateDetailedMenu();
  EXPECT_TRUE(CheckFeaturesEnabledOnDetailMenu({FeatureType::kAutoclick}));
  CloseDetailMenu();

  // Disabling autoclick.
  SetFeatureEnabled(FeatureType::kAutoclick, false);
  CreateDetailedMenu();
  EXPECT_TRUE(CheckFeaturesEnabledOnDetailMenu({}));
  CloseDetailMenu();
}

}  // namespace ash
