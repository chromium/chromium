// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/tray_accessibility.h"

#include <memory>
#include <utility>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/magnifier/docked_magnifier_controller_impl.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/command_line.h"
#include "base/metrics/user_metrics.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/separator.h"

namespace ash {
namespace {

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

}  // namespace

namespace tray {

////////////////////////////////////////////////////////////////////////////////
// ash::tray::AccessibilityDetailedView

AccessibilityDetailedView::AccessibilityDetailedView(
    DetailedViewDelegate* delegate)
    : TrayDetailedView(delegate) {
  Reset();
  AppendAccessibilityList();
  CreateTitleRow(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_TITLE);
  Layout();
}

void AccessibilityDetailedView::OnAccessibilityStatusChanged() {
  AccessibilityDelegate* delegate = Shell::Get()->accessibility_delegate();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();

  if (spoken_feedback_view_ &&
      controller->IsSpokenFeedbackSettingVisibleInTray()) {
    spoken_feedback_enabled_ = controller->spoken_feedback_enabled();
    TrayPopupUtils::UpdateCheckMarkVisibility(spoken_feedback_view_,
                                              spoken_feedback_enabled_);
  }

  if (select_to_speak_view_ &&
      controller->IsSelectToSpeakSettingVisibleInTray()) {
    select_to_speak_enabled_ = controller->select_to_speak_enabled();
    TrayPopupUtils::UpdateCheckMarkVisibility(select_to_speak_view_,
                                              select_to_speak_enabled_);
  }

  if (dictation_view_ && controller->IsDictationSettingVisibleInTray()) {
    dictation_enabled_ = controller->dictation_enabled();
    TrayPopupUtils::UpdateCheckMarkVisibility(dictation_view_,
                                              dictation_enabled_);
  }

  if (high_contrast_view_ && controller->IsHighContrastSettingVisibleInTray()) {
    high_contrast_enabled_ = controller->high_contrast_enabled();
    TrayPopupUtils::UpdateCheckMarkVisibility(high_contrast_view_,
                                              high_contrast_enabled_);
  }

  if (screen_magnifier_view_ &&
      controller->IsFullScreenMagnifierSettingVisibleInTray()) {
    screen_magnifier_enabled_ = delegate->IsMagnifierEnabled();
    TrayPopupUtils::UpdateCheckMarkVisibility(screen_magnifier_view_,
                                              screen_magnifier_enabled_);
  }

  if (docked_magnifier_view_ &&
      controller->IsDockedMagnifierSettingVisibleInTray()) {
    docked_magnifier_enabled_ =
        Shell::Get()->docked_magnifier_controller()->GetEnabled();
    TrayPopupUtils::UpdateCheckMarkVisibility(docked_magnifier_view_,
                                              docked_magnifier_enabled_);
  }

  if (autoclick_view_ && controller->IsAutoclickSettingVisibleInTray()) {
    autoclick_enabled_ = controller->autoclick_enabled();
    TrayPopupUtils::UpdateCheckMarkVisibility(autoclick_view_,
                                              autoclick_enabled_);
  }

  if (virtual_keyboard_view_ &&
      controller->IsVirtualKeyboardSettingVisibleInTray()) {
    virtual_keyboard_enabled_ = controller->virtual_keyboard_enabled();
    TrayPopupUtils::UpdateCheckMarkVisibility(virtual_keyboard_view_,
                                              virtual_keyboard_enabled_);
  }

  if (switch_access_view_ && controller->IsSwitchAccessSettingVisibleInTray()) {
    switch_access_enabled_ = controller->switch_access_enabled();
    TrayPopupUtils::UpdateCheckMarkVisibility(switch_access_view_,
                                              switch_access_enabled_);
  }

  if (large_cursor_view_ && controller->IsLargeCursorSettingVisibleInTray()) {
    large_cursor_enabled_ = controller->large_cursor_enabled();
    TrayPopupUtils::UpdateCheckMarkVisibility(large_cursor_view_,
                                              large_cursor_enabled_);
  }

  if (mono_audio_view_ && controller->IsMonoAudioSettingVisibleInTray()) {
    mono_audio_enabled_ = controller->mono_audio_enabled();
    TrayPopupUtils::UpdateCheckMarkVisibility(mono_audio_view_,
                                              mono_audio_enabled_);
  }

  if (caret_highlight_view_ &&
      controller->IsCaretHighlightSettingVisibleInTray()) {
    caret_highlight_enabled_ = controller->caret_highlight_enabled();
    TrayPopupUtils::UpdateCheckMarkVisibility(caret_highlight_view_,
                                              caret_highlight_enabled_);
  }

  if (highlight_mouse_cursor_view_ &&
      controller->IsCursorHighlightSettingVisibleInTray()) {
    highlight_mouse_cursor_enabled_ = controller->cursor_highlight_enabled();
    TrayPopupUtils::UpdateCheckMarkVisibility(highlight_mouse_cursor_view_,
                                              highlight_mouse_cursor_enabled_);
  }

  if (highlight_keyboard_focus_view_ &&
      controller->IsFocusHighlightSettingVisibleInTray()) {
    highlight_keyboard_focus_enabled_ = controller->focus_highlight_enabled();
    TrayPopupUtils::UpdateCheckMarkVisibility(
        highlight_keyboard_focus_view_, highlight_keyboard_focus_enabled_);
  }

  if (sticky_keys_view_ && controller->IsStickyKeysSettingVisibleInTray()) {
    sticky_keys_enabled_ = controller->sticky_keys_enabled();
    TrayPopupUtils::UpdateCheckMarkVisibility(sticky_keys_view_,
                                              sticky_keys_enabled_);
  }
}

const char* AccessibilityDetailedView::GetClassName() const {
  return "AccessibilityDetailedView";
}

void AccessibilityDetailedView::AppendAccessibilityList() {
  CreateScrollableList();

  AccessibilityDelegate* delegate = Shell::Get()->accessibility_delegate();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();

  if (controller->IsSpokenFeedbackSettingVisibleInTray()) {
    spoken_feedback_enabled_ = controller->spoken_feedback_enabled();
    spoken_feedback_view_ = AddScrollListCheckableItem(
        kSystemMenuAccessibilityChromevoxIcon,
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SPOKEN_FEEDBACK),
        spoken_feedback_enabled_,
        controller->IsEnterpriseIconVisibleForSpokenFeedback());
  }

  if (controller->IsSelectToSpeakSettingVisibleInTray()) {
    select_to_speak_enabled_ = controller->select_to_speak_enabled();
    select_to_speak_view_ = AddScrollListCheckableItem(
        kSystemMenuAccessibilitySelectToSpeakIcon,
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SELECT_TO_SPEAK),
        select_to_speak_enabled_,
        controller->IsEnterpriseIconVisibleForSelectToSpeak());
  }

  if (controller->IsDictationSettingVisibleInTray()) {
    dictation_enabled_ = controller->dictation_enabled();
    dictation_view_ = AddScrollListCheckableItem(
        kDictationMenuIcon,
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_DICTATION),
        dictation_enabled_, controller->IsEnterpriseIconVisibleForDictation());
  }

  if (controller->IsHighContrastSettingVisibleInTray()) {
    high_contrast_enabled_ = controller->high_contrast_enabled();
    high_contrast_view_ = AddScrollListCheckableItem(
        kSystemMenuAccessibilityContrastIcon,
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_ACCESSIBILITY_HIGH_CONTRAST_MODE),
        high_contrast_enabled_,
        controller->IsEnterpriseIconVisibleForHighContrast());
  }

  if (controller->IsFullScreenMagnifierSettingVisibleInTray()) {
    screen_magnifier_enabled_ = delegate->IsMagnifierEnabled();
    screen_magnifier_view_ = AddScrollListCheckableItem(
        kSystemMenuAccessibilityFullscreenMagnifierIcon,
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SCREEN_MAGNIFIER),
        screen_magnifier_enabled_,
        controller->IsEnterpriseIconVisibleForFullScreenMagnifier());
  }

  if (controller->IsDockedMagnifierSettingVisibleInTray()) {
    docked_magnifier_enabled_ =
        Shell::Get()->docked_magnifier_controller()->GetEnabled();
    docked_magnifier_view_ = AddScrollListCheckableItem(
        kSystemMenuAccessibilityDockedMagnifierIcon,
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_ACCESSIBILITY_DOCKED_MAGNIFIER),
        docked_magnifier_enabled_,
        controller->IsEnterpriseIconVisibleForDockedMagnifier());
  }

  if (controller->IsAutoclickSettingVisibleInTray()) {
    autoclick_enabled_ = controller->autoclick_enabled();
    autoclick_view_ = AddScrollListCheckableItem(
        kSystemMenuAccessibilityAutoClickIcon,
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_AUTOCLICK),
        autoclick_enabled_, controller->IsEnterpriseIconVisibleForAutoclick());
  }

  if (controller->IsVirtualKeyboardSettingVisibleInTray()) {
    virtual_keyboard_enabled_ = controller->virtual_keyboard_enabled();
    virtual_keyboard_view_ = AddScrollListCheckableItem(
        kSystemMenuKeyboardIcon,
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_ACCESSIBILITY_VIRTUAL_KEYBOARD),
        virtual_keyboard_enabled_,
        controller->IsEnterpriseIconVisibleForVirtualKeyboard());
    virtual_keyboard_view_->SetID(ash::VIEW_ID_ACCESSIBILITY_VIRTUAL_KEYBOARD);
    virtual_keyboard_view_->right_view()->SetID(
        ash::VIEW_ID_ACCESSIBILITY_VIRTUAL_KEYBOARD_ENABLED);
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalAccessibilitySwitchAccess) &&
      controller->IsSwitchAccessSettingVisibleInTray()) {
    switch_access_enabled_ = controller->switch_access_enabled();
    switch_access_view_ = AddScrollListCheckableItem(
        kSwitchAccessIcon,
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SWITCH_ACCESS),
        switch_access_enabled_,
        controller->IsEnterpriseIconVisibleForSwitchAccess());
  }

  if (controller->IsAdditionalSettingsSeparatorVisibleInTray())
    scroll_content()->AddChildView(CreateListSubHeaderSeparator());

  if (controller->IsAdditionalSettingsViewVisibleInTray()) {
    AddScrollListSubHeader(
        IDS_ASH_STATUS_TRAY_ACCESSIBILITY_ADDITIONAL_SETTINGS);
  }

  if (controller->IsLargeCursorSettingVisibleInTray()) {
    large_cursor_enabled_ = controller->large_cursor_enabled();
    large_cursor_view_ = AddScrollListCheckableItem(
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_ACCESSIBILITY_LARGE_CURSOR),
        large_cursor_enabled_,
        controller->IsEnterpriseIconVisibleForLargeCursor());
  }

  if (controller->IsMonoAudioSettingVisibleInTray()) {
    mono_audio_enabled_ = controller->mono_audio_enabled();
    mono_audio_view_ = AddScrollListCheckableItem(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_MONO_AUDIO),
        mono_audio_enabled_, controller->IsEnterpriseIconVisibleForMonoAudio());
  }

  if (controller->IsCaretHighlightSettingVisibleInTray()) {
    caret_highlight_enabled_ = controller->caret_highlight_enabled();
    caret_highlight_view_ = AddScrollListCheckableItem(
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_ACCESSIBILITY_CARET_HIGHLIGHT),
        caret_highlight_enabled_,
        controller->IsEnterpriseIconVisibleForCaretHighlight());
  }

  if (controller->IsCursorHighlightSettingVisibleInTray()) {
    highlight_mouse_cursor_enabled_ = controller->cursor_highlight_enabled();
    highlight_mouse_cursor_view_ = AddScrollListCheckableItem(
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_ACCESSIBILITY_HIGHLIGHT_MOUSE_CURSOR),
        highlight_mouse_cursor_enabled_,
        controller->IsEnterpriseIconVisibleForCursorHighlight());
  }
  // Focus highlighting can't be on when spoken feedback is on because
  // ChromeVox does its own focus highlighting.
  if (!spoken_feedback_enabled_ &&
      controller->IsFocusHighlightSettingVisibleInTray()) {
    highlight_keyboard_focus_enabled_ = controller->focus_highlight_enabled();
    highlight_keyboard_focus_view_ = AddScrollListCheckableItem(
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_ACCESSIBILITY_HIGHLIGHT_KEYBOARD_FOCUS),
        highlight_keyboard_focus_enabled_,
        controller->IsEnterpriseIconVisibleForFocusHighlight());
  }

  if (controller->IsStickyKeysSettingVisibleInTray()) {
    sticky_keys_enabled_ = controller->sticky_keys_enabled();
    sticky_keys_view_ = AddScrollListCheckableItem(
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_ACCESSIBILITY_STICKY_KEYS),
        sticky_keys_enabled_,
        controller->IsEnterpriseIconVisibleForStickyKeys());
  }
}

void AccessibilityDetailedView::HandleViewClicked(views::View* view) {
  AccessibilityDelegate* delegate = Shell::Get()->accessibility_delegate();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  using base::RecordAction;
  using base::UserMetricsAction;
  if (spoken_feedback_view_ && view == spoken_feedback_view_ &&
      !controller->IsEnterpriseIconVisibleForSpokenFeedback()) {
    bool new_state = !controller->spoken_feedback_enabled();
    RecordAction(new_state
                     ? UserMetricsAction("StatusArea_SpokenFeedbackEnabled")
                     : UserMetricsAction("StatusArea_SpokenFeedbackDisabled"));
    controller->SetSpokenFeedbackEnabled(new_state, A11Y_NOTIFICATION_NONE);
  } else if (select_to_speak_view_ && view == select_to_speak_view_ &&
             !controller->IsEnterpriseIconVisibleForSelectToSpeak()) {
    bool new_state = !controller->select_to_speak_enabled();
    RecordAction(new_state
                     ? UserMetricsAction("StatusArea_SelectToSpeakEnabled")
                     : UserMetricsAction("StatusArea_SelectToSpeakDisabled"));
    controller->SetSelectToSpeakEnabled(new_state);
  } else if (dictation_view_ && view == dictation_view_ &&
             !controller->IsEnterpriseIconVisibleForDictation()) {
    bool new_state = !controller->dictation_enabled();
    RecordAction(new_state ? UserMetricsAction("StatusArea_DictationEnabled")
                           : UserMetricsAction("StatusArea_DictationDisabled"));
    controller->SetDictationEnabled(new_state);
  } else if (high_contrast_view_ && view == high_contrast_view_ &&
             !controller->IsEnterpriseIconVisibleForHighContrast()) {
    bool new_state = !controller->high_contrast_enabled();
    RecordAction(new_state
                     ? UserMetricsAction("StatusArea_HighContrastEnabled")
                     : UserMetricsAction("StatusArea_HighContrastDisabled"));
    controller->SetHighContrastEnabled(new_state);
  } else if (screen_magnifier_view_ && view == screen_magnifier_view_ &&
             !controller->IsEnterpriseIconVisibleForFullScreenMagnifier()) {
    RecordAction(delegate->IsMagnifierEnabled()
                     ? UserMetricsAction("StatusArea_MagnifierDisabled")
                     : UserMetricsAction("StatusArea_MagnifierEnabled"));
    delegate->SetMagnifierEnabled(!delegate->IsMagnifierEnabled());
  } else if (docked_magnifier_view_ && view == docked_magnifier_view_ &&
             !controller->IsEnterpriseIconVisibleForDockedMagnifier()) {
    auto* docked_magnifier_controller =
        Shell::Get()->docked_magnifier_controller();
    const bool new_state = !docked_magnifier_controller->GetEnabled();

    // Close the system tray bubble as the available screen space has changed
    // E.g. there may not be enough screen space to display the current bubble
    // after enabling the docked magnifier or more space is made available after
    // disabling the docked magnifier.
    CloseBubble();

    RecordAction(new_state
                     ? UserMetricsAction("StatusArea_DockedMagnifierEnabled")
                     : UserMetricsAction("StatusArea_DockedMagnifierDisabled"));
    docked_magnifier_controller->SetEnabled(new_state);
  } else if (large_cursor_view_ && view == large_cursor_view_ &&
             !controller->IsEnterpriseIconVisibleForLargeCursor()) {
    bool new_state = !controller->large_cursor_enabled();
    RecordAction(new_state
                     ? UserMetricsAction("StatusArea_LargeCursorEnabled")
                     : UserMetricsAction("StatusArea_LargeCursorDisabled"));
    controller->SetLargeCursorEnabled(new_state);
  } else if (autoclick_view_ && view == autoclick_view_ &&
             !controller->IsEnterpriseIconVisibleForAutoclick()) {
    bool new_state = !controller->autoclick_enabled();
    RecordAction(new_state ? UserMetricsAction("StatusArea_AutoClickEnabled")
                           : UserMetricsAction("StatusArea_AutoClickDisabled"));
    controller->SetAutoclickEnabled(new_state);
  } else if (virtual_keyboard_view_ && view == virtual_keyboard_view_ &&
             !controller->IsEnterpriseIconVisibleForVirtualKeyboard()) {
    bool new_state = !controller->virtual_keyboard_enabled();
    RecordAction(new_state
                     ? UserMetricsAction("StatusArea_VirtualKeyboardEnabled")
                     : UserMetricsAction("StatusArea_VirtualKeyboardDisabled"));
    controller->SetVirtualKeyboardEnabled(new_state);
  } else if (switch_access_view_ && view == switch_access_view_ &&
             !controller->IsEnterpriseIconVisibleForSwitchAccess()) {
    bool new_state = !controller->switch_access_enabled();
    RecordAction(new_state
                     ? UserMetricsAction("StatusArea_SwitchAccessEnabled")
                     : UserMetricsAction("StatusArea_SwitchAccessDisabled"));
    controller->SetSwitchAccessEnabled(new_state);
  } else if (caret_highlight_view_ && view == caret_highlight_view_ &&
             !controller->IsEnterpriseIconVisibleForCaretHighlight()) {
    bool new_state = !controller->caret_highlight_enabled();
    RecordAction(new_state
                     ? UserMetricsAction("StatusArea_CaretHighlightEnabled")
                     : UserMetricsAction("StatusArea_CaretHighlightDisabled"));
    controller->SetCaretHighlightEnabled(new_state);
  } else if (mono_audio_view_ && view == mono_audio_view_ &&
             !controller->IsEnterpriseIconVisibleForMonoAudio()) {
    bool new_state = !controller->mono_audio_enabled();
    RecordAction(new_state ? UserMetricsAction("StatusArea_MonoAudioEnabled")
                           : UserMetricsAction("StatusArea_MonoAudioDisabled"));
    controller->SetMonoAudioEnabled(new_state);
  } else if (highlight_mouse_cursor_view_ &&
             view == highlight_mouse_cursor_view_ &&
             !controller->IsEnterpriseIconVisibleForCursorHighlight()) {
    bool new_state = !controller->cursor_highlight_enabled();
    RecordAction(
        new_state
            ? UserMetricsAction("StatusArea_HighlightMouseCursorEnabled")
            : UserMetricsAction("StatusArea_HighlightMouseCursorDisabled"));
    controller->SetCursorHighlightEnabled(new_state);
  } else if (highlight_keyboard_focus_view_ &&
             view == highlight_keyboard_focus_view_ &&
             !controller->IsEnterpriseIconVisibleForFocusHighlight()) {
    bool new_state = !controller->focus_highlight_enabled();
    RecordAction(
        new_state
            ? UserMetricsAction("StatusArea_HighlightKeyboardFocusEnabled")
            : UserMetricsAction("StatusArea_HighlightKeyboardFocusDisabled"));
    controller->SetFocusHighlightEnabled(new_state);
  } else if (sticky_keys_view_ && view == sticky_keys_view_ &&
             !controller->IsEnterpriseIconVisibleForStickyKeys()) {
    bool new_state = !controller->sticky_keys_enabled();
    RecordAction(new_state
                     ? UserMetricsAction("StatusArea_StickyKeysEnabled")
                     : UserMetricsAction("StatusArea_StickyKeysDisabled"));
    controller->SetStickyKeysEnabled(new_state);
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
