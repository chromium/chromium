// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_H_
#define ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>

#include "ash/accessibility/a11y_feature_type.h"
#include "ash/accessibility/accessibility_notification_controller.h"
#include "ash/ash_export.h"
#include "ash/constants/ash_constants.h"
#include "ash/public/cpp/accelerator_actions.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/display/display_observer.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect.h"

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

namespace display {
enum class TabletState;
}  // namespace display

namespace gfx {
class Point;
class PointF;
struct VectorIcon;
}  // namespace gfx

namespace ash {

class AccessibilityConfirmationDialog;
class AccessibilityControllerClient;
class AccessibilityEventRewriter;
class AccessibilityHighlightController;
class AccessibilityObserver;
enum class AccessibilityPanelState;
enum class DictationToggleSource;
class DictationBubbleController;
enum class DictationBubbleHintType;
enum class DictationBubbleIconType;
enum class DictationNotificationType;
class DisableTrackpadEventRewriter;
enum class DisableTrackpadMode;
class FaceGazeBubbleController;
class FilterKeysEventRewriter;
class FlashScreenController;
class FloatingAccessibilityController;
class PointScanController;
class ScopedBacklightsForcedOff;
class SelectToSpeakEventHandler;
class SelectToSpeakEventHandlerDelegate;
class SelectToSpeakMenuBubbleController;
enum class SelectToSpeakState;
enum class Sound;
class SwitchAccessMenuBubbleController;

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
  // Shown when the facegaze-assets DLC has successfully downloaded.
  kFaceGazeAssetsDownloaded,
  // Shown when the facegaze-assets DLC failed to download.
  kFaceGazeAssetsFailed,
  // Shown when braille display is connected while spoken feedback is not
  // enabled yet. Note: in this case braille display connected would enable
  // spoken feedback.
  kSpokenFeedbackBrailleEnabled,
  // Shown when Switch Access is enabled.
  kSwitchAccessEnabled,
  // Shown when the internal trackpad is disabled.
  kTrackpadDisabled,
};

// The controller for accessibility features in ash. Features can be enabled
// in chrome's webui settings or the system tray menu (see TrayAccessibility).
// Uses preferences to communicate with chrome to support mash.
class ASH_EXPORT AccessibilityController
    : public SessionObserver,
      public display::DisplayObserver,
      public InputDeviceSettingsController::Observer {
 public:
  // Common interface for all features.
  class Feature {
   public:
    Feature(A11yFeatureType type,
            const std::string& pref_name,
            const gfx::VectorIcon* icon,
            const int name_resource_id,
            const bool toggleable_in_quicksettings,
            AccessibilityController* controller);
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
    int name_resource_id() const { return name_resource_id_; }
    bool toggleable_in_quicksettings() const {
      return toggleable_in_quicksettings_;
    }
    A11yFeatureType conflicting_feature() const { return conflicting_feature_; }

    void UpdateFromPref();
    void SetConflictingFeature(A11yFeatureType feature);
    // Start observing changes to the conflicting feature's pref, in order to
    // update own enabled state.
    void ObserveConflictingFeature();

    // Logs the amount of time this feature has been on, if it was turned on
    // during the logged in state. Clears the `enabled_time_`.
    void LogDurationMetric();

   protected:
    const A11yFeatureType type_;
    // Some features cannot be enabled while others are on. When a conflicting
    // feature is enabled, we cannot enable current feature.
    A11yFeatureType conflicting_feature_ =
        A11yFeatureType::kNoConflictingFeature;
    // Used to watch for changes in conflicting feature to ensure this updates
    // enabled state appropriately.
    std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
    bool enabled_ = false;
    const std::string pref_name_;
    raw_ptr<const gfx::VectorIcon> icon_;

    // The resource id used to fetch the string with this feature's name. Used
    // in quicksettings.
    const int name_resource_id_;

    // Specifies if this feature can be toggled from the accessibility options
    // available in the quicksettings menu.
    const bool toggleable_in_quicksettings_;

    // The time at which this feature was last enabled. Used for metrics.
    base::Time enabled_time_;

    const raw_ptr<AccessibilityController> owner_;
  };

  // Some features have confirmation dialog associated with them.
  // Dialog can be applied for all SetEnabled() actions, or only to ones
  // associated with accelerators.
  class FeatureWithDialog : public Feature {
   public:
    FeatureWithDialog(A11yFeatureType type,
                      const std::string& pref_name,
                      const gfx::VectorIcon* icon,
                      const int name_resource_id,
                      const bool toggleable_in_quicksettings,
                      const std::string& dialog_pref_name,
                      AccessibilityController* controller);
    ~FeatureWithDialog() override;

    void SetDialogAccepted();
    bool WasDialogAccepted() const;

   private:
    const std::string dialog_pref_;
  };

  // Contains data used to give an accessibility-related notification.
  struct A11yNotificationWrapper {
    A11yNotificationWrapper();
    A11yNotificationWrapper(A11yNotificationType type_in,
                            std::vector<std::u16string> replacements_in);
    A11yNotificationWrapper(
        A11yNotificationType type_in,
        std::vector<std::u16string> replacements_in,
        std::optional<base::RepeatingCallback<void(std::optional<int>)>>
            callback_in);
    ~A11yNotificationWrapper();
    A11yNotificationWrapper(const A11yNotificationWrapper&);

    A11yNotificationType type = A11yNotificationType::kNone;
    std::vector<std::u16string> replacements;
    std::optional<base::RepeatingCallback<void(std::optional<int>)>> callback;
  };

  static AccessibilityController* Get();

  AccessibilityController();

  AccessibilityController(const AccessibilityController&) = delete;
  AccessibilityController& operator=(const AccessibilityController&) = delete;

  ~AccessibilityController() override;

  // See Shell::RegisterProfilePrefs().
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  void Shutdown();

  void AddObserver(AccessibilityObserver* observer);
  void RemoveObserver(AccessibilityObserver* observer);

  Feature& GetFeature(A11yFeatureType feature) const;

  // Returns all `Feature`s that are toggleable in quicksettings and currently
  // enabled.
  std::vector<Feature*> GetEnabledFeaturesInQuickSettings() const;

  base::WeakPtr<AccessibilityController> GetWeakPtr();

  // Getters for the corresponding features.
  Feature& autoclick() const;
  Feature& caret_highlight() const;
  Feature& color_correction() const;
  Feature& cursor_color() const;
  Feature& cursor_highlight() const;
  Feature& dictation() const;
  Feature& disable_trackpad() const;
  Feature& face_gaze() const;
  Feature& flash_notifications() const;
  Feature& floating_menu() const;
  Feature& focus_highlight() const;
  Feature& large_cursor() const;
  Feature& live_caption() const;
  Feature& mono_audio() const;
  Feature& mouse_keys() const;
  Feature& reduced_animations() const;
  Feature& spoken_feedback() const;
  Feature& select_to_speak() const;
  Feature& sticky_keys() const;
  Feature& switch_access() const;
  Feature& virtual_keyboard() const;
  FeatureWithDialog& docked_magnifier() const;
  FeatureWithDialog& fullscreen_magnifier() const;
  FeatureWithDialog& high_contrast() const;

  void SetDisplayRotationAcceleratorDialogBeenAccepted();
  bool HasDisplayRotationAcceleratorDialogBeenAccepted() const;

  bool IsAutoclickSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForAutoclick();

  void SetAutoclickEventType(AutoclickEventType event_type);
  AutoclickEventType GetAutoclickEventType();
  void SetAutoclickMenuPosition(FloatingMenuPosition position);
  FloatingMenuPosition GetAutoclickMenuPosition();
  void RequestAutoclickScrollableBoundsForPoint(
      const gfx::Point& point_in_screen);
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

  bool IsFaceGazeSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForFaceGaze();

  bool IsFocusHighlightSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForFocusHighlight();

  bool IsFullScreenMagnifierSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForFullScreenMagnifier();

  bool IsDockedMagnifierSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForDockedMagnifier();

  bool IsHighContrastSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForHighContrast();

  bool IsColorCorrectionSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForColorCorrection();

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

  bool IsReducedAnimationsSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForReducedAnimations();

  void OnTrackpadNotificationClicked(std::optional<int> button_index);

  // Switch access may be disabled in prefs but still running when the disable
  // dialog is displaying.
  bool IsSwitchAccessRunning() const;
  bool IsSwitchAccessSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForSwitchAccess();
  void SetAccessibilityEventRewriter(
      AccessibilityEventRewriter* accessibility_event_rewriter);
  void SetDisableTrackpadEventRewriter(DisableTrackpadEventRewriter* rewriter);
  void EnableInternalTrackpad();
  DisableTrackpadMode GetDisableTrackpadMode();
  void SetFilterKeysEventRewriter(FilterKeysEventRewriter* rewriter);
  bool IsPointScanEnabled();

  bool IsVirtualKeyboardSettingVisibleInTray();
  bool IsEnterpriseIconVisibleForVirtualKeyboard();

  void SetTabletModeShelfNavigationButtonsEnabled(bool enabled);
  bool tablet_mode_shelf_navigation_buttons_enabled() const {
    return tablet_mode_shelf_navigation_buttons_enabled_;
  }

  // Shows floating accessibility menu if it was enabled by policy.
  void ShowFloatingMenuIfEnabled();

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

  // Sets the client interface.
  void SetClient(AccessibilityControllerClient* client);

  // Starts or stops darkening the screen (e.g. to allow chrome a11y extensions
  // to darken the screen).
  void SetDarkenScreen(bool darken);

  // Called when braille display state is changed.
  void BrailleDisplayStateChanged(bool connected);

  // Sets the focus highlight rect using |bounds_in_screen|. Called when focus
  // changed in page and a11y focus highlight feature is enabled.
  void SetFocusHighlightRect(const gfx::Rect& bounds_in_screen);

  // Sets the text input caret bounds used to draw the caret highlight effect.
  // For effciency, only sent when the caret highlight feature is enabled.
  // Setting off-screen or empty bounds suppresses the highlight.
  void SetCaretBounds(const gfx::Rect& bounds_in_screen);

  // Sets whether the accessibility panel should always be visible, regardless
  // of whether the window is fullscreen.
  void SetAccessibilityPanelAlwaysVisible(bool always_visible);

  // Sets the bounds for the accessibility panel. Overrides current
  // configuration (i.e. fullscreen, full-width).
  void SetAccessibilityPanelBounds(const gfx::Rect& bounds,
                                   AccessibilityPanelState state);

  // Sets the current Select-to-Speak state. This should be used by the Select-
  // to-Speak extension to inform ash of its updated state.
  void SetSelectToSpeakState(SelectToSpeakState state);

  // Set the delegate used by the Select-to-Speak event handler.
  void SetSelectToSpeakEventHandlerDelegate(
      SelectToSpeakEventHandlerDelegate* delegate);

  // Displays the Select-to-Speak panel.
  void ShowSelectToSpeakPanel(const gfx::Rect& anchor,
                              bool is_paused,
                              double speech_rate);

  // Hides the Select-to-Speak panel.
  void HideSelectToSpeakPanel();

  // Dispatches event to notify Select-to-speak that a panel action occurred,
  // with an optional value.
  void OnSelectToSpeakPanelAction(SelectToSpeakPanelAction action,
                                  double value);

  // Hides the Switch Access back button.
  void HideSwitchAccessBackButton();

  // Hides the Switch Access menu.
  void HideSwitchAccessMenu();

  // Show the Switch Access back button next to the specified rectangle.
  void ShowSwitchAccessBackButton(const gfx::Rect& anchor);

  // Show the Switch Access menu with the specified actions.
  void ShowSwitchAccessMenu(const gfx::Rect& anchor,
                            std::vector<std::string> actions_to_show);

  // Starts point scanning in Switch Access.
  void StartPointScan();

  // Stops point scanning in Switch Access.
  void StopPointScan();

  // Sets point scanning speed in Switch Access.
  void SetPointScanSpeedDipsPerSecond(int point_scan_speed_dips_per_second);

  // Set whether dictation is active.
  void SetDictationActive(bool is_active);

  // Starts or stops dictation. Records metrics for toggling via SwitchAccess.
  void ToggleDictationFromSource(DictationToggleSource source);

  // Enables Select to Speak if the feature is currently disabled. If Select to
  // Speak has not been enabled on the current profile before, then this method
  // shows a dialog giving background about the feature. Once the dialog is
  // accepted, it is never shown again for that profile. This method does
  // nothing if the Select to Speak is currently enabled.
  void EnableSelectToSpeakWithDialog();

  // Disables the internal trackpad with a dialog.
  void DisableTrackpadWithDialog();

  // Enables Dictation if the feature is currently disabled. Toggles (starts or
  // stops) Dictation if the feature is currently enabled.
  void EnableOrToggleDictationFromSource(DictationToggleSource source);

  // Shows a nudge explaining that a user's dictation language was upgraded to
  // work offline.
  void ShowDictationLanguageUpgradedNudge(
      const std::string& dictation_locale,
      const std::string& application_locale);

  // Called when the Automatic Clicks extension finds scrollable bounds.
  void HandleAutoclickScrollableBoundsFound(const gfx::Rect& bounds_in_screen);

  // Retrieves a string description of the current battery status.
  std::u16string GetBatteryDescription() const;

  // Shows or hides the virtual keyboard.
  void SetVirtualKeyboardVisible(bool is_visible);

  // Toggle Mouse Keys.
  void ToggleMouseKeys();

  // Perform the action assigned to the accessibility key.
  void PerformAccessibilityAction();

  // Performs the given accelerator action.
  void PerformAcceleratorAction(AcceleratorAction accelerator_action);

  // Notify observers that the accessibility status has changed. This is part of
  // the public interface because a11y features like screen magnifier are
  // managed outside of this accessibility controller.
  void NotifyAccessibilityStatusChanged();

  // Returns true if the |path| pref is being controlled by a policy which
  // enforces turning it on or its not being controlled by any type of policy
  // and false otherwise.
  bool IsAccessibilityFeatureVisibleInTrayMenu(const std::string& path);

  // Disables restoring of recommended policy values.
  void DisablePolicyRecommendationRestorerForTesting();

  // Suspends (or resumes) key handling for Switch Access.
  void SuspendSwitchAccessKeyHandling(bool suspend);

  // Enables ChromeVox's volume slide gesture.
  void EnableChromeVoxVolumeSlideGesture();

  // Updates the enabled state, tooltip, and progress ring of the dictation
  // button in the status tray when speech recognition file download state
  // changes. `download_progress` indicates SODA download progress and is
  // guaranteed to be between 0 and 100 (inclusive).
  void UpdateDictationButtonOnSpeechRecognitionDownloadChanged(
      int download_progress);

  // Shows a notification card in the message center informing the user that
  // speech recognition files have either downloaded successfully or failed.
  // Specific to the Dictation feature.
  void ShowNotificationForDictation(DictationNotificationType type,
                                    const std::u16string& display_language);
  // Updates the Dictation UI bubble. `text` is optional to allow clients to
  // clear the bubble's text.
  void UpdateDictationBubble(
      bool visible,
      DictationBubbleIconType icon,
      const std::optional<std::u16string>& text,
      const std::optional<std::vector<DictationBubbleHintType>>& hints);

  // Updates the FaceGaze UI bubble.
  void UpdateFaceGazeBubble(const std::u16string& text);

  // Shows a notification notifying the user about the FaceGaze DLC download.
  void ShowNotificationForFaceGaze(FaceGazeNotificationType type);

  // Cancels all of spoken feedback's current and queued speech immediately.
  void SilenceSpokenFeedback();

  // Determines the action key that corresponds to F7 for the caret browsing
  // dialog.
  std::optional<ui::KeyboardCode> GetCaretBrowsingActionKey();

  // Shows an accessibility-related toast.
  void ShowToast(AccessibilityToastType type);

  // Shows a confirmation dialog with the given text, description,
  // and cancel button name, and calls the relevant callback when the
  // dialog is confirmed, canceled or closed.
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
  // This is also used to show the dialog for Select to Speak's enhanced network
  // voices.
  void ShowConfirmationDialog(const std::u16string& title,
                              const std::u16string& description,
                              const std::u16string& confirm_name,
                              const std::u16string& cancel_name,
                              base::OnceClosure on_accept_callback,
                              base::OnceClosure on_cancel_callback,
                              base::OnceClosure on_close_callback);
  gfx::Rect GetConfirmationDialogBoundsInScreen();

  void PreviewFlashNotification() const;

  // SessionObserver:
  void OnSigninScreenPrefServiceInitialized(PrefService* prefs) override;
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // InputDeviceSettingsController::Observer:
  void OnMouseConnected(const mojom::Mouse& mouse) override;
  void OnTouchpadConnected(const mojom::Touchpad& touchpad) override;

  // Test helpers:
  AccessibilityEventRewriter* GetAccessibilityEventRewriterForTest();
  DisableTrackpadEventRewriter* GetDisableTrackpadEventRewriterForTest();
  FilterKeysEventRewriter* GetFilterKeysEventRewriterForTest();
  SwitchAccessMenuBubbleController* GetSwitchAccessBubbleControllerForTest() {
    return switch_access_bubble_controller_.get();
  }

  // Disables the dialog shown when Auto Click is turned on.
  // Used in tests.
  void DisableAutoClickConfirmationDialogForTest();
  // Disables the dialog shown when Switch Access is turned off.
  // Used in tests.
  void DisableSwitchAccessDisableConfirmationDialogTesting();
  // Disables the dialog shown when Switch Access is turned off.
  // Used in tests.
  void DisableSwitchAccessEnableNotificationTesting();
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

  int dictation_soda_download_progress() {
    return dictation_soda_download_progress_;
  }

  DictationBubbleController* GetDictationBubbleControllerForTest();

  FaceGazeBubbleController* GetFaceGazeBubbleControllerForTest();

  bool IsDictationKeyboardDialogShowingForTesting() {
    return dictation_keyboard_dialog_showing_for_testing_;
  }
  void AcceptDictationKeyboardDialogForTesting() {
    OnDictationKeyboardDialogAccepted();
  }
  void DismissDictationKeyboardDialogForTesting() {
    OnDictationKeyboardDialogDismissed();
  }
  void AcceptDisableTrackpadDialogForTesting() {
    OnDisableTrackpadDialogAccepted();
  }
  void DismissDisableTrackpadDialogForTesting() {
    OnDisableTrackpadDialogDismissed();
  }

  void AddShowToastCallbackForTesting(
      base::RepeatingCallback<void(AccessibilityToastType)> callback);

  void AddShowConfirmationDialogCallbackForTesting(
      base::RepeatingCallback<void()> callback);

  bool VerifyFeaturesDataForTesting();

  SelectToSpeakEventHandler* GetSelectToSpeakEventHandlerForTesting() const {
    return select_to_speak_event_handler_.get();
  }

  FlashScreenController* GetFlashScreenControllerForTesting() const {
    return flash_screen_controller_.get();
  }

  void SetVirtualKeyboardVisibleCallbackForTesting(
      base::RepeatingCallback<void()> callback);

  // Scrolls at the target location in the specified direction.
  void ScrollAtPoint(const gfx::Point& target,
                     AccessibilityScrollDirection direction);

  void ObserveInputDeviceSettings();
  void StopObservingInputDeviceSettings() {
    input_device_settings_observer_.Reset();
  }

 private:
  // Populate |features_| with the feature of the correct type.
  void CreateAccessibilityFeatures();

  // Propagates the state of |feature| according to |feature->enabled()|.
  void OnFeatureChanged(A11yFeatureType feature);

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

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
  void UpdateMouseKeysDisableInTextFieldsFromPref();
  void UpdateMouseKeysAccelerationFromPref();
  void UpdateMouseKeysMaxSpeedFromPref();
  void UpdateMouseKeysUsePrimaryKeysFromPref();
  void UpdateMouseKeysDominantHandFromPref();
  void UpdateFloatingMenuPositionFromPref();
  void UpdateLargeCursorFromPref();
  void UpdateLiveCaptionFromPref();
  void UpdateCursorColorFromPrefs(bool notify);
  void UpdateFaceGazeFromPrefs();
  void UpdateFlashNotificationsFromPrefs();
  void UpdateDisableTrackpadFromPrefs();
  void UpdateColorCorrectionFromPrefs();
  void UpdateCaretBlinkIntervalFromPrefs() const;
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

  void ShowDictationKeyboardDialog();
  void OnDictationKeyboardDialogAccepted();
  void OnDictationKeyboardDialogDismissed();

  void ShowSelectToSpeakKeyboardDialog();
  void OnSelectToSpeakKeyboardDialogAccepted();
  void OnSelectToSpeakKeyboardDialogDismissed();

  void ShowDisableTrackpadDialog();
  void OnDisableTrackpadDialogAccepted();
  void OnDisableTrackpadDialogDismissed();
  void ExternalDeviceConnected();

  void RecordSelectToSpeakSpeechDuration(SelectToSpeakState old_state,
                                         SelectToSpeakState new_state);

  // Dictation's SODA download progress. Values are between 0 and 100. Tracked
  // for testing purposes only.
  int dictation_soda_download_progress_ = 0;

  bool dictation_keyboard_dialog_showing_for_testing_ = false;

  // Client interface in chrome browser.
  raw_ptr<AccessibilityControllerClient> client_ = nullptr;

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
  raw_ptr<SelectToSpeakEventHandlerDelegate>
      select_to_speak_event_handler_delegate_ = nullptr;
  std::unique_ptr<SelectToSpeakMenuBubbleController>
      select_to_speak_bubble_controller_;

  // List of key codes that Switch Access should capture.
  std::vector<int> switch_access_keys_to_capture_;
  std::unique_ptr<SwitchAccessMenuBubbleController>
      switch_access_bubble_controller_;
  raw_ptr<AccessibilityEventRewriter> accessibility_event_rewriter_ = nullptr;
  raw_ptr<DisableTrackpadEventRewriter> disable_trackpad_event_rewriter_ =
      nullptr;
  raw_ptr<FilterKeysEventRewriter> filter_keys_event_rewriter_ = nullptr;
  // Used in tests to disable the dialog shown when Auto Click is turned on.
  bool no_auto_click_confirmation_dialog_for_testing_ = false;
  bool no_switch_access_disable_confirmation_dialog_for_testing_ = false;
  bool switch_access_disable_dialog_showing_ = false;
  bool skip_switch_access_notification_ = false;

  base::RepeatingCallback<void()> set_virtual_keyboard_visible_callback_;

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

  // Used to control the Dictation bubble UI.
  std::unique_ptr<DictationBubbleController> dictation_bubble_controller_;

  // Used to control the FaceGaze bubble UI.
  std::unique_ptr<FaceGazeBubbleController> facegaze_bubble_controller_;

  // Used to control accessibility-related notifications.
  std::unique_ptr<AccessibilityNotificationController>
      accessibility_notification_controller_;

  std::unique_ptr<FlashScreenController> flash_screen_controller_;

  // True if ChromeVox should enable its volume slide gesture.
  bool enable_chromevox_volume_slide_gesture_ = false;

  base::ObserverList<AccessibilityObserver> observers_;

  // The pref service of the currently active user or the signin profile before
  // user logs in. Can be null in ash_unittests.
  raw_ptr<PrefService> active_user_prefs_ = nullptr;

  // This has to be the first one to be destroyed so we don't get updates about
  // any prefs during destruction.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // The current AccessibilityConfirmationDialog, if one exists.
  base::WeakPtr<AccessibilityConfirmationDialog> confirmation_dialog_;

  base::RepeatingCallback<void()>
      show_confirmation_dialog_callback_for_testing_;

  base::Time select_to_speak_speech_start_time_;

  base::ScopedObservation<InputDeviceSettingsController,
                          InputDeviceSettingsController::Observer>
      input_device_settings_observer_{this};

  base::WeakPtrFactory<AccessibilityController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_H_
