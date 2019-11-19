// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_IMPL_H_
#define ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_IMPL_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/accessibility_controller.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/session/session_observer.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace ax {
namespace mojom {
enum class Gesture;
}  // namespace mojom
}  // namespace ax

namespace gfx {
class Point;
}  // namespace gfx

namespace ash {

class AccessibilityHighlightController;
class AccessibilityObserver;
class ScopedBacklightsForcedOff;
class SelectToSpeakEventHandler;
class SwitchAccessEventHandler;

enum AccessibilityNotificationVisibility {
  A11Y_NOTIFICATION_NONE,
  A11Y_NOTIFICATION_SHOW,
};

// The controller for accessibility features in ash. Features can be enabled
// in chrome's webui settings or the system tray menu (see TrayAccessibility).
// Uses preferences to communicate with chrome to support mash.
class ASH_EXPORT AccessibilityControllerImpl : public AccessibilityController,
                                               public SessionObserver,
                                               public TabletModeObserver {
 public:
  AccessibilityControllerImpl();
  ~AccessibilityControllerImpl() override;

  // See Shell::RegisterProfilePrefs().
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  void Shutdown();

  void AddObserver(AccessibilityObserver* observer);
  void RemoveObserver(AccessibilityObserver* observer);

  // The following functions read and write to their associated preference.
  // These values are then used to determine whether the accelerator
  // confirmation dialog for the respective preference has been accepted before.
  void SetHighContrastAcceleratorDialogAccepted();
  bool HasHighContrastAcceleratorDialogBeenAccepted() const;
  void SetScreenMagnifierAcceleratorDialogAccepted();
  bool HasScreenMagnifierAcceleratorDialogBeenAccepted() const;
  void SetDockedMagnifierAcceleratorDialogAccepted();
  bool HasDockedMagnifierAcceleratorDialogBeenAccepted() const;
  void SetDictationAcceleratorDialogAccepted();
  bool HasDictationAcceleratorDialogBeenAccepted() const;
  void SetDisplayRotationAcceleratorDialogBeenAccepted();
  bool HasDisplayRotationAcceleratorDialogBeenAccepted() const;

  void SetAutoclickEnabled(bool enabled);
  bool autoclick_enabled() const { return autoclick_enabled_; }
  bool IsAutoclickSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForAutoclick();

  void SetAutoclickEventType(AutoclickEventType event_type);
  AutoclickEventType GetAutoclickEventType();
  void SetAutoclickMenuPosition(AutoclickMenuPosition position);
  AutoclickMenuPosition GetAutoclickMenuPosition();
  void RequestAutoclickScrollableBoundsForPoint(gfx::Point& point_in_screen);

  // Update the autoclick menu bounds if necessary. This may need to happen when
  // the display work area changes, or if system ui regions change (like the
  // virtual keyboard position).
  void UpdateAutoclickMenuBoundsIfNeeded();

  void SetCaretHighlightEnabled(bool enabled);
  bool caret_highlight_enabled() const { return caret_highlight_enabled_; }
  bool IsCaretHighlightSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForCaretHighlight();

  void SetCursorHighlightEnabled(bool enabled);
  bool cursor_highlight_enabled() const { return cursor_highlight_enabled_; }
  bool IsCursorHighlightSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForCursorHighlight();

  void SetDictationEnabled(bool enabled);
  bool dictation_enabled() const { return dictation_enabled_; }
  bool IsDictationSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForDictation();

  void SetFocusHighlightEnabled(bool enabled);
  bool focus_highlight_enabled() const { return focus_highlight_enabled_; }
  bool IsFocusHighlightSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForFocusHighlight();

  void SetFullscreenMagnifierEnabled(bool enabled);
  bool IsFullscreenMagnifierEnabledForTesting();
  bool IsFullScreenMagnifierSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForFullScreenMagnifier();

  void SetDockedMagnifierEnabledForTesting(bool enabled);
  bool IsDockedMagnifierEnabledForTesting();
  bool IsDockedMagnifierSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForDockedMagnifier();

  void SetHighContrastEnabled(bool enabled);
  bool high_contrast_enabled() const { return high_contrast_enabled_; }
  bool IsHighContrastSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForHighContrast();

  void SetLargeCursorEnabled(bool enabled);
  bool large_cursor_enabled() const { return large_cursor_enabled_; }
  bool IsLargeCursorSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForLargeCursor();

  void SetMonoAudioEnabled(bool enabled);
  bool mono_audio_enabled() const { return mono_audio_enabled_; }
  bool IsMonoAudioSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForMonoAudio();

  void SetSpokenFeedbackEnabled(bool enabled,
                                AccessibilityNotificationVisibility notify);
  bool spoken_feedback_enabled() const { return spoken_feedback_enabled_; }
  bool IsSpokenFeedbackSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForSpokenFeedback();

  void SetSelectToSpeakEnabled(bool enabled);
  bool select_to_speak_enabled() const { return select_to_speak_enabled_; }
  bool IsSelectToSpeakSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForSelectToSpeak();

  void RequestSelectToSpeakStateChange();
  SelectToSpeakState GetSelectToSpeakState() const;

  void SetStickyKeysEnabled(bool enabled);
  bool sticky_keys_enabled() const { return sticky_keys_enabled_; }
  bool IsStickyKeysSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForStickyKeys();

  void SetSwitchAccessEnabled(bool enabled);
  bool switch_access_enabled() const { return switch_access_enabled_; }
  bool IsSwitchAccessSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForSwitchAccess();

  void SetVirtualKeyboardEnabled(bool enabled);
  bool virtual_keyboard_enabled() const { return virtual_keyboard_enabled_; }
  bool IsVirtualKeyboardSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForVirtualKeyboard();

  bool dictation_active() const { return dictation_active_; }

  // Triggers an accessibility alert to give the user feedback.
  void TriggerAccessibilityAlert(AccessibilityAlert alert);

  // Triggers an accessibility alert with the given |message|.
  void TriggerAccessibilityAlertWithMessage(const std::string& message);

  // Plays an earcon. Earcons are brief and distinctive sounds that indicate
  // that their mapped event has occurred. The |sound_key| enums can be found in
  // chromeos/audio/chromeos_sounds.h.
  void PlayEarcon(int sound_key);

  // Initiates play of shutdown sound. Returns the TimeDelta duration.
  base::TimeDelta PlayShutdownSound();

  // Forwards an accessibility gesture from the touch exploration controller to
  // ChromeVox.
  void HandleAccessibilityGesture(ax::mojom::Gesture gesture);

  // Toggle dictation.
  void ToggleDictation();

  // Cancels all current and queued speech immediately.
  void SilenceSpokenFeedback();

  // Called when we first detect two fingers are held down, which can be used to
  // toggle spoken feedback on some touch-only devices.
  void OnTwoFingerTouchStart();

  // Called when the user is no longer holding down two fingers (including
  // releasing one, holding down three, or moving them).
  void OnTwoFingerTouchStop();

  // Whether or not to enable toggling spoken feedback via holding down two
  // fingers on the screen.
  bool ShouldToggleSpokenFeedbackViaTouch() const;

  // Plays tick sound indicating spoken feedback will be toggled after
  // countdown.
  void PlaySpokenFeedbackToggleCountdown(int tick_count);

  // Returns true if that accessibility feature pref |path| is being controlled
  // by a policy and false otherwise.
  bool IsEnterpriseIconVisibleInTrayMenu(const std::string& path);

  // Returns true if at least one of the primary settings of the accessibility
  // features is going to be visible in the accessibility tray menu.
  bool IsPrimarySettingsViewVisibleInTray();

  // Returns true if at least one of the additional settings of the
  // accessibility features is going to be visible in the accessibility tray
  // menu.
  bool IsAdditionalSettingsViewVisibleInTray();

  // Returns true if there exist one of the additional accessibility features
  // and one of the primary accessibility features which are going to visible on
  // accessibility tray menu.
  bool IsAdditionalSettingsSeparatorVisibleInTray();

  // AccessibilityController:
  void SetClient(AccessibilityControllerClient* client) override;
  void SetDarkenScreen(bool darken) override;
  void BrailleDisplayStateChanged(bool connected) override;
  void SetFocusHighlightRect(const gfx::Rect& bounds_in_screen) override;
  void SetCaretBounds(const gfx::Rect& bounds_in_screen) override;
  void SetAccessibilityPanelAlwaysVisible(bool always_visible) override;
  void SetAccessibilityPanelBounds(const gfx::Rect& bounds,
                                   AccessibilityPanelState state) override;
  void SetSelectToSpeakState(SelectToSpeakState state) override;
  void SetSelectToSpeakEventHandlerDelegate(
      SelectToSpeakEventHandlerDelegate* delegate) override;
  void SetSwitchAccessEventHandlerDelegate(
      SwitchAccessEventHandlerDelegate* delegate) override;
  void SetDictationActive(bool is_active) override;
  void ToggleDictationFromSource(DictationToggleSource source) override;
  void OnAutoclickScrollableBoundsFound(gfx::Rect& bounds_in_screen) override;
  void ForwardKeyEventsToSwitchAccess(bool should_forward) override;
  base::string16 GetBatteryDescription() const override;
  void SetVirtualKeyboardVisible(bool is_visible) override;
  void NotifyAccessibilityStatusChanged() override;
  bool IsAccessibilityFeatureVisibleInTrayMenu(
      const std::string& path) override;
  void SetSwitchAccessIgnoreVirtualKeyEventForTesting(
      bool should_ignore) override;

  // SessionObserver:
  void OnSigninScreenPrefServiceInitialized(PrefService* prefs) override;
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

  // Test helpers:
  SwitchAccessEventHandler* GetSwitchAccessEventHandlerForTest();

 private:
  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // Observes either the signin screen prefs or active user prefs and loads
  // initial settings.
  void ObservePrefs(PrefService* prefs);

  void UpdateAutoclickFromPref();
  void UpdateAutoclickDelayFromPref();
  void UpdateAutoclickEventTypeFromPref();
  void UpdateAutoclickRevertToLeftClickFromPref();
  void UpdateAutoclickStabilizePositionFromPref();
  void UpdateAutoclickMovementThresholdFromPref();
  void UpdateAutoclickMenuPositionFromPref();
  void UpdateCaretHighlightFromPref();
  void UpdateCursorHighlightFromPref();
  void UpdateDictationFromPref();
  void UpdateFocusHighlightFromPref();
  void UpdateHighContrastFromPref();
  void UpdateLargeCursorFromPref();
  void UpdateMonoAudioFromPref();
  void UpdateSpokenFeedbackFromPref();
  void UpdateSelectToSpeakFromPref();
  void UpdateStickyKeysFromPref();
  void UpdateSwitchAccessFromPref();
  void UpdateSwitchAccessKeyCodesFromPref(SwitchAccessCommand command);
  void UpdateSwitchAccessAutoScanEnabledFromPref();
  void UpdateSwitchAccessAutoScanSpeedFromPref();
  void UpdateSwitchAccessAutoScanKeyboardSpeedFromPref();
  void UpdateVirtualKeyboardFromPref();
  void UpdateAccessibilityHighlightingFromPrefs();

  void MaybeCreateSelectToSpeakEventHandler();
  void MaybeCreateSwitchAccessEventHandler();

  // The pref service of the currently active user or the signin profile before
  // user logs in. Can be null in ash_unittests.
  PrefService* active_user_prefs_ = nullptr;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Client interface in chrome browser.
  AccessibilityControllerClient* client_ = nullptr;

  bool autoclick_enabled_ = false;
  base::TimeDelta autoclick_delay_;
  bool caret_highlight_enabled_ = false;
  bool cursor_highlight_enabled_ = false;
  bool dictation_enabled_ = false;
  bool focus_highlight_enabled_ = false;
  bool high_contrast_enabled_ = false;
  bool large_cursor_enabled_ = false;
  int large_cursor_size_in_dip_ = kDefaultLargeCursorSize;
  bool mono_audio_enabled_ = false;
  bool spoken_feedback_enabled_ = false;
  bool select_to_speak_enabled_ = false;
  bool sticky_keys_enabled_ = false;
  bool switch_access_enabled_ = false;
  bool virtual_keyboard_enabled_ = false;
  bool dictation_active_ = false;

  SelectToSpeakState select_to_speak_state_ =
      SelectToSpeakState::kSelectToSpeakStateInactive;
  std::unique_ptr<SelectToSpeakEventHandler> select_to_speak_event_handler_;
  SelectToSpeakEventHandlerDelegate* select_to_speak_event_handler_delegate_ =
      nullptr;

  // List of key codes that Switch Access should capture.
  std::vector<int> switch_access_keys_to_capture_;
  std::unique_ptr<SwitchAccessEventHandler> switch_access_event_handler_;
  SwitchAccessEventHandlerDelegate* switch_access_event_handler_delegate_ =
      nullptr;

  // Used to control the highlights of caret, cursor and focus.
  std::unique_ptr<AccessibilityHighlightController>
      accessibility_highlight_controller_;

  // Used to force the backlights off to darken the screen.
  std::unique_ptr<ScopedBacklightsForcedOff> scoped_backlights_forced_off_;

  base::ObserverList<AccessibilityObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityControllerImpl);
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_IMPL_H_
