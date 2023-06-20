// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_MANAGER_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ash/accessibility/chromevox_panel.h"
#include "chrome/browser/ash/accessibility/service/accessibility_service_client.h"
#include "chrome/browser/extensions/api/braille_display_private/braille_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/soda/soda_installer.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_system.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace content {
struct FocusedNodeDetails;
}  // namespace content

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

class AccessibilityExtensionLoader;
class Dictation;
class PumpkinInstaller;
class SelectToSpeakEventHandlerDelegateImpl;
enum class SelectToSpeakState;
enum class SelectToSpeakPanelAction;
enum class Sound;
struct AccessibilityFocusRingInfo;
class AccessibilityServiceClient;

enum class AccessibilityNotificationType {
  kManagerShutdown,
  kToggleHighContrastMode,
  kToggleLargeCursor,
  kToggleLiveCaption,
  kToggleStickyKeys,
  kToggleScreenMagnifier,
  kToggleSpokenFeedback,
  kToggleSelectToSpeak,
  kToggleSwitchAccess,
  kToggleVirtualKeyboard,
  kToggleMonoAudio,
  kToggleCaretHighlight,
  kToggleCursorHighlight,
  kToggleFocusHighlight,
  kToggleDictation,
  kToggleDockedMagnifier,
};

struct AccessibilityStatusEventDetails {
  AccessibilityStatusEventDetails(
      AccessibilityNotificationType notification_type,
      bool enabled);

  AccessibilityNotificationType notification_type;
  bool enabled;
};

using AccessibilityStatusCallbackList =
    base::RepeatingCallbackList<void(const AccessibilityStatusEventDetails&)>;
using AccessibilityStatusCallback =
    AccessibilityStatusCallbackList::CallbackType;
using GetDlcContentsCallback =
    base::OnceCallback<void(const std::vector<uint8_t>&,
                            absl::optional<std::string>)>;
using InstallPumpkinCallback = base::OnceCallback<void(
    absl::optional<::extensions::api::accessibility_private::PumpkinData>)>;

class AccessibilityPanelWidgetObserver;

enum class PlaySoundOption {
  // The sound is always played.
  kAlways = 0,

  // The sound is played only if spoken feedback is enabled, and
  // --ash-disable-system-sounds is not set.
  kOnlyIfSpokenFeedbackEnabled,
};

// AccessibilityManager changes the statuses of accessibility features
// watching profile notifications and pref-changes.
// TODO(yoshiki): merge MagnificationManager with AccessibilityManager.
class AccessibilityManager
    : public session_manager::SessionManagerObserver,
      public extensions::api::braille_display_private::BrailleObserver,
      public extensions::ExtensionRegistryObserver,
      public user_manager::UserManager::UserSessionStateObserver,
      public input_method::InputMethodManager::Observer,
      public CrasAudioHandler::AudioObserver,
      public ProfileObserver,
      public speech::SodaInstaller::Observer {
 public:
  AccessibilityManager(const AccessibilityManager&) = delete;
  AccessibilityManager& operator=(const AccessibilityManager&) = delete;

  // Creates an instance of AccessibilityManager, this should be called once,
  // because only one instance should exist at the same time.
  static void Initialize();
  // Deletes the existing instance of AccessibilityManager.
  static void Shutdown();
  // Returns the existing instance. If there is no instance, returns NULL.
  static AccessibilityManager* Get();

  // Show the accessibility help as a tab in the browser.
  static void ShowAccessibilityHelp();

  // Returns true when the accessibility menu should be shown.
  bool ShouldShowAccessibilityMenu();

  void ShowChromeVoxTutorial();

  // Enables or disables the large cursor.
  void EnableLargeCursor(bool enabled);

  // Returns true if the large cursor is enabled, or false if not.
  bool IsLargeCursorEnabled() const;

  // Enables or disables Live Caption.
  void EnableLiveCaption(bool enabled);

  // Returns true if Live Caption is enabled, or false if not.
  bool IsLiveCaptionEnabled() const;

  // Enables or disable Sticky Keys.
  void EnableStickyKeys(bool enabled);

  // Returns true if the Sticky Keys is enabled, or false if not.
  bool IsStickyKeysEnabled() const;

  // Enables or disables spoken feedback. Enabling spoken feedback installs the
  // ChromeVox component extension.
  void EnableSpokenFeedback(bool enabled);

  // Enables spoken feedback. Automatically opens the tutorial once ChromeVox
  // loads.
  void EnableSpokenFeedbackWithTutorial();

  // Returns true if spoken feedback is enabled, or false if not.
  bool IsSpokenFeedbackEnabled() const;

  // Enables or disables the high contrast mode for Chrome.
  void EnableHighContrast(bool enabled);

  // Returns true if High Contrast is enabled, or false if not.
  bool IsHighContrastEnabled() const;

  // Enables or disables autoclick.
  void EnableAutoclick(bool enabled);

  // Returns true if autoclick is enabled.
  bool IsAutoclickEnabled() const;

  // Requests the Autoclick extension find the bounds of the nearest scrollable
  // ancestor to the point in the screen, as given in screen coordinates.
  void RequestAutoclickScrollableBoundsForPoint(gfx::Point& point_in_screen);

  // Dispatches magnifier bounds update to Magnifier (through Accessibility
  // Common extension).
  void MagnifierBoundsChanged(const gfx::Rect& bounds_in_screen);

  // Enables or disables the virtual keyboard.
  void EnableVirtualKeyboard(bool enabled);
  // Returns true if the virtual keyboard is enabled, otherwise false.
  bool IsVirtualKeyboardEnabled() const;

  // Enables or disables mono audio output.
  void EnableMonoAudio(bool enabled);
  // Returns true if mono audio output is enabled, otherwise false.
  bool IsMonoAudioEnabled() const;

  // Starts or stops darkening the screen.
  void SetDarkenScreen(bool darken);

  // Invoked to enable or disable caret highlighting.
  void SetCaretHighlightEnabled(bool enabled);

  // Returns if caret highlighting is enabled.
  bool IsCaretHighlightEnabled() const;

  // Invoked to enable or disable cursor highlighting.
  void SetCursorHighlightEnabled(bool enabled);

  // Returns if cursor highlighting is enabled.
  bool IsCursorHighlightEnabled() const;

  // Enables or disables dictation.
  void SetDictationEnabled(bool enabled) const;

  // Returns if dictation is enabled.
  bool IsDictationEnabled() const;

  // Invoked to enable or disable focus highlighting.
  void SetFocusHighlightEnabled(bool enabled);

  // Returns if focus highlighting is enabled.
  bool IsFocusHighlightEnabled() const;

  // Enables or disables tap dragging.
  void EnableTapDragging(bool enabled);

  // Returns true if the tap dragging is enabled, or false if not.
  bool IsTapDraggingEnabled() const;

  // Invoked to enable or disable select-to-speak.
  void SetSelectToSpeakEnabled(bool enabled);

  // Returns if select-to-speak is enabled.
  bool IsSelectToSpeakEnabled() const;

  // Requests that the Select-to-Speak extension change its state.
  void RequestSelectToSpeakStateChange();

  // Called when the Select-to-Speak extension state has changed.
  void SetSelectToSpeakState(SelectToSpeakState state);

  // Invoked to enable or disable Switch Access.
  void SetSwitchAccessEnabled(bool enabled);

  // Returns if Switch Access is enabled.
  bool IsSwitchAccessEnabled() const;

  // Invoked to enable or disable Color Correction.
  void SetColorCorrectionEnabled(bool enabled);

  // Returns if Color Correction is enabled.
  bool IsColorCorrectionEnabled() const;

  // Returns true if a braille display is connected to the system, otherwise
  // false.
  bool IsBrailleDisplayConnected() const;

  // user_manager::UserManager::UserSessionStateObserver overrides:
  void ActiveUserChanged(user_manager::User* active_user) override;

  // Initiates play of shutdown sound and returns it's duration.
  base::TimeDelta PlayShutdownSound();

  // Register a callback to be notified when the status of an accessibility
  // option changes.
  [[nodiscard]] base::CallbackListSubscription RegisterCallback(
      const AccessibilityStatusCallback& cb);

  // Notify registered callbacks of a status change in an accessibility setting.
  void NotifyAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details);

  // Notify accessibility when locale changes occur.
  void OnLocaleChanged();

  // Called when we first detect two fingers are held down, which can be
  // used to toggle spoken feedback on some touch-only devices.
  void OnTwoFingerTouchStart();

  // Called when the user is no longer holding down two fingers (including
  // releasing one, holding down three, or moving them).
  void OnTwoFingerTouchStop();

  // Whether or not to enable toggling spoken feedback via holding down
  // two fingers on the screen.
  bool ShouldToggleSpokenFeedbackViaTouch();

  // Play tick sound indicating spoken feedback will be toggled after countdown.
  bool PlaySpokenFeedbackToggleCountdown(int tick_count);

  // Update when a view is focused in ARC++.
  void OnViewFocusedInArc(const gfx::Rect& bounds_in_screen);

  // Plays an earcon. Earcons are brief and distinctive sounds that indicate
  // the their mapped event has occurred. The |sound_key| enums can be found in
  // chromeos/ash/components/audio/sounds.h.
  bool PlayEarcon(Sound sound_key, PlaySoundOption option);

  // Forward an accessibility gesture from the touch exploration controller
  // to ChromeVox.
  void HandleAccessibilityGesture(ax::mojom::Gesture gesture,
                                  gfx::PointF location);

  // Update the touch exploration controller so that synthesized
  // touch events are anchored at this point.
  void SetTouchAccessibilityAnchorPoint(const gfx::Point& anchor_point);

  // Called by our widget observer when the respective panel is closing.
  void OnChromeVoxPanelDestroying();

  // Profile having the a11y context.
  Profile* profile() { return profile_; }

  // Extension id of extension receiving keyboard events.
  void SetKeyboardListenerExtensionId(const std::string& id,
                                      content::BrowserContext* context);
  const std::string& keyboard_listener_extension_id() {
    return keyboard_listener_extension_id_;
  }

  // Unloads Switch Access.
  void OnSwitchAccessDisabled();

  // Starts or stops dictation (type what you speak).
  bool ToggleDictation();

  // Sets the focus ring with the given ID based on |focus_ring|.
  void SetFocusRing(std::string focus_ring_id,
                    std::unique_ptr<AccessibilityFocusRingInfo> focus_ring);

  // Hides focus ring on screen.
  void HideFocusRing(std::string caller_id);

  // Initializes the focus rings when an extension loads.
  void InitializeFocusRings(const std::string& extension_id);

  // Hides all focus rings for the extension, and removes that extension from
  // |focus_ring_names_for_extension_id_|.
  void RemoveFocusRings(const std::string& extension_id);

  // Draws a highlight at the given rects in screen coordinates. Rects may be
  // overlapping and will be merged into one layer. This looks similar to
  // selecting a region with the cursor, except it is drawn in the foreground
  // rather than behind a text layer.
  void SetHighlights(const std::vector<gfx::Rect>& rects_in_screen,
                     SkColor color);

  // Hides highlight on screen.
  void HideHighlights();

  // Sets the bounds used to highlight the text input caret.
  void SetCaretBounds(const gfx::Rect& bounds_in_screen);

  // Gets the startup sound user preference.
  bool GetStartupSoundEnabled() const;

  // Sets the startup sound user preference.
  void SetStartupSoundEnabled(bool value) const;

  // Gets the bluetooth braille display device address for the current user.
  const std::string GetBluetoothBrailleDisplayAddress() const;

  // Sets the bluetooth braille display device address for the current user.
  void UpdateBluetoothBrailleDisplayAddress(const std::string& address);

  // Create a focus ring ID from the extension ID and the name of the ring.
  const std::string GetFocusRingId(const std::string& extension_id,
                                   const std::string& focus_ring_name);

  // Sends a panel action event to the Select-to-speak extension.
  void OnSelectToSpeakPanelAction(SelectToSpeakPanelAction action,
                                  double value);

  // Sends the keys currently pressed to the Select-to-speak extension.
  void SendKeysCurrentlyDownToSelectToSpeak(
      const std::set<ui::KeyboardCode>& pressed_keys);

  // Sends a mouse event to the Select-to-speak extension.
  void SendMouseEventToSelectToSpeak(ui::EventType type,
                                     const gfx::PointF& position);

  // Called when Shimless RMA launches to enable accessibility features.
  void OnShimlessRmaLaunched();

  // SodaInstaller::Observer:
  void OnSodaInstalled(speech::LanguageCode language_code) override;
  void OnSodaInstallError(speech::LanguageCode language_code,
                          speech::SodaInstaller::ErrorCode error_code) override;
  void OnSodaProgress(speech::LanguageCode language_code,
                      int progress) override;

  // Test helpers:
  void SetProfileForTest(Profile* profile);
  static void SetBrailleControllerForTest(
      extensions::api::braille_display_private::BrailleController* controller);
  void SetFocusRingObserverForTest(base::RepeatingCallback<void()> observer);
  // Runs when highlights are set or updated, but not when they are removed.
  void SetHighlightsObserverForTest(base::RepeatingCallback<void()> observer);
  void SetSelectToSpeakStateObserverForTest(
      base::RepeatingCallback<void()> observer);
  void SetCaretBoundsObserverForTest(
      base::RepeatingCallback<void(const gfx::Rect&)> observer);
  void SetMagnifierBoundsObserverForTest(
      base::RepeatingCallback<void()> observer);
  void SetSwitchAccessKeysForTest(const std::set<int>& action_keys,
                                  const std::string& pref_name);

  const std::set<std::string>& GetAccessibilityCommonEnabledFeaturesForTest() {
    return accessibility_common_enabled_features_;
  }
  bool IsDisableAutoclickDialogVisibleForTest();
  bool is_pumpkin_installed_for_testing() {
    return is_pumpkin_installed_for_testing_;
  }

  // Triggers a request to install Pumpkin. Runs `callback` with a value of
  // true if the install was successful. Otherwise, runs `callback` with a
  // value of false.
  void InstallPumpkinForDictation(InstallPumpkinCallback callback);

  // Reads the contents of a DLC file and runs `callback` with the results.
  void GetDlcContents(::extensions::api::accessibility_private::DlcType dlc,
                      GetDlcContentsCallback callback);
  void SetDlcPathForTest(base::FilePath path);

 protected:
  AccessibilityManager();
  ~AccessibilityManager() override;

 private:
  void PostLoadChromeVox();
  void PostUnloadChromeVox();
  void PostSwitchChromeVoxProfile();

  void PostLoadSelectToSpeak();
  void PostUnloadSelectToSpeak();

  void PostLoadSwitchAccess();
  void PostUnloadSwitchAccess();

  void PostLoadAccessibilityCommon();
  void PostUnloadAccessibilityCommon();

  void UpdateEnhancedNetworkTts();
  void LoadEnhancedNetworkTts();
  void UnloadEnhancedNetworkTts();
  void PostLoadEnhancedNetworkTts();

  void UpdateAlwaysShowMenuFromPref();
  void OnLargeCursorChanged();
  void OnLiveCaptionChanged();
  void OnStickyKeysChanged();
  void OnSpokenFeedbackChanged();
  void OnHighContrastChanged();
  void OnVirtualKeyboardChanged();
  void OnMonoAudioChanged();
  void OnCaretHighlightChanged();
  void OnCursorHighlightChanged();
  void OnFocusHighlightChanged();
  void OnTapDraggingChanged();
  void OnSelectToSpeakChanged();
  void OnAccessibilityCommonChanged(const std::string& pref_name);
  void OnSwitchAccessChanged();
  void OnFocusChangedInPage(const content::FocusedNodeDetails& details);
  // |triggered_by_user| is false when Dictation pref is changed at startup,
  // and true if Dictation enabled changed because the user changed their
  // Dictation enabled setting in Chrome OS settings or in the tray quick
  // settings menu.
  void OnDictationChanged(bool triggered_by_user);
  // Called after the Dictation locale pref is changed.
  void OnDictationLocaleChanged();
  // Called after Dictation is enabled by the user to ensure the correct
  // dialogs/downloads occur.
  void MaybeShowNetworkDictationDialogOrInstallSoda(
      const std::string& dictation_locale);

  void CheckBrailleState();
  void ReceiveBrailleDisplayState(
      std::unique_ptr<extensions::api::braille_display_private::DisplayState>
          state);
  void UpdateBrailleImeState();

  void SetProfile(Profile* profile);

  void SetProfileByUser(const user_manager::User* user);

  void UpdateChromeOSAccessibilityHistograms();

  void PlayVolumeAdjustSound();

  // session_manager::SessionManagerObserver:
  void OnLoginOrLockScreenVisible() override;

  // Sets the current profile using the active profile.
  void SetActiveProfile();

  // extensions::api::braille_display_private::BrailleObserver implementation.
  // Enables spoken feedback if a braille display becomes available.
  void OnBrailleDisplayStateChanged(
      const extensions::api::braille_display_private::DisplayState&
          display_state) override;
  void OnBrailleKeyEvent(
      const extensions::api::braille_display_private::KeyEvent& event) override;

  // ExtensionRegistryObserver implementation.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnShutdown(extensions::ExtensionRegistry* registry) override;

  // InputMethodManager::Observer
  void InputMethodChanged(input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

  // CrasAudioHandler::AudioObserver:
  void OnActiveOutputNodeChanged() override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Dictation dialog methods.
  bool ShouldShowNetworkDictationDialog(const std::string& locale);
  void ShowNetworkDictationDialog();
  void OnNetworkDictationDialogAccepted();
  void OnNetworkDictationDialogDismissed();

  // SODA-related methods.
  void MaybeInstallSoda(const std::string& locale);
  void OnSodaInstallUpdated(int progress);
  bool ShouldShowSodaSucceededNotificationForDictation();
  bool ShouldShowSodaFailedNotificationForDictation(
      speech::LanguageCode language_code);

  // Updates the Dictation notification according to DLC states. Assumes that
  // it's only called when a Dictation-related DLC has downloaded (or failed to
  // download).
  void UpdateDictationNotification();

  void ShowDictationLanguageUpgradedNudge(const std::string& locale);
  speech::LanguageCode GetDictationLanguageCode();

  void CreateChromeVoxPanel();

  // Pumpkin-related methods.
  void OnPumpkinInstalled(bool success);
  void OnPumpkinError(const std::string& error);
  void OnPumpkinDataCreated(
      absl::optional<::extensions::api::accessibility_private::PumpkinData>
          data);

  void OnAppTerminating();

  // Returns a full file path given a DLC.
  base::FilePath TtsDlcTypeToPath(
      ::extensions::api::accessibility_private::DlcType dlc);

  // Profile which has the current a11y context.
  raw_ptr<Profile, ExperimentalAsh> profile_ = nullptr;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  std::unique_ptr<PrefChangeRegistrar> local_state_pref_change_registrar_;

  bool spoken_feedback_enabled_ = false;
  bool select_to_speak_enabled_ = false;
  bool switch_access_enabled_ = false;

  bool start_chromevox_with_tutorial_ = false;

  // A set of pref names of enabled accessibility features using the
  // accessibility common extension.
  std::set<std::string> accessibility_common_enabled_features_;

  AccessibilityStatusCallbackList callback_list_;

  std::unique_ptr<AccessibilityServiceClient> accessibility_service_client_;

  bool braille_display_connected_ = false;
  base::ScopedObservation<
      extensions::api::braille_display_private::BrailleController,
      extensions::api::braille_display_private::BrailleObserver>
      scoped_braille_observation_{this};

  base::ScopedObservation<speech::SodaInstaller,
                          speech::SodaInstaller::Observer>
      soda_observation_{this};

  bool braille_ime_current_ = false;

  raw_ptr<ChromeVoxPanel, ExperimentalAsh> chromevox_panel_ = nullptr;
  std::unique_ptr<AccessibilityPanelWidgetObserver>
      chromevox_panel_widget_observer_;

  std::string keyboard_listener_extension_id_;
  bool keyboard_listener_capture_ = false;

  // Listen to extension unloaded notifications.
  base::ScopedMultiSourceObservation<extensions::ExtensionRegistry,
                                     extensions::ExtensionRegistryObserver>
      extension_registry_observations_{this};

  std::unique_ptr<AccessibilityExtensionLoader>
      accessibility_common_extension_loader_;

  std::unique_ptr<AccessibilityExtensionLoader> chromevox_loader_;

  std::unique_ptr<AccessibilityExtensionLoader> select_to_speak_loader_;

  std::unique_ptr<SelectToSpeakEventHandlerDelegateImpl>
      select_to_speak_event_handler_delegate_;

  std::unique_ptr<AccessibilityExtensionLoader> switch_access_loader_;

  std::unique_ptr<PumpkinInstaller> pumpkin_installer_;

  std::map<std::string, std::set<std::string>>
      focus_ring_names_for_extension_id_;

  bool app_terminating_ = false;

  bool dictation_active_ = false;
  bool network_dictation_dialog_is_showing_ = false;
  // Whether a SODA download failed notification has been shown. This is
  // reset each time download is initialized because each download attempt
  // could fail separately.
  bool soda_failed_notification_shown_ = false;
  bool dictation_triggered_by_user_ = false;
  bool ignore_dictation_locale_pref_change_ = false;

  base::RepeatingCallback<void()> highlights_observer_for_test_;
  base::RepeatingCallback<void()> select_to_speak_state_observer_for_test_;
  base::RepeatingCallback<void(const gfx::Rect&)>
      caret_bounds_observer_for_test_;
  base::RepeatingCallback<void()> magnifier_bounds_observer_for_test_;
  base::OnceClosure enhanced_network_tts_waiter_for_test_;

  // Used to set the audio focus enforcement type for ChromeVox.
  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_manager_;

  // Whether the virtual keyboard was enabled before Switch Access loaded.
  bool was_vk_enabled_before_switch_access_ = false;

  InstallPumpkinCallback install_pumpkin_callback_;
  bool is_pumpkin_installed_for_testing_ = false;

  base::FilePath dlc_path_for_test_;

  base::CallbackListSubscription focus_changed_subscription_;

  base::CallbackListSubscription on_app_terminating_subscription_;

  base::WeakPtrFactory<AccessibilityManager> weak_ptr_factory_{this};

  friend class DictationTest;
  friend class SwitchAccessTest;
  friend class AccessibilityManagerTest;
  friend class AccessibilityManagerDlcTest;
  friend class AccessibilityManagerDictationDialogTest;
  friend class AccessibilityManagerNoOnDeviceSpeechRecognitionTest;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_MANAGER_H_
