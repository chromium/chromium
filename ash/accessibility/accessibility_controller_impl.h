// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_IMPL_H_
#define ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_IMPL_H_

#include <memory>

#include "ash/accessibility/a11y_feature_type.h"
#include "ash/ash_export.h"
#include "ash/constants/ash_constants.h"
#include "ash/public/cpp/accessibility_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace aura {
class Window;
}  // namespace aura

namespace ax {
namespace mojom {
enum class Gesture;
}  // namespace mojom
}  // namespace ax

namespace gfx {
class Point;
class PointF;
struct VectorIcon;
}  // namespace gfx

namespace ash {

class AccessibilityConfirmationDialog;
class AccessibilityEventRewriter;
class AccessibilityHighlightController;
class AccessibilityObserver;
class DictationBubbleController;
class DictationNudgeController;
class FloatingAccessibilityController;
class PointScanController;
class ScopedBacklightsForcedOff;
class SelectToSpeakEventHandler;
class SelectToSpeakMenuBubbleController;
class SwitchAccessMenuBubbleController;
enum class Sound;

enum AccessibilityNotificationVisibility {
  A11Y_NOTIFICATION_NONE,
  A11Y_NOTIFICATION_SHOW,
};

// Used to indicate which accessibility notification should be shown.
enum class A11yNotificationType {
  // No accessibility notification.
  kNone,
  // Shown when spoken feedback is set enabled with A11Y_NOTIFICATION_SHOW.
  kSpokenFeedbackEnabled,
  // Shown when braille display is connected while spoken feedback is enabled.
  kBrailleDisplayConnected,
  // Shown when all Dictation-related DLCs have downloaded successfully.
  kDictationAllDlcsDownloaded,
  // Shown when all Dictation-related DLCs failed to download.
  kDictationNoDlcsDownloaded,
  // Shown when the Pumpkin DLC (but no other DLCs) have downloaded.
  kDicationOnlyPumpkinDownloaded,
  // Shown when the SODA DLC (but no other DLCs) have downloaded.
  kDictationOnlySodaDownloaded,
  // Shown when braille display is connected while spoken feedback is not
  // enabled yet. Note: in this case braille display connected would enable
  // spoken feedback.
  kSpokenFeedbackBrailleEnabled,
  // Shown when Switch Access is enabled.
  kSwitchAccessEnabled,
};

// The controller for accessibility features in ash. Features can be enabled
// in chrome's webui settings or the system tray menu (see TrayAccessibility).
// Uses preferences to communicate with chrome to support mash.
class ASH_EXPORT AccessibilityControllerImpl : public AccessibilityController,
                                               public SessionObserver,
                                               public TabletModeObserver {
 public:
  // Common interface for all features.
  class Feature {
   public:
    Feature(A11yFeatureType type,
            const std::string& pref_name,
            const gfx::VectorIcon* icon,
            AccessibilityControllerImpl* controller);
    Feature(const Feature&) = delete;
    Feature& operator=(Feature const&) = delete;
    virtual ~Feature();

    A11yFeatureType type() const { return type_; }
    // Tries to set the feature to |enabled| by setting the user pref.
    // Setting feature to be enabled can fail in following conditions:
    // - there is a higher priority pref(managed), which overrides this value.
    // - there is an other feature, which conflicts with the current one.
    virtual void SetEnabled(bool enabled);
    bool enabled() const { return enabled_; }
    bool IsVisibleInTray() const;
    bool IsEnterpriseIconVisible() const;
    const std::string& pref_name() const { return pref_name_; }
    const gfx::VectorIcon& icon() const;

    void UpdateFromPref();
    void SetConflictingFeature(A11yFeatureType feature);

   protected:
    const A11yFeatureType type_;
    // Some features cannot be enabled while others are on. When a conflicting
    // feature is enabled, we cannot enable current feature.
    A11yFeatureType conflicting_feature_ =
        A11yFeatureType::kNoConflictingFeature;
    bool enabled_ = false;
    const std::string pref_name_;
    raw_ptr<const gfx::VectorIcon, ExperimentalAsh> icon_;
    const raw_ptr<AccessibilityControllerImpl, ExperimentalAsh> owner_;
  };

  // Helper struct to store information about a11y dialog -- pref name, resource
  // ids for title and body. Also stores the information whether this dialog is
  // mandatory for every SetEnabled call.
  struct Dialog {
    std::string pref_name;
    int title_resource_id;
    int body_resource_id;
    // Whether this dialog should be shown on every SetEnabled action.
    bool mandatory;
  };

  // Some features have confirmation dialog associated with them.
  // Dialog can be applied for all SetEnabled() actions, or only to ones
  // associated with accelerators.
  class FeatureWithDialog : public Feature {
   public:
    FeatureWithDialog(A11yFeatureType type,
                      const std::string& pref_name,
                      const gfx::VectorIcon* icon,
                      const Dialog& dialog,
                      AccessibilityControllerImpl* controller);
    ~FeatureWithDialog() override;

    // Tries to set the feature enabled, if its dialog is mandatory, shows the
    // dailog for the first time feature is enabled.
    void SetEnabled(bool enabled) override;
    // If the dialog have not been accepted, we show it. When it is accepted, we
    // call SetEnabled() and invoke |completion_callback|.
    void SetEnabledWithDialog(bool enabled,
                              base::OnceClosure completion_callback);
    void SetDialogAccepted();
    bool WasDialogAccepted() const;

   private:
    Dialog dialog_;
  };

  // Contains data used to give an accessibility-related notification.
  struct A11yNotificationWrapper {
    A11yNotificationWrapper();
    A11yNotificationWrapper(A11yNotificationType type_in,
                            std::vector<std::u16string> replacements_in);
    ~A11yNotificationWrapper();
    A11yNotificationWrapper(const A11yNotificationWrapper&);

    A11yNotificationType type = A11yNotificationType::kNone;
    std::vector<std::u16string> replacements;
  };

  AccessibilityControllerImpl();

  AccessibilityControllerImpl(const AccessibilityControllerImpl&) = delete;
  AccessibilityControllerImpl& operator=(const AccessibilityControllerImpl&) =
      delete;

  ~AccessibilityControllerImpl() override;

  // See Shell::RegisterProfilePrefs().
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  void Shutdown();

  void AddObserver(AccessibilityObserver* observer);
  void RemoveObserver(AccessibilityObserver* observer);

  Feature& GetFeature(A11yFeatureType feature) const;

  base::WeakPtr<AccessibilityControllerImpl> GetWeakPtr();

  // Getters for the corresponding features.
  Feature& autoclick() const;
  Feature& caret_highlight() const;
  Feature& cursor_highlight() const;
  Feature& dictation() const;
  Feature& floating_menu() const;
  Feature& focus_highlight() const;
  FeatureWithDialog& fullscreen_magnifier() const;
  FeatureWithDialog& docked_magnifier() const;
  FeatureWithDialog& high_contrast() const;
  Feature& large_cursor() const;
  Feature& live_caption() const;
  Feature& mono_audio() const;
  Feature& spoken_feedback() const;
  Feature& select_to_speak() const;
  Feature& sticky_keys() const;
  Feature& switch_access() const;
  Feature& virtual_keyboard() const;
  Feature& cursor_color() const;

  void SetDisplayRotationAcceleratorDialogBeenAccepted();
  bool HasDisplayRotationAcceleratorDialogBeenAccepted() const;

  bool IsAutoclickSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForAutoclick();

  void SetAutoclickEventType(AutoclickEventType event_type);
  AutoclickEventType GetAutoclickEventType();
  void SetAutoclickMenuPosition(FloatingMenuPosition position);
  FloatingMenuPosition GetAutoclickMenuPosition();
  void RequestAutoclickScrollableBoundsForPoint(gfx::Point& point_in_screen);
  void MagnifierBoundsChanged(const gfx::Rect& bounds_in_screen);

  void SetFloatingMenuPosition(FloatingMenuPosition position);
  FloatingMenuPosition GetFloatingMenuPosition();
  FloatingAccessibilityController* GetFloatingMenuController();

  PointScanController* GetPointScanController();

  // Update the autoclick menu bounds and sticky keys overlay bounds if
  // necessary. This may need to happen when the display work area changes, or
  // if system UI regions change (like the virtual keyboard position).
  void UpdateFloatingPanelBoundsIfNeeded();

  // Update the autoclick menu bounds if necessary. This may need to happen when
  // the display work area changes, or if system UI regions change (like the
  // virtual keyboard position).
  void UpdateAutoclickMenuBoundsIfNeeded();

  bool IsCaretHighlightSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForCaretHighlight();

  bool IsCursorHighlightSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForCursorHighlight();

  bool IsDictationSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForDictation();

  bool IsFocusHighlightSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForFocusHighlight();

  bool IsFullScreenMagnifierSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForFullScreenMagnifier();

  bool IsDockedMagnifierSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForDockedMagnifier();

  bool IsHighContrastSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForHighContrast();

  bool IsLargeCursorSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForLargeCursor();

  bool IsLiveCaptionSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForLiveCaption();

  bool IsMonoAudioSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForMonoAudio();

  void SetSpokenFeedbackEnabled(bool enabled,
                                AccessibilityNotificationVisibility notify);
  bool IsSpokenFeedbackSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForSpokenFeedback();

  bool IsSelectToSpeakSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForSelectToSpeak();

  void RequestSelectToSpeakStateChange();
  SelectToSpeakState GetSelectToSpeakState() const;

  bool IsStickyKeysSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForStickyKeys();

  // Switch access may be disabled in prefs but still running when the disable
  // dialog is displaying.
  bool IsSwitchAccessRunning() const;
  bool IsSwitchAccessSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForSwitchAccess();
  void SetAccessibilityEventRewriter(
      AccessibilityEventRewriter* accessibility_event_rewriter);
  bool IsPointScanEnabled();

  bool IsVirtualKeyboardSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForVirtualKeyboard();

  void SetTabletModeShelfNavigationButtonsEnabled(bool enabled);
  bool tablet_mode_shelf_navigation_buttons_enabled() const {
    return tablet_mode_shelf_navigation_buttons_enabled_;
  }

  void ShowFloatingMenuIfEnabled() override;

  bool dictation_active() const { return dictation_active_; }

  // Returns true if accessibility shortcuts have been disabled.
  bool accessibility_shortcuts_enabled() const { return shortcuts_enabled_; }

  // Triggers an accessibility alert to give the user feedback.
  void TriggerAccessibilityAlert(AccessibilityAlert alert);

  // Triggers an accessibility alert with the given |message|.
  void TriggerAccessibilityAlertWithMessage(const std::string& message);

  // Plays an earcon. Earcons are brief and distinctive sounds that indicate
  // that their mapped event has occurred. The |sound_key| enums can be found in
  // chromeos/ash/components/audio/sounds.h.
  void PlayEarcon(Sound sound_key);

  // Initiates play of shutdown sound. Returns the TimeDelta duration.
  base::TimeDelta PlayShutdownSound();

  // Forwards an accessibility gesture from the touch exploration controller to
  // ChromeVox.
  void HandleAccessibilityGesture(ax::mojom::Gesture gesture,
                                  gfx::PointF location);

  // Toggle dictation.
  void ToggleDictation();

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

  // Starts point scanning, to select a point onscreen without using a mouse
  // (as used by Switch Access).
  void StartPointScanning();

  // Sets a window to take a11y focus. This is for windows that need to work
  // with accessibility clients that consume accessibility APIs, but cannot take
  // real focus themselves. This is meant for temporary UIs, such as capture
  // mode and should be set back to null when exiting those UIs, so a11y can
  // focus windows with real focus. Destroying |a11y_override_window| will also
  // set the a11y override window back to null.
  void SetA11yOverrideWindow(aura::Window* a11y_override_window);

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
  void ShowSelectToSpeakPanel(const gfx::Rect& anchor,
                              bool is_paused,
                              double speech_rate) override;
  void HideSelectToSpeakPanel() override;
  void OnSelectToSpeakPanelAction(SelectToSpeakPanelAction action,
                                  double value) override;
  void HideSwitchAccessBackButton() override;
  void HideSwitchAccessMenu() override;
  void ShowSwitchAccessBackButton(const gfx::Rect& anchor) override;
  void ShowSwitchAccessMenu(const gfx::Rect& anchor,
                            std::vector<std::string> actions_to_show) override;
  void StartPointScan() override;
  void StopPointScan() override;
  void SetPointScanSpeedDipsPerSecond(
      int point_scan_speed_dips_per_second) override;
  void SetDictationActive(bool is_active) override;
  void ToggleDictationFromSource(DictationToggleSource source) override;
  void ShowDictationLanguageUpgradedNudge(
      const std::string& dictation_locale,
      const std::string& application_locale) override;
  void HandleAutoclickScrollableBoundsFound(
      gfx::Rect& bounds_in_screen) override;
  std::u16string GetBatteryDescription() const override;
  void SetVirtualKeyboardVisible(bool is_visible) override;
  void PerformAcceleratorAction(AcceleratorAction accelerator_action) override;
  void NotifyAccessibilityStatusChanged() override;
  bool IsAccessibilityFeatureVisibleInTrayMenu(
      const std::string& path) override;
  void DisablePolicyRecommendationRestorerForTesting() override;
  void SuspendSwitchAccessKeyHandling(bool suspend) override;
  void EnableChromeVoxVolumeSlideGesture() override;
  void UpdateDictationButtonOnSpeechRecognitionDownloadChanged(
      int download_progress) override;
  void ShowNotificationForDictation(
      DictationNotificationType type,
      const std::u16string& display_language) override;
  void UpdateDictationBubble(
      bool visible,
      DictationBubbleIconType icon,
      const absl::optional<std::u16string>& text,
      const absl::optional<std::vector<DictationBubbleHintType>>& hints)
      override;
  void SilenceSpokenFeedback() override;

  // A confirmation dialog will be shown the first time an accessibility feature
  // is enabled using the specified accelerator key sequence. Only one dialog
  // will be shown at a time, and will not be shown again if the user has
  // selected "accept" on a given dialog. The dialog was added to ensure that
  // users would be aware of the shortcut they have just enabled, and to prevent
  // users from accidentally triggering the feature. The dialog is currently
  // shown when enabling the following features: high contrast, full screen
  // magnifier, docked magnifier and screen rotation and when requested by the
  // AccessibilityPrivate extension API. The shown dialog is stored as a weak
  // pointer in the variable |confirmation_dialog_| below.
  void ShowConfirmationDialog(const std::u16string& title,
                              const std::u16string& description,
                              base::OnceClosure on_accept_callback,
                              base::OnceClosure on_cancel_callback,
                              base::OnceClosure on_close_callback) override;

  // SessionObserver:
  void OnSigninScreenPrefServiceInitialized(PrefService* prefs) override;
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // Test helpers:
  AccessibilityEventRewriter* GetAccessibilityEventRewriterForTest();
  SwitchAccessMenuBubbleController* GetSwitchAccessBubbleControllerForTest() {
    return switch_access_bubble_controller_.get();
  }
  void DisableSwitchAccessDisableConfirmationDialogTesting() override;
  SelectToSpeakMenuBubbleController*
  GetSelectToSpeakMenuBubbleControllerForTest() {
    return select_to_speak_bubble_controller_.get();
  }
  AccessibilityConfirmationDialog* GetConfirmationDialogForTest() {
    return confirmation_dialog_.get();
  }

  bool enable_chromevox_volume_slide_gesture() {
    return enable_chromevox_volume_slide_gesture_;
  }
  DictationNudgeController* GetDictationNudgeControllerForTest() {
    return dictation_nudge_controller_.get();
  }

  int dictation_soda_download_progress() {
    return dictation_soda_download_progress_;
  }

  DictationBubbleController* GetDictationBubbleControllerForTest();

 private:
  // Populate |features_| with the feature of the correct type.
  void CreateAccessibilityFeatures();

  // Propagates the state of |feature| according to |feature->enabled()|.
  void OnFeatureChanged(A11yFeatureType feature);

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // Observes either the signin screen prefs or active user prefs and loads
  // initial settings.
  void ObservePrefs(PrefService* prefs);

  // Updates the actual feature status based on the prefs value.
  void UpdateFeatureFromPref(A11yFeatureType feature);

  void UpdateAutoclickDelayFromPref();
  void UpdateAutoclickEventTypeFromPref();
  void UpdateAutoclickRevertToLeftClickFromPref();
  void UpdateAutoclickStabilizePositionFromPref();
  void UpdateAutoclickMovementThresholdFromPref();
  void UpdateAutoclickMenuPositionFromPref();
  void UpdateFloatingMenuPositionFromPref();
  void UpdateLargeCursorFromPref();
  void UpdateLiveCaptionFromPref();
  void UpdateCursorColorFromPrefs();
  void UpdateColorFilteringFromPrefs();
  void UpdateSwitchAccessKeyCodesFromPref(SwitchAccessCommand command);
  void UpdateSwitchAccessAutoScanEnabledFromPref();
  void UpdateSwitchAccessAutoScanSpeedFromPref();
  void UpdateSwitchAccessAutoScanKeyboardSpeedFromPref();
  void UpdateSwitchAccessPointScanSpeedFromPref();
  void UpdateAccessibilityHighlightingFromPrefs();
  void UpdateShortcutsEnabledFromPref();
  void UpdateTabletModeShelfNavigationButtonsFromPref();

  void SwitchAccessDisableDialogClosed(bool disable_dialog_accepted);
  void MaybeCreateSelectToSpeakEventHandler();
  void ActivateSwitchAccess();
  void DeactivateSwitchAccess();
  void SyncSwitchAccessPrefsToSignInProfile();
  void UpdateKeyCodesAfterSwitchAccessEnabled();

  // Dictation's SODA download progress. Values are between 0 and 100. Tracked
  // for testing purposes only.
  int dictation_soda_download_progress_ = 0;

  // Client interface in chrome browser.
  raw_ptr<AccessibilityControllerClient, ExperimentalAsh> client_ = nullptr;

  // Features are indexed by A11yFeatureType cast to int.
  std::unique_ptr<Feature> features_[kA11yFeatureTypeCount];

  base::TimeDelta autoclick_delay_;
  int large_cursor_size_in_dip_ = kDefaultLargeCursorSize;

  bool dictation_active_ = false;
  bool shortcuts_enabled_ = true;
  bool tablet_mode_shelf_navigation_buttons_enabled_ = false;

  SelectToSpeakState select_to_speak_state_ =
      SelectToSpeakState::kSelectToSpeakStateInactive;
  std::unique_ptr<SelectToSpeakEventHandler> select_to_speak_event_handler_;
  raw_ptr<SelectToSpeakEventHandlerDelegate, ExperimentalAsh>
      select_to_speak_event_handler_delegate_ = nullptr;
  std::unique_ptr<SelectToSpeakMenuBubbleController>
      select_to_speak_bubble_controller_;

  // List of key codes that Switch Access should capture.
  std::vector<int> switch_access_keys_to_capture_;
  std::unique_ptr<SwitchAccessMenuBubbleController>
      switch_access_bubble_controller_;
  raw_ptr<AccessibilityEventRewriter, ExperimentalAsh>
      accessibility_event_rewriter_ = nullptr;
  bool no_switch_access_disable_confirmation_dialog_for_testing_ = false;
  bool switch_access_disable_dialog_showing_ = false;
  bool skip_switch_access_notification_ = false;

  // Used to control the highlights of caret, cursor and focus.
  std::unique_ptr<AccessibilityHighlightController>
      accessibility_highlight_controller_;

  // Used to display accessibility floating menu.
  std::unique_ptr<FloatingAccessibilityController> floating_menu_controller_;
  // By default, floating accessibility menu is not shown unless
  // ShowFloatingMenuIfEnabled() is called. This is used in kiosk mode to
  // postpone the showing of the menu till the splash screen closes. This value
  // makes floating menu visible as soon as it is enabled.
  bool always_show_floating_menu_when_enabled_ = false;

  // Used to control point scanning, or selecting a point onscreen without using
  // a mouse (as done by Switch Access).
  std::unique_ptr<PointScanController> point_scan_controller_;

  // Used to force the backlights off to darken the screen.
  std::unique_ptr<ScopedBacklightsForcedOff> scoped_backlights_forced_off_;

  // Used to show the offline dictation language upgrade nudge. This is created
  // with ShowDictationLanguageUpgradedNudge() and reset at Shutdown() or when
  // the Dictation feature is disabled.
  std::unique_ptr<DictationNudgeController> dictation_nudge_controller_;

  // Used to control the Dictation bubble UI.
  std::unique_ptr<DictationBubbleController> dictation_bubble_controller_;

  // True if ChromeVox should enable its volume slide gesture.
  bool enable_chromevox_volume_slide_gesture_ = false;

  base::ObserverList<AccessibilityObserver> observers_;

  // The pref service of the currently active user or the signin profile before
  // user logs in. Can be null in ash_unittests.
  raw_ptr<PrefService, ExperimentalAsh> active_user_prefs_ = nullptr;

  // This has to be the first one to be destroyed so we don't get updates about
  // any prefs during destruction.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // The current AccessibilityConfirmationDialog, if one exists.
  base::WeakPtr<AccessibilityConfirmationDialog> confirmation_dialog_;

  base::WeakPtrFactory<AccessibilityControllerImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_IMPL_H_
