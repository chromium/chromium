// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/accessibility_detailed_view.h"

#include <memory>
#include <utility>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/rounded_container.h"
#include "ash/system/machine_learning/user_settings_event_logger.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/soda/soda_installer.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/separator.h"

namespace ash {
namespace {

using ml::UserSettingsEvent;

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
  A11Y_LIVE_CAPTION = 1 << 15,
};

void LogUserAccessibilityEvent(UserSettingsEvent::Event::AccessibilityId id,
                               bool new_state) {
  auto* logger = ml::UserSettingsEventLogger::Get();
  if (logger) {
    logger->LogAccessibilityUkmEvent(id, new_state);
  }
}

speech::LanguageCode GetSodaFeatureLocale(SodaFeature feature) {
  std::string feature_locale = speech::kUsEnglishLocale;
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (pref_service) {
    switch (feature) {
      case SodaFeature::kDictation:
        feature_locale =
            pref_service->GetString(prefs::kAccessibilityDictationLocale);
        break;
      case SodaFeature::kLiveCaption:
        feature_locale = ::prefs::GetLiveCaptionLanguageCode(pref_service);
        break;
    }
  }
  return speech::GetLanguageCode(feature_locale);
}

bool IsSodaFeatureEnabled(SodaFeature feature) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  switch (feature) {
    case SodaFeature::kDictation:
      return ::features::IsDictationOfflineAvailable() &&
             controller->dictation().enabled();
    case SodaFeature::kLiveCaption:
      return controller->live_caption().enabled();
  }
}

bool SodaFeatureHasUpdate(SodaFeature feature,
                          speech::LanguageCode language_code) {
  // Only show updates for this feature if the language code applies to the SODA
  // binary (encoded by by LanguageCode::kNone) or the language pack matching
  // the feature locale.
  return language_code == speech::LanguageCode::kNone ||
         language_code == GetSodaFeatureLocale(feature);
}

// Updates the check mark to `checked` on `view1` and `view2` if the views
// exist.
void UpdateCheckMark(bool checked,
                     HoverHighlightView* view1,
                     HoverHighlightView* view2) {
  if (view1)
    TrayPopupUtils::UpdateCheckMarkVisibility(view1, checked);
  if (view2)
    TrayPopupUtils::UpdateCheckMarkVisibility(view2, checked);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AccessibilityDetailedView

const char AccessibilityDetailedView::kClassName[] = "AccessibilityDetailedView";

AccessibilityDetailedView::AccessibilityDetailedView(
    DetailedViewDelegate* delegate)
    : TrayDetailedView(delegate) {
  Reset();
  AppendAccessibilityList();
  CreateTitleRow(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_TITLE);
  Layout();

  if (!::features::IsDictationOfflineAvailable() &&
      !captions::IsLiveCaptionFeatureSupported()) {
    return;
  }
  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  if (soda_installer)
    soda_installer->AddObserver(this);
}

AccessibilityDetailedView::~AccessibilityDetailedView() {
  if (!::features::IsDictationOfflineAvailable() &&
      !captions::IsLiveCaptionFeatureSupported()) {
    return;
  }
  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  // `soda_installer` is not guaranteed to be valid, since it's possible for
  // this class to out-live it. This means that this class cannot use
  // ScopedObservation and needs to manage removing the observer itself.
  if (soda_installer)
    soda_installer->RemoveObserver(this);
}

void AccessibilityDetailedView::OnAccessibilityStatusChanged() {
  AccessibilityDelegate* delegate = Shell::Get()->accessibility_delegate();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();

  if (controller->IsSpokenFeedbackSettingVisibleInTray()) {
    bool checked = controller->spoken_feedback().enabled();
    UpdateCheckMark(checked, spoken_feedback_view_, spoken_feedback_top_view_);
  }

  if (controller->IsSelectToSpeakSettingVisibleInTray()) {
    bool checked = controller->select_to_speak().enabled();
    UpdateCheckMark(checked, select_to_speak_view_, select_to_speak_top_view_);
  }

  if (controller->IsDictationSettingVisibleInTray()) {
    bool checked = controller->dictation().enabled();
    UpdateCheckMark(checked, dictation_view_, dictation_top_view_);
  }

  if (controller->IsHighContrastSettingVisibleInTray()) {
    bool checked = controller->high_contrast().enabled();
    UpdateCheckMark(checked, high_contrast_view_, high_contrast_top_view_);
  }

  if (controller->IsFullScreenMagnifierSettingVisibleInTray()) {
    bool checked = delegate->IsMagnifierEnabled();
    UpdateCheckMark(checked, screen_magnifier_view_,
                    screen_magnifier_top_view_);
  }

  if (controller->IsDockedMagnifierSettingVisibleInTray()) {
    bool checked = Shell::Get()->docked_magnifier_controller()->GetEnabled();
    UpdateCheckMark(checked, docked_magnifier_view_,
                    docked_magnifier_top_view_);
  }

  if (controller->IsAutoclickSettingVisibleInTray()) {
    bool checked = controller->autoclick().enabled();
    UpdateCheckMark(checked, autoclick_view_, autoclick_top_view_);
  }

  if (controller->IsVirtualKeyboardSettingVisibleInTray()) {
    bool checked = controller->virtual_keyboard().enabled();
    UpdateCheckMark(checked, virtual_keyboard_view_,
                    virtual_keyboard_top_view_);
  }

  if (controller->IsSwitchAccessSettingVisibleInTray()) {
    bool checked = controller->switch_access().enabled();
    UpdateCheckMark(checked, switch_access_view_, switch_access_top_view_);
  }

  if (controller->IsLiveCaptionSettingVisibleInTray()) {
    bool checked = controller->live_caption().enabled();
    UpdateCheckMark(checked, live_caption_view_, live_caption_top_view_);
  }

  if (controller->IsLargeCursorSettingVisibleInTray()) {
    bool checked = controller->large_cursor().enabled();
    UpdateCheckMark(checked, large_cursor_view_, large_cursor_top_view_);
  }

  if (controller->IsMonoAudioSettingVisibleInTray()) {
    bool checked = controller->mono_audio().enabled();
    UpdateCheckMark(checked, mono_audio_view_, mono_audio_top_view_);
  }

  if (controller->IsCaretHighlightSettingVisibleInTray()) {
    bool checked = controller->caret_highlight().enabled();
    UpdateCheckMark(checked, caret_highlight_view_, caret_highlight_top_view_);
  }

  if (controller->IsCursorHighlightSettingVisibleInTray()) {
    bool checked = controller->cursor_highlight().enabled();
    UpdateCheckMark(checked, highlight_mouse_cursor_view_,
                    highlight_mouse_cursor_top_view_);
  }

  if (controller->IsFocusHighlightSettingVisibleInTray()) {
    bool checked = controller->focus_highlight().enabled();
    UpdateCheckMark(checked, highlight_keyboard_focus_view_,
                    highlight_keyboard_focus_top_view_);
  }

  if (controller->IsStickyKeysSettingVisibleInTray()) {
    bool checked = controller->sticky_keys().enabled();
    UpdateCheckMark(checked, sticky_keys_view_, sticky_keys_top_view_);
  }
}

const char* AccessibilityDetailedView::GetClassName() const {
  return kClassName;
}

void AccessibilityDetailedView::AppendAccessibilityList() {
  CreateScrollableList();

  views::View* container = scroll_content();
  if (features::IsQsRevampEnabled()) {
    container =
        scroll_content()->AddChildView(std::make_unique<RoundedContainer>());

    AddEnabledFeatures(container);
  }

  AddAllFeatures(container);
}

void AccessibilityDetailedView::AddEnabledFeatures(views::View* container) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();

  if (controller->IsSpokenFeedbackSettingVisibleInTray() &&
      controller->spoken_feedback().enabled()) {
    spoken_feedback_top_view_ = AddSpokenFeedbackView(container);
  }
  if (controller->IsSelectToSpeakSettingVisibleInTray() &&
      controller->select_to_speak().enabled()) {
    select_to_speak_top_view_ = AddSelectToSpeakView(container);
  }
  if (controller->IsDictationSettingVisibleInTray() &&
      controller->dictation().enabled()) {
    dictation_top_view_ = AddDictationView(container);
  }
  if (controller->IsHighContrastSettingVisibleInTray() &&
      controller->high_contrast().enabled()) {
    high_contrast_top_view_ = AddHighContrastView(container);
  }
  if (controller->IsFullScreenMagnifierSettingVisibleInTray() &&
      Shell::Get()->accessibility_delegate()->IsMagnifierEnabled()) {
    screen_magnifier_top_view_ = AddScreenMagnifierView(container);
  }
  if (controller->IsDockedMagnifierSettingVisibleInTray() &&
      Shell::Get()->docked_magnifier_controller()->GetEnabled()) {
    docked_magnifier_top_view_ = AddDockedMagnifierView(container);
  }
  if (controller->IsAutoclickSettingVisibleInTray() &&
      controller->autoclick().enabled()) {
    autoclick_top_view_ = AddAutoclickView(container);
  }
  if (controller->IsVirtualKeyboardSettingVisibleInTray() &&
      controller->virtual_keyboard().enabled()) {
    virtual_keyboard_top_view_ = AddVirtualKeyboardView(container);
  }
  if (controller->IsSwitchAccessSettingVisibleInTray() &&
      controller->switch_access().enabled()) {
    switch_access_top_view_ = AddSwitchAccessView(container);
  }
  if (controller->IsLiveCaptionSettingVisibleInTray() &&
      controller->live_caption().enabled()) {
    live_caption_top_view_ = AddLiveCaptionView(container);
  }
  if (controller->IsLargeCursorSettingVisibleInTray() &&
      controller->large_cursor().enabled()) {
    large_cursor_top_view_ = AddLargeCursorView(container);
  }
  if (controller->IsMonoAudioSettingVisibleInTray() &&
      controller->mono_audio().enabled()) {
    mono_audio_top_view_ = AddMonoAudioView(container);
  }
  if (controller->IsCaretHighlightSettingVisibleInTray() &&
      controller->caret_highlight().enabled()) {
    caret_highlight_top_view_ = AddCaretHighlightView(container);
  }
  if (controller->IsCursorHighlightSettingVisibleInTray() &&
      controller->cursor_highlight().enabled()) {
    highlight_mouse_cursor_top_view_ = AddHighlightMouseCursorView(container);
  }
  // Focus highlighting can't be on when spoken feedback is on because
  // ChromeVox does its own focus highlighting.
  if (!controller->spoken_feedback().enabled() &&
      controller->IsFocusHighlightSettingVisibleInTray() &&
      controller->focus_highlight().enabled()) {
    highlight_keyboard_focus_top_view_ =
        AddHighlightKeyboardFocusView(container);
  }
  if (controller->IsStickyKeysSettingVisibleInTray() &&
      controller->sticky_keys().enabled()) {
    sticky_keys_top_view_ = AddStickyKeysView(container);
  }

  // Add a divider line if any features were added above.
  if (!container->children().empty()) {
    container->AddChildView(TrayPopupUtils::CreateListSubHeaderSeparator());
  }
}

void AccessibilityDetailedView::AddAllFeatures(views::View* container) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();

  if (controller->IsSpokenFeedbackSettingVisibleInTray()) {
    spoken_feedback_view_ = AddSpokenFeedbackView(container);
  }

  if (controller->IsSelectToSpeakSettingVisibleInTray()) {
    select_to_speak_view_ = AddSelectToSpeakView(container);
  }

  if (controller->IsDictationSettingVisibleInTray()) {
    dictation_view_ = AddDictationView(container);
  }

  if (controller->IsHighContrastSettingVisibleInTray()) {
    high_contrast_view_ = AddHighContrastView(container);
  }

  if (controller->IsFullScreenMagnifierSettingVisibleInTray()) {
    screen_magnifier_view_ = AddScreenMagnifierView(container);
  }

  if (controller->IsDockedMagnifierSettingVisibleInTray()) {
    docked_magnifier_view_ = AddDockedMagnifierView(container);
  }

  if (controller->IsAutoclickSettingVisibleInTray()) {
    autoclick_view_ = AddAutoclickView(container);
  }

  if (controller->IsVirtualKeyboardSettingVisibleInTray()) {
    virtual_keyboard_view_ = AddVirtualKeyboardView(container);
    virtual_keyboard_view_->SetID(ash::VIEW_ID_ACCESSIBILITY_VIRTUAL_KEYBOARD);
    virtual_keyboard_view_->right_view()->SetID(
        ash::VIEW_ID_ACCESSIBILITY_VIRTUAL_KEYBOARD_ENABLED);
  }

  if (controller->IsSwitchAccessSettingVisibleInTray()) {
    switch_access_view_ = AddSwitchAccessView(container);
  }

  if (controller->IsLiveCaptionSettingVisibleInTray()) {
    live_caption_view_ = AddLiveCaptionView(container);
  }

  if (controller->IsAdditionalSettingsSeparatorVisibleInTray()) {
    container->AddChildView(TrayPopupUtils::CreateListSubHeaderSeparator());
  }

  if (controller->IsAdditionalSettingsViewVisibleInTray()) {
    AddScrollListSubHeader(
        container, gfx::kNoneIcon,
        IDS_ASH_STATUS_TRAY_ACCESSIBILITY_ADDITIONAL_SETTINGS);
  }

  if (controller->IsLargeCursorSettingVisibleInTray()) {
    large_cursor_view_ = AddLargeCursorView(container);
  }

  if (controller->IsMonoAudioSettingVisibleInTray()) {
    mono_audio_view_ = AddMonoAudioView(container);
  }

  if (controller->IsCaretHighlightSettingVisibleInTray()) {
    caret_highlight_view_ = AddCaretHighlightView(container);
  }

  if (controller->IsCursorHighlightSettingVisibleInTray()) {
    highlight_mouse_cursor_view_ = AddHighlightMouseCursorView(container);
  }
  // Focus highlighting can't be on when spoken feedback is on because
  // ChromeVox does its own focus highlighting.
  if (!controller->spoken_feedback().enabled() &&
      controller->IsFocusHighlightSettingVisibleInTray()) {
    highlight_keyboard_focus_view_ = AddHighlightKeyboardFocusView(container);
  }

  if (controller->IsStickyKeysSettingVisibleInTray()) {
    sticky_keys_view_ = AddStickyKeysView(container);
  }
}

HoverHighlightView* AccessibilityDetailedView::AddSpokenFeedbackView(
    views::View* container) {
  auto* controller = Shell::Get()->accessibility_controller();
  bool checked = controller->spoken_feedback().enabled();
  return AddScrollListCheckableItem(
      container, kSystemMenuAccessibilityChromevoxIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SPOKEN_FEEDBACK),
      checked, controller->IsEnterpriseIconVisibleForSpokenFeedback());
}

HoverHighlightView* AccessibilityDetailedView::AddSelectToSpeakView(
    views::View* container) {
  auto* controller = Shell::Get()->accessibility_controller();
  bool checked = controller->select_to_speak().enabled();
  return AddScrollListCheckableItem(
      container, kSystemMenuAccessibilitySelectToSpeakIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SELECT_TO_SPEAK),
      checked, controller->IsEnterpriseIconVisibleForSelectToSpeak());
}

HoverHighlightView* AccessibilityDetailedView::AddDictationView(
    views::View* container) {
  auto* controller = Shell::Get()->accessibility_controller();
  bool checked = controller->dictation().enabled();
  return AddScrollListCheckableItem(
      container, kDictationMenuIcon,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_DICTATION),
      checked, controller->IsEnterpriseIconVisibleForDictation());
}

HoverHighlightView* AccessibilityDetailedView::AddHighContrastView(
    views::View* container) {
  auto* controller = Shell::Get()->accessibility_controller();
  bool checked = controller->high_contrast().enabled();
  return AddScrollListCheckableItem(
      container, kSystemMenuAccessibilityContrastIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY_HIGH_CONTRAST_MODE),
      checked, controller->IsEnterpriseIconVisibleForHighContrast());
}

HoverHighlightView* AccessibilityDetailedView::AddScreenMagnifierView(
    views::View* container) {
  auto* controller = Shell::Get()->accessibility_controller();
  bool checked = Shell::Get()->accessibility_delegate()->IsMagnifierEnabled();
  return AddScrollListCheckableItem(
      container, kSystemMenuAccessibilityFullscreenMagnifierIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SCREEN_MAGNIFIER),
      checked, controller->IsEnterpriseIconVisibleForFullScreenMagnifier());
}

HoverHighlightView* AccessibilityDetailedView::AddDockedMagnifierView(
    views::View* container) {
  auto* controller = Shell::Get()->accessibility_controller();
  bool checked = Shell::Get()->docked_magnifier_controller()->GetEnabled();
  return AddScrollListCheckableItem(
      container, kSystemMenuAccessibilityDockedMagnifierIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY_DOCKED_MAGNIFIER),
      checked, controller->IsEnterpriseIconVisibleForDockedMagnifier());
}

HoverHighlightView* AccessibilityDetailedView::AddAutoclickView(
    views::View* container) {
  auto* controller = Shell::Get()->accessibility_controller();
  bool checked = controller->autoclick().enabled();
  return AddScrollListCheckableItem(
      container, kSystemMenuAccessibilityAutoClickIcon,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_AUTOCLICK),
      checked, controller->IsEnterpriseIconVisibleForAutoclick());
}

HoverHighlightView* AccessibilityDetailedView::AddVirtualKeyboardView(
    views::View* container) {
  auto* controller = Shell::Get()->accessibility_controller();
  bool checked = controller->virtual_keyboard().enabled();
  return AddScrollListCheckableItem(
      container, kSystemMenuKeyboardLegacyIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY_VIRTUAL_KEYBOARD),
      checked, controller->IsEnterpriseIconVisibleForVirtualKeyboard());
}

HoverHighlightView* AccessibilityDetailedView::AddSwitchAccessView(
    views::View* container) {
  auto* controller = Shell::Get()->accessibility_controller();
  bool checked = controller->switch_access().enabled();
  return AddScrollListCheckableItem(
      container, kSwitchAccessIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SWITCH_ACCESS),
      checked, controller->IsEnterpriseIconVisibleForSwitchAccess());
}

HoverHighlightView* AccessibilityDetailedView::AddLiveCaptionView(
    views::View* container) {
  auto* controller = Shell::Get()->accessibility_controller();
  bool checked = controller->live_caption().enabled();
  return AddScrollListCheckableItem(
      container, vector_icons::kLiveCaptionOnIcon,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LIVE_CAPTION), checked,
      controller->IsEnterpriseIconVisibleForLiveCaption());
}

HoverHighlightView* AccessibilityDetailedView::AddLargeCursorView(
    views::View* container) {
  auto* controller = Shell::Get()->accessibility_controller();
  bool checked = controller->large_cursor().enabled();
  return AddScrollListCheckableItem(
      container, gfx::kNoneIcon,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_LARGE_CURSOR),
      checked, controller->IsEnterpriseIconVisibleForLargeCursor());
}

HoverHighlightView* AccessibilityDetailedView::AddMonoAudioView(
    views::View* container) {
  auto* controller = Shell::Get()->accessibility_controller();
  bool checked = controller->mono_audio().enabled();
  return AddScrollListCheckableItem(
      container, gfx::kNoneIcon,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_MONO_AUDIO),
      checked, controller->IsEnterpriseIconVisibleForMonoAudio());
}

HoverHighlightView* AccessibilityDetailedView::AddCaretHighlightView(
    views::View* container) {
  auto* controller = Shell::Get()->accessibility_controller();
  bool checked = controller->caret_highlight().enabled();
  return AddScrollListCheckableItem(
      container, gfx::kNoneIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY_CARET_HIGHLIGHT),
      checked, controller->IsEnterpriseIconVisibleForCaretHighlight());
}

HoverHighlightView* AccessibilityDetailedView::AddHighlightMouseCursorView(
    views::View* container) {
  auto* controller = Shell::Get()->accessibility_controller();
  bool checked = controller->cursor_highlight().enabled();
  return AddScrollListCheckableItem(
      container, gfx::kNoneIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY_HIGHLIGHT_MOUSE_CURSOR),
      checked, controller->IsEnterpriseIconVisibleForCursorHighlight());
}

HoverHighlightView* AccessibilityDetailedView::AddHighlightKeyboardFocusView(
    views::View* container) {
  auto* controller = Shell::Get()->accessibility_controller();
  bool checked = controller->focus_highlight().enabled();
  return AddScrollListCheckableItem(
      container, gfx::kNoneIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY_HIGHLIGHT_KEYBOARD_FOCUS),
      checked, controller->IsEnterpriseIconVisibleForFocusHighlight());
}

HoverHighlightView* AccessibilityDetailedView::AddStickyKeysView(
    views::View* container) {
  auto* controller = Shell::Get()->accessibility_controller();
  bool checked = controller->sticky_keys().enabled();
  return AddScrollListCheckableItem(
      container, gfx::kNoneIcon,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_STICKY_KEYS),
      checked, controller->IsEnterpriseIconVisibleForStickyKeys());
}

void AccessibilityDetailedView::HandleViewClicked(views::View* view) {
  AccessibilityDelegate* delegate = Shell::Get()->accessibility_delegate();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  using base::RecordAction;
  using base::UserMetricsAction;
  // Since `view` is never null, there's no need to check for the existence of
  // individual views in the if statements below.
  DCHECK(view);
  if ((view == spoken_feedback_top_view_ || view == spoken_feedback_view_) &&
      !controller->IsEnterpriseIconVisibleForSpokenFeedback()) {
    bool new_state = !controller->spoken_feedback().enabled();
    RecordAction(new_state
                     ? UserMetricsAction("StatusArea_SpokenFeedbackEnabled")
                     : UserMetricsAction("StatusArea_SpokenFeedbackDisabled"));
    LogUserAccessibilityEvent(UserSettingsEvent::Event::SPOKEN_FEEDBACK,
                              new_state);
    controller->SetSpokenFeedbackEnabled(new_state, A11Y_NOTIFICATION_NONE);
  } else if ((view == select_to_speak_top_view_ ||
              view == select_to_speak_view_) &&
             !controller->IsEnterpriseIconVisibleForSelectToSpeak()) {
    bool new_state = !controller->select_to_speak().enabled();
    RecordAction(new_state
                     ? UserMetricsAction("StatusArea_SelectToSpeakEnabled")
                     : UserMetricsAction("StatusArea_SelectToSpeakDisabled"));
    LogUserAccessibilityEvent(UserSettingsEvent::Event::SELECT_TO_SPEAK,
                              new_state);
    controller->select_to_speak().SetEnabled(new_state);
  } else if ((view == dictation_top_view_ || view == dictation_view_) &&
             !controller->IsEnterpriseIconVisibleForDictation()) {
    bool new_state = !controller->dictation().enabled();
    RecordAction(new_state ? UserMetricsAction("StatusArea_DictationEnabled")
                           : UserMetricsAction("StatusArea_DictationDisabled"));
    LogUserAccessibilityEvent(UserSettingsEvent::Event::DICTATION, new_state);
    controller->dictation().SetEnabled(new_state);
  } else if ((view == high_contrast_top_view_ || view == high_contrast_view_) &&
             !controller->IsEnterpriseIconVisibleForHighContrast()) {
    bool new_state = !controller->high_contrast().enabled();
    RecordAction(new_state
                     ? UserMetricsAction("StatusArea_HighContrastEnabled")
                     : UserMetricsAction("StatusArea_HighContrastDisabled"));
    LogUserAccessibilityEvent(UserSettingsEvent::Event::HIGH_CONTRAST,
                              new_state);
    controller->high_contrast().SetEnabled(new_state);
  } else if ((view == screen_magnifier_top_view_ ||
              view == screen_magnifier_view_) &&
             !controller->IsEnterpriseIconVisibleForFullScreenMagnifier()) {
    bool new_state = !delegate->IsMagnifierEnabled();
    RecordAction(new_state ? UserMetricsAction("StatusArea_MagnifierEnabled")
                           : UserMetricsAction("StatusArea_MagnifierDisabled"));
    LogUserAccessibilityEvent(UserSettingsEvent::Event::MAGNIFIER, new_state);
    delegate->SetMagnifierEnabled(new_state);
  } else if ((view == docked_magnifier_top_view_ ||
              view == docked_magnifier_view_) &&
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
    LogUserAccessibilityEvent(UserSettingsEvent::Event::DOCKED_MAGNIFIER,
                              new_state);
    docked_magnifier_controller->SetEnabled(new_state);
  } else if ((view == large_cursor_top_view_ || view == large_cursor_view_) &&
             !controller->IsEnterpriseIconVisibleForLargeCursor()) {
    bool new_state = !controller->large_cursor().enabled();
    RecordAction(new_state
                     ? UserMetricsAction("StatusArea_LargeCursorEnabled")
                     : UserMetricsAction("StatusArea_LargeCursorDisabled"));
    LogUserAccessibilityEvent(UserSettingsEvent::Event::LARGE_CURSOR,
                              new_state);
    controller->large_cursor().SetEnabled(new_state);
  } else if ((view == autoclick_top_view_ || view == autoclick_view_) &&
             !controller->IsEnterpriseIconVisibleForAutoclick()) {
    bool new_state = !controller->autoclick().enabled();
    RecordAction(new_state ? UserMetricsAction("StatusArea_AutoClickEnabled")
                           : UserMetricsAction("StatusArea_AutoClickDisabled"));
    LogUserAccessibilityEvent(UserSettingsEvent::Event::AUTO_CLICK, new_state);
    controller->autoclick().SetEnabled(new_state);
  } else if ((view == virtual_keyboard_top_view_ ||
              view == virtual_keyboard_view_) &&
             !controller->IsEnterpriseIconVisibleForVirtualKeyboard()) {
    bool new_state = !controller->virtual_keyboard().enabled();
    RecordAction(new_state
                     ? UserMetricsAction("StatusArea_VirtualKeyboardEnabled")
                     : UserMetricsAction("StatusArea_VirtualKeyboardDisabled"));
    LogUserAccessibilityEvent(UserSettingsEvent::Event::VIRTUAL_KEYBOARD,
                              new_state);
    controller->virtual_keyboard().SetEnabled(new_state);
  } else if ((view == switch_access_top_view_ || view == switch_access_view_) &&
             !controller->IsEnterpriseIconVisibleForSwitchAccess()) {
    bool new_state = !controller->switch_access().enabled();
    RecordAction(new_state
                     ? UserMetricsAction("StatusArea_SwitchAccessEnabled")
                     : UserMetricsAction("StatusArea_SwitchAccessDisabled"));
    LogUserAccessibilityEvent(UserSettingsEvent::Event::SWITCH_ACCESS,
                              new_state);
    controller->switch_access().SetEnabled(new_state);
  } else if (view == live_caption_top_view_ || view == live_caption_view_) {
    bool new_state = !controller->live_caption().enabled();
    RecordAction(new_state
                     ? UserMetricsAction("StatusArea_LiveCaptionEnabled")
                     : UserMetricsAction("StatusArea_LiveCaptionDisabled"));
    LogUserAccessibilityEvent(UserSettingsEvent::Event::LIVE_CAPTION,
                              new_state);
    controller->live_caption().SetEnabled(new_state);
  } else if ((view == caret_highlight_top_view_ ||
              view == caret_highlight_view_) &&
             !controller->IsEnterpriseIconVisibleForCaretHighlight()) {
    bool new_state = !controller->caret_highlight().enabled();
    RecordAction(new_state
                     ? UserMetricsAction("StatusArea_CaretHighlightEnabled")
                     : UserMetricsAction("StatusArea_CaretHighlightDisabled"));
    LogUserAccessibilityEvent(UserSettingsEvent::Event::CARET_HIGHLIGHT,
                              new_state);
    controller->caret_highlight().SetEnabled(new_state);
  } else if ((view == mono_audio_top_view_ || view == mono_audio_view_) &&
             !controller->IsEnterpriseIconVisibleForMonoAudio()) {
    bool new_state = !controller->mono_audio().enabled();
    RecordAction(new_state ? UserMetricsAction("StatusArea_MonoAudioEnabled")
                           : UserMetricsAction("StatusArea_MonoAudioDisabled"));
    LogUserAccessibilityEvent(UserSettingsEvent::Event::MONO_AUDIO, new_state);
    controller->mono_audio().SetEnabled(new_state);
  } else if ((view == highlight_mouse_cursor_top_view_ ||
              view == highlight_mouse_cursor_view_) &&
             !controller->IsEnterpriseIconVisibleForCursorHighlight()) {
    bool new_state = !controller->cursor_highlight().enabled();
    RecordAction(
        new_state
            ? UserMetricsAction("StatusArea_HighlightMouseCursorEnabled")
            : UserMetricsAction("StatusArea_HighlightMouseCursorDisabled"));
    LogUserAccessibilityEvent(UserSettingsEvent::Event::HIGHLIGHT_MOUSE_CURSOR,
                              new_state);
    controller->cursor_highlight().SetEnabled(new_state);
  } else if ((view == highlight_keyboard_focus_top_view_ ||
              view == highlight_keyboard_focus_view_) &&
             !controller->IsEnterpriseIconVisibleForFocusHighlight()) {
    bool new_state = !controller->focus_highlight().enabled();
    RecordAction(
        new_state
            ? UserMetricsAction("StatusArea_HighlightKeyboardFocusEnabled")
            : UserMetricsAction("StatusArea_HighlightKeyboardFocusDisabled"));
    LogUserAccessibilityEvent(
        UserSettingsEvent::Event::HIGHLIGHT_KEYBOARD_FOCUS, new_state);
    controller->focus_highlight().SetEnabled(new_state);
  } else if ((view == sticky_keys_top_view_ || view == sticky_keys_view_) &&
             !controller->IsEnterpriseIconVisibleForStickyKeys()) {
    bool new_state = !controller->sticky_keys().enabled();
    RecordAction(new_state
                     ? UserMetricsAction("StatusArea_StickyKeysEnabled")
                     : UserMetricsAction("StatusArea_StickyKeysDisabled"));
    LogUserAccessibilityEvent(UserSettingsEvent::Event::STICKY_KEYS, new_state);
    controller->sticky_keys().SetEnabled(new_state);
  }
}

void AccessibilityDetailedView::CreateExtraTitleRowButtons() {
  DCHECK(!help_view_);
  DCHECK(!settings_view_);

  tri_view()->SetContainerVisible(TriView::Container::END, true);

  help_view_ = CreateHelpButton(base::BindRepeating(
      &AccessibilityDetailedView::ShowHelp, base::Unretained(this)));
  settings_view_ = CreateSettingsButton(
      base::BindRepeating(&AccessibilityDetailedView::ShowSettings,
                          base::Unretained(this)),
      IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SETTINGS);
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

// SodaInstaller::Observer:
void AccessibilityDetailedView::OnSodaInstalled(
    speech::LanguageCode language_code) {
  std::u16string message = l10n_util::GetStringUTF16(
      IDS_ASH_ACCESSIBILITY_SETTING_SUBTITLE_SODA_DOWNLOAD_COMPLETE);
  MaybeShowSodaMessage(SodaFeature::kDictation, language_code, message);
  MaybeShowSodaMessage(SodaFeature::kLiveCaption, language_code, message);
}

void AccessibilityDetailedView::OnSodaInstallError(
    speech::LanguageCode language_code,
    speech::SodaInstaller::ErrorCode error_code) {
  std::u16string error_message;
  switch (error_code) {
    case speech::SodaInstaller::ErrorCode::kUnspecifiedError: {
      error_message = l10n_util::GetStringUTF16(
          IDS_ASH_ACCESSIBILITY_SETTING_SUBTITLE_SODA_DOWNLOAD_ERROR);
      break;
    }
    case speech::SodaInstaller::ErrorCode::kNeedsReboot: {
      error_message = l10n_util::GetStringUTF16(
          IDS_ASH_ACCESSIBILITY_SETTING_SUBTITLE_SODA_DOWNLOAD_ERROR_REBOOT_REQUIRED);
      break;
    }
  }

  MaybeShowSodaMessage(SodaFeature::kDictation, language_code, error_message);
  MaybeShowSodaMessage(SodaFeature::kLiveCaption, language_code, error_message);
}

void AccessibilityDetailedView::OnSodaProgress(
    speech::LanguageCode language_code,
    int progress) {
  std::u16string message = l10n_util::GetStringFUTF16Int(
      IDS_ASH_ACCESSIBILITY_SETTING_SUBTITLE_SODA_DOWNLOAD_PROGRESS, progress);
  MaybeShowSodaMessage(SodaFeature::kDictation, language_code, message);
  MaybeShowSodaMessage(SodaFeature::kLiveCaption, language_code, message);
}

void AccessibilityDetailedView::MaybeShowSodaMessage(
    SodaFeature feature,
    speech::LanguageCode language_code,
    std::u16string message) {
  if (IsSodaFeatureEnabled(feature) && IsSodaFeatureInTray(feature) &&
      SodaFeatureHasUpdate(feature, language_code)) {
    SetSodaFeatureSubtext(feature, message);
  }
}

bool AccessibilityDetailedView::IsSodaFeatureInTray(SodaFeature feature) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  switch (feature) {
    case SodaFeature::kDictation:
      return dictation_view_ && controller->IsDictationSettingVisibleInTray();
    case SodaFeature::kLiveCaption:
      return live_caption_view_ &&
             controller->IsLiveCaptionSettingVisibleInTray();
  }
}

void AccessibilityDetailedView::SetSodaFeatureSubtext(SodaFeature feature,
                                                      std::u16string message) {
  switch (feature) {
    case SodaFeature::kDictation:
      DCHECK(dictation_view_);
      dictation_view_->SetSubText(message);
      break;
    case SodaFeature::kLiveCaption:
      DCHECK(live_caption_view_);
      live_caption_view_->SetSubText(message);
      break;
  }
}

}  // namespace ash
