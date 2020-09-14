// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/tray_accessibility.h"

#include <memory>
#include <utility>

#include "ash/accessibility/accessibility_delegate.h"
#include "ash/magnifier/docked_magnifier_controller_impl.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/machine_learning/user_settings_event_logger.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/metrics/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/separator.h"

namespace ash {
namespace {

using ml::UserSettingsEvent;
using FeatureType = AccessibilityControllerImpl::FeatureType;

enum AccessibilityState {
  A11Y_NONE = 0,
  A11Y_SPOKEN_FEEDBACK = 1 << 0,
  A11Y_HIGH_CONTRAST = 1 << 1,
  A11Y_SCREEN_MAGNIFIER = 1 << 2,
  A11Y_LARGE_CURSOR = 1 << 3,
  A11Y_AUTOCLICK = 1 << 4,
  A11Y_VIRTUAL_KEYBOARD = 1 << 5,
  A11Y_MONO_AUDIO = 1 << 6,
  A11Y_CARET_HIGHLIGHT = 1 << 7,
  A11Y_HIGHLIGHT_MOUSE_CURSOR = 1 << 8,
  A11Y_HIGHLIGHT_KEYBOARD_FOCUS = 1 << 9,
  A11Y_STICKY_KEYS = 1 << 10,
  A11Y_SELECT_TO_SPEAK = 1 << 11,
  A11Y_DOCKED_MAGNIFIER = 1 << 12,
  A11Y_DICTATION = 1 << 13,
  A11Y_SWITCH_ACCESS = 1 << 14,
};

const FeatureType kPrimaryA11yOptions[] = {
    FeatureType::kSpokenFeedback,      FeatureType::kSelectToSpeak,
    FeatureType::kDictation,           FeatureType::kHighContrast,
    FeatureType::kFullscreenMagnifier, FeatureType::kDockedMagnifier,
    FeatureType::kAutoclick,           FeatureType::kVirtualKeyboard,
    FeatureType::kSwitchAccess};
const FeatureType kSecondaryA11yOptions[] = {
    FeatureType::kLargeCursor,    FeatureType::kMonoAudio,
    FeatureType::kCaretHighlight, FeatureType::kCursorHighlight,
    FeatureType::kFocusHighlight, FeatureType::kStickyKeys};

void LogUserAccessibilityEvent(UserSettingsEvent::Event::AccessibilityId id,
                               bool new_state) {
  auto* logger = ml::UserSettingsEventLogger::Get();
  if (logger) {
    logger->LogAccessibilityUkmEvent(id, new_state);
  }
}

const struct FeatureData {
  FeatureType type;
  int description;
  UserSettingsEvent::Event::AccessibilityId event_id;
  const char* action_enabled;
  const char* action_disabled;
} kFeaturesData[] = {
    {FeatureType::kAutoclick, IDS_ASH_STATUS_TRAY_ACCESSIBILITY_AUTOCLICK,
     UserSettingsEvent::Event::AUTO_CLICK, "StatusArea_AutoClickEnabled",
     "StatusArea_AutoClickDisabled"},
    {FeatureType::kCaretHighlight,
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_CARET_HIGHLIGHT,
     UserSettingsEvent::Event::CARET_HIGHLIGHT,
     "StatusArea_CaretHighlightEnabled", "StatusArea_CaretHighlightDisabled"},
    {FeatureType::kCursorHighlight,
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_HIGHLIGHT_MOUSE_CURSOR,
     UserSettingsEvent::Event::HIGHLIGHT_MOUSE_CURSOR,
     "StatusArea_CursorHighlightEnabled", "StatusArea_CursorHighlightDisabled"},
    {FeatureType::kDictation, IDS_ASH_STATUS_TRAY_ACCESSIBILITY_DICTATION,
     UserSettingsEvent::Event::DICTATION, "StatusArea_DictationEnabled",
     "StatusArea_DictationDisabled"},
    {FeatureType::kFocusHighlight,
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_HIGHLIGHT_KEYBOARD_FOCUS,
     UserSettingsEvent::Event::HIGHLIGHT_KEYBOARD_FOCUS,
     "StatusArea_FocusHighlightEnabled", "StatusArea_FocusHighlightDisabled"},
    {FeatureType::kFullscreenMagnifier,
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SCREEN_MAGNIFIER,
     UserSettingsEvent::Event::MAGNIFIER, "StatusArea_ScreenMagnifierEnabled",
     "StatusArea_ScreenMagnifierDisabled"},
    {FeatureType::kDockedMagnifier,
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_DOCKED_MAGNIFIER,
     UserSettingsEvent::Event::DOCKED_MAGNIFIER,
     "StatusArea_DockedMagnifierEnabled", "StatusArea_DockedMagnifierDisabled"},
    {FeatureType::kHighContrast,
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_HIGH_CONTRAST_MODE,
     UserSettingsEvent::Event::HIGH_CONTRAST, "StatusArea_HighContrastEnabled",
     "StatusArea_HighContrastDisabled"},
    {FeatureType::kLargeCursor, IDS_ASH_STATUS_TRAY_ACCESSIBILITY_LARGE_CURSOR,
     UserSettingsEvent::Event::LARGE_CURSOR, "StatusArea_LargeCursorEnabled",
     "StatusArea_LargeCursorDisabled"},
    {FeatureType::kMonoAudio, IDS_ASH_STATUS_TRAY_ACCESSIBILITY_MONO_AUDIO,
     UserSettingsEvent::Event::MONO_AUDIO, "StatusArea_MonoAudioEnabled",
     "StatusArea_MonoAudioDisabled"},
    {FeatureType::kSpokenFeedback,
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SPOKEN_FEEDBACK,
     UserSettingsEvent::Event::SPOKEN_FEEDBACK,
     "StatusArea_SpokenFeedbackEnabled", "StatusArea_SpokenFeedbackDisabled"},
    {FeatureType::kSelectToSpeak,
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SELECT_TO_SPEAK,
     UserSettingsEvent::Event::SELECT_TO_SPEAK,
     "StatusArea_SelectToSpeakEnabled", "StatusArea_SelectToSpeakDisabled"},
    {FeatureType::kStickyKeys, IDS_ASH_STATUS_TRAY_ACCESSIBILITY_STICKY_KEYS,
     UserSettingsEvent::Event::STICKY_KEYS, "StatusArea_StickyKeysEnabled",
     "StatusArea_StickyKeysDisabled"},
    {FeatureType::kSwitchAccess,
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SWITCH_ACCESS,
     UserSettingsEvent::Event::SWITCH_ACCESS, "StatusArea_SwitchAccessEnabled",
     "StatusArea_SwitchAccessDisabled"},
    {FeatureType::kVirtualKeyboard,
     IDS_ASH_STATUS_TRAY_ACCESSIBILITY_VIRTUAL_KEYBOARD,
     UserSettingsEvent::Event::VIRTUAL_KEYBOARD,
     "StatusArea_VirtualKeyboardEnabled",
     "StatusArea_VirtualKeyboardDisabled"}};

const FeatureData& GetDataForFeature(FeatureType type) {
  for (const auto& data : kFeaturesData) {
    if (data.type == type)
      return data;
  }
  NOTREACHED();
  static FeatureData unknown;
  return unknown;
}

bool IsPrimarySettingsViewVisibleInTray() {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  for (FeatureType feature : kPrimaryA11yOptions)
    if (controller->GetFeature(feature).IsVisibleInTray())
      return true;
  return false;
}

bool IsAdditionalSettingsViewVisibleInTray() {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  for (FeatureType feature : kSecondaryA11yOptions)
    if (controller->GetFeature(feature).IsVisibleInTray())
      return true;
  return false;
}

bool IsAdditionalSettingsSeparatorVisibleInTray() {
  return IsPrimarySettingsViewVisibleInTray() &&
         IsAdditionalSettingsViewVisibleInTray();
}

}  // namespace

namespace tray {

////////////////////////////////////////////////////////////////////////////////
// ash::tray::AccessibilityDetailedView

constexpr char AccessibilityDetailedView::kClassName[];

AccessibilityDetailedView::AccessibilityDetailedView(
    DetailedViewDelegate* delegate)
    : TrayDetailedView(delegate) {
  Reset();
  AppendAccessibilityList();
  CreateTitleRow(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_TITLE);
  Layout();
}

void AccessibilityDetailedView::OnAccessibilityStatusChanged() {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  for (int feature_id = 0; feature_id < FeatureType::kFeatureCount;
       feature_id++) {
    AccessibilityControllerImpl::Feature& feature =
        controller->GetFeature(static_cast<FeatureType>(feature_id));
    features_enabled_[feature_id] = feature.enabled();
    if (feature.IsVisibleInTray() && feature_views_[feature_id]) {
      TrayPopupUtils::UpdateCheckMarkVisibility(feature_views_[feature_id],
                                                feature.enabled());
    }
  }
}

const char* AccessibilityDetailedView::GetClassName() const {
  return kClassName;
}

void AccessibilityDetailedView::AppendAccessibilityList() {
  CreateScrollableList();
  // We need to reset all existing feature views.
  for (int feature_id = 0; feature_id < FeatureType::kFeatureCount;
       feature_id++) {
    feature_views_[feature_id] = nullptr;
  }

  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  for (FeatureType feature_type : kPrimaryA11yOptions) {
    AccessibilityControllerImpl::Feature& feature =
        controller->GetFeature(feature_type);
    features_enabled_[feature_type] = feature.enabled();
    if (feature.IsVisibleInTray()) {
      feature_views_[feature_type] = AddScrollListCheckableItem(
          feature.icon(),
          l10n_util::GetStringUTF16(
              GetDataForFeature(feature_type).description),
          feature.enabled(), feature.IsEnterpriseIconVisible());
    }
  }

  if (feature_views_[FeatureType::kVirtualKeyboard]) {
    HoverHighlightView* virtual_keyboard_view =
        feature_views_[FeatureType::kVirtualKeyboard];
    virtual_keyboard_view->SetID(ash::VIEW_ID_ACCESSIBILITY_VIRTUAL_KEYBOARD);
    virtual_keyboard_view->right_view()->SetID(
        ash::VIEW_ID_ACCESSIBILITY_VIRTUAL_KEYBOARD_ENABLED);
  }

  if (IsAdditionalSettingsSeparatorVisibleInTray())
    scroll_content()->AddChildView(CreateListSubHeaderSeparator());

  if (IsAdditionalSettingsViewVisibleInTray()) {
    AddScrollListSubHeader(
        IDS_ASH_STATUS_TRAY_ACCESSIBILITY_ADDITIONAL_SETTINGS);
  }

  for (FeatureType feature_type : kSecondaryA11yOptions) {
    AccessibilityControllerImpl::Feature& feature =
        controller->GetFeature(feature_type);
    features_enabled_[feature_type] = feature.enabled();
    if (feature.IsVisibleInTray()) {
      feature_views_[feature_type] = AddScrollListCheckableItem(
          l10n_util::GetStringUTF16(
              GetDataForFeature(feature_type).description),
          feature.enabled(), feature.IsEnterpriseIconVisible());
    }
  }
}

void AccessibilityDetailedView::HandleViewClicked(views::View* view) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  using base::RecordAction;
  using base::UserMetricsAction;

  for (int feature_id = 0; feature_id < FeatureType::kFeatureCount;
       feature_id++) {
    AccessibilityControllerImpl::Feature& feature =
        controller->GetFeature(static_cast<FeatureType>(feature_id));
    if (feature_views_[feature_id] && view == feature_views_[feature_id] &&
        !feature.IsEnterpriseIconVisible()) {
      bool new_state = !feature.enabled();
      const FeatureData& feature_data = GetDataForFeature(feature.type());
      RecordAction(UserMetricsAction(new_state ? feature_data.action_enabled
                                               : feature_data.action_disabled));
      LogUserAccessibilityEvent(feature_data.event_id, new_state);

      // Close the system tray bubble as the available screen space has changed
      // E.g. there may not be enough screen space to display the current bubble
      // after enabling the docked magnifier or more space is made available
      // after disabling the docked magnifier.
      if (feature.type() == FeatureType::kDockedMagnifier)
        CloseBubble();

      feature.SetEnabled(new_state);
      break;
    }
  }
}

void AccessibilityDetailedView::HandleButtonPressed(views::Button* sender,
                                                    const ui::Event& event) {
  if (sender == help_view_)
    ShowHelp();
  else if (sender == settings_view_)
    ShowSettings();
}

void AccessibilityDetailedView::CreateExtraTitleRowButtons() {
  DCHECK(!help_view_);
  DCHECK(!settings_view_);

  tri_view()->SetContainerVisible(TriView::Container::END, true);

  help_view_ = CreateHelpButton();
  settings_view_ =
      CreateSettingsButton(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SETTINGS);
  tri_view()->AddView(TriView::Container::END, help_view_);
  tri_view()->AddView(TriView::Container::END, settings_view_);
}

void AccessibilityDetailedView::ShowSettings() {
  if (TrayPopupUtils::CanOpenWebUISettings()) {
    CloseBubble();  // Deletes |this|.
    Shell::Get()->system_tray_model()->client()->ShowAccessibilitySettings();
  }
}

void AccessibilityDetailedView::ShowHelp() {
  if (TrayPopupUtils::CanOpenWebUISettings()) {
    CloseBubble();  // Deletes |this|.
    Shell::Get()->system_tray_model()->client()->ShowAccessibilityHelp();
  }
}

}  // namespace tray
}  // namespace ash
