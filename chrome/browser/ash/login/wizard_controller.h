// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_WIZARD_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_WIZARD_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
// TODO(https://crbug.com/1164001): move KioskAppType to forward declaration
// when moved to chrome/browser/ash/.
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/enrollment/auto_enrollment_controller.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/screens/active_directory_login_screen.h"
#include "chrome/browser/ash/login/screens/arc_terms_of_service_screen.h"
#include "chrome/browser/ash/login/screens/assistant_optin_flow_screen.h"
#include "chrome/browser/ash/login/screens/demo_preferences_screen.h"
#include "chrome/browser/ash/login/screens/demo_setup_screen.h"
#include "chrome/browser/ash/login/screens/edu_coexistence_login_screen.h"
#include "chrome/browser/ash/login/screens/enable_adb_sideloading_screen.h"
#include "chrome/browser/ash/login/screens/enable_debugging_screen.h"
#include "chrome/browser/ash/login/screens/eula_screen.h"
#include "chrome/browser/ash/login/screens/family_link_notice_screen.h"
#include "chrome/browser/ash/login/screens/fingerprint_setup_screen.h"
#include "chrome/browser/ash/login/screens/gaia_password_changed_screen.h"
#include "chrome/browser/ash/login/screens/gaia_screen.h"
#include "chrome/browser/ash/login/screens/gesture_navigation_screen.h"
#include "chrome/browser/ash/login/screens/hid_detection_screen.h"
#include "chrome/browser/ash/login/screens/kiosk_autolaunch_screen.h"
#include "chrome/browser/ash/login/screens/locale_switch_screen.h"
#include "chrome/browser/ash/login/screens/marketing_opt_in_screen.h"
#include "chrome/browser/ash/login/screens/multidevice_setup_screen.h"
#include "chrome/browser/ash/login/screens/network_screen.h"
#include "chrome/browser/ash/login/screens/offline_login_screen.h"
#include "chrome/browser/ash/login/screens/packaged_license_screen.h"
#include "chrome/browser/ash/login/screens/parental_handoff_screen.h"
#include "chrome/browser/ash/login/screens/pin_setup_screen.h"
#include "chrome/browser/ash/login/screens/recommend_apps_screen.h"
#include "chrome/browser/ash/login/screens/signin_fatal_error_screen.h"
#include "chrome/browser/ash/login/screens/sync_consent_screen.h"
#include "chrome/browser/ash/login/screens/terms_of_service_screen.h"
#include "chrome/browser/ash/login/screens/update_screen.h"
#include "chrome/browser/ash/login/screens/user_creation_screen.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
// TODO(https://crbug.com/1164001): move LoginDisplayHost to forward
// declaration when moved to chrome/browser/ash/.
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "components/account_id/account_id.h"

class PrefService;

namespace chromeos {

namespace login {
class NetworkStateHelper;
}  // namespace login

class DemoSetupController;
class ErrorScreen;
struct Geoposition;
class SimpleGeolocationProvider;
class TimeZoneProvider;
struct TimeZoneResponseData;

// Class that manages control flow between wizard screens. Wizard controller
// interacts with screen controllers to move the user between screens.
class WizardController {
 public:
  class ScreenObserver : public base::CheckedObserver {
   public:
    virtual void OnCurrentScreenChanged(BaseScreen* new_screen) = 0;
    virtual void OnShutdown() = 0;
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other). Entries should be never modified
  // or deleted. Only additions possible.
  enum class ScreenShownStatus { kSkipped = 0, kShown = 1, kMaxValue = kShown };

  WizardController();
  ~WizardController();

  // Returns the default wizard controller if it has been created. This is a
  // helper for LoginDisplayHost::default_host()->GetWizardController();
  static WizardController* default_controller();

  // Whether to skip any screens that may normally be shown after login
  // (registration, Terms of Service, user image selection).
  static bool skip_post_login_screens() { return skip_post_login_screens_; }

  // Whether to skip any prompts that may be normally shown during enrollment.
  static bool skip_enrollment_prompts() { return skip_enrollment_prompts_; }

  // Sets delays to zero. MUST be used only for tests.
  static void SetZeroDelays();

  // If true zero delays have been enabled (for browser tests).
  static bool IsZeroDelayEnabled();

  // Skips any screens that may normally be shown after login (registration,
  // Terms of Service, user image selection).
  static void SkipPostLoginScreensForTesting();

  // Skips any enrollment prompts that may be normally shown.
  static void SkipEnrollmentPromptsForTesting();

  // Forces screens that should only appear in chrome branded builds to show.
  static std::unique_ptr<base::AutoReset<bool>> ForceBrandedBuildForTesting(
      bool value);

  // Returns true if OOBE is operating under the
  // Zero-Touch Hands-Off Enrollment Flow.
  static bool UsingHandsOffEnrollment();

  // Returns true if this is a branded build, value could be overwritten by
  // `ForceBrandedBuildForTesting`.
  static bool IsBrandedBuild() { return is_branded_build_; }

  bool is_initialized() { return is_initialized_; }

  // Shows the first screen defined by `first_screen` or by default if the
  // parameter is empty.
  void Init(OobeScreenId first_screen);

  // Advances to screen defined by `screen` and shows it. Might show HID
  // detection screen in case HID connection is needed and screen_id ==
  // OobeScreen::SCREEN_UNKNOWN.
  void AdvanceToScreen(OobeScreenId screen_id);

  // Advances to screen defined by `screen` and shows it.
  void AdvanceToScreenAfterHIDDetection(OobeScreenId first_screen);

  // Returns `true` if accelerator `action` was handled by current screen
  // or WizardController itself.
  bool HandleAccelerator(ash::LoginAcceleratorAction action);

  // Starts Demo Mode setup flow. The flow starts from network screen and reuses
  // some of regular OOBE screens. It consists of the following screens:
  //    chromeos::DemoPreferencesScreenView::kScreenId
  //    chromeos::NetworkScreenView::kScreenId
  //    chromeos::EulaView::kScreenId
  //    chromeos::ArcTermsOfServiceScreenView::kScreenId
  //    chromeos::UpdateView::kScreenId
  //    chromeos::DemoSetupScreenView::kScreenId
  void StartDemoModeSetup();

  // Simulates demo mode setup environment. If `demo_config` has a value, it
  // is explicitly set on DemoSetupController and going through demo settings
  // screens can be skipped.
  void SimulateDemoModeSetupForTesting(
      base::Optional<DemoSession::DemoModeConfig> demo_config = base::nullopt);

  // Stores authorization data that will be used to configure extra auth factors
  // during user onboarding.
  void SetAuthSessionForOnboarding(const UserContext& auth_session);

  // Advances to login/update screen. Should be used in for testing only.
  void SkipToLoginForTesting();
  void SkipToUpdateForTesting();

  // Skip update, go straight to enrollment after EULA is accepted.
  void SkipUpdateEnrollAfterEula();

  // Returns current DemoSetupController if demo setup flow is in progress or
  // nullptr otherwise.
  DemoSetupController* demo_setup_controller() const {
    return demo_setup_controller_.get();
  }

  // Returns a pointer to the current screen or nullptr if there's no such
  // screen.
  BaseScreen* current_screen() const { return current_screen_; }

  // Returns true if the current wizard instance has reached the login screen.
  bool login_screen_started() const { return login_screen_started_; }

  // Returns true if a given screen exists.
  bool HasScreen(OobeScreenId screen_id);

  // Returns a given screen. Creates it lazily.
  BaseScreen* GetScreen(OobeScreenId screen_id);

  // Returns the current ScreenManager instance.
  ScreenManager* screen_manager() const { return screen_manager_.get(); }

  template <typename TScreen>
  TScreen* GetScreen() const {
    return static_cast<TScreen*>(
        screen_manager()->GetScreen(TScreen::TView::kScreenId));
  }

  // Returns the current WizardContext instance.
  WizardContext* get_wizard_context_for_testing() const {
    return wizard_context_.get();
  }

  // Volume percent at which spoken feedback is still audible.
  static const int kMinAudibleOutputVolumePercent;

  // Set the current screen. For Test use only.
  void SetCurrentScreenForTesting(BaseScreen* screen);

  void SetSharedURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> factory);

  // Configure and show GAIA password changed screen.
  void ShowGaiaPasswordChangedScreen(const AccountId& account_id,
                                     bool has_error);

  // Configure and show active directory password change screen.
  void ShowActiveDirectoryPasswordChangeScreen(const std::string& username);

  // Configure and show the signin fatal error screen.
  void ShowSignInFatalErrorScreen(SignInFatalErrorScreen::Error error,
                                  const base::Value* params);

  // Show Family Link notice screen.
  void ShowFamilyLinkNoticeScreen();

  // Set pref value for first run.
  void PrepareFirstRunPrefs();

  // Returns true if we are in user creation screen or gaia signin screen.
  static bool IsSigninScreen(OobeScreenId screen_id);

  OobeScreenId first_screen_for_testing() const {
    return first_screen_for_testing_;
  }

  void AddObserver(ScreenObserver* obs);
  void RemoveObserver(ScreenObserver* obs);

 private:
  // Create BaseScreen instances. These are owned by `screen_manager_`.
  std::vector<std::unique_ptr<BaseScreen>> CreateScreens();

  // Show specific screen.
  void ShowWelcomeScreen();
  void ShowNetworkScreen();
  void ShowEulaScreen();
  void ShowEnrollmentScreen();
  void ShowDemoModeSetupScreen();
  void ShowDemoModePreferencesScreen();
  void ShowResetScreen();
  void ShowKioskAutolaunchScreen();
  void ShowEnableAdbSideloadingScreen();
  void ShowEnableDebuggingScreen();
  void ShowKioskEnableScreen();
  void ShowTermsOfServiceScreen();
  void ShowSyncConsentScreen();
  void ShowFingerprintSetupScreen();
  void ShowArcTermsOfServiceScreen();
  void ShowRecommendAppsScreen();
  void ShowAppDownloadingScreen();
  void ShowWrongHWIDScreen();
  void ShowAutoEnrollmentCheckScreen();
  void ShowHIDDetectionScreen();
  void ShowDeviceDisabledScreen();
  void ShowEncryptionMigrationScreen();
  void ShowSupervisionTransitionScreen();
  void ShowUpdateRequiredScreen();
  void ShowAssistantOptInFlowScreen();
  void ShowMultiDeviceSetupScreen();
  void ShowGestureNavigationScreen();
  void ShowPinSetupScreen();
  void ShowMarketingOptInScreen();
  void ShowPackagedLicenseScreen();
  void ShowEduCoexistenceLoginScreen();
  void ShowParentalHandoffScreen();

  // Shows images login screen.
  void ShowLoginScreen();

  // Check if advancing to `screen` is allowed using screen priorities. Return
  // true if the priority of `screen` is higher or equal to current screen.
  bool CanNavigateTo(OobeScreenId screen_id);

  // Shows default screen depending on device ownership.
  void OnOwnershipStatusCheckDone(
      DeviceSettingsService::OwnershipStatus status);

  // Shared actions to be performed on a screen exit.
  // `exit_reason` is the screen specific exit reason reported by the screen.
  void OnScreenExit(OobeScreenId screen, const std::string& exit_reason);

  // Exit handlers:
  void OnWrongHWIDScreenExit();
  void OnHidDetectionScreenExit(HIDDetectionScreen::Result result);
  void OnWelcomeScreenExit(WelcomeScreen::Result result);
  void OnNetworkScreenExit(NetworkScreen::Result result);
  bool ShowEulaOrArcTosAfterNetworkScreen();
  void OnEulaScreenExit(EulaScreen::Result result);
  void OnEulaAccepted(bool usage_statistics_reporting_enabled);
  void OnUpdateScreenExit(UpdateScreen::Result result);
  void OnUpdateCompleted();
  void OnAutoEnrollmentCheckScreenExit();
  void OnEnrollmentScreenExit(EnrollmentScreen::Result result);
  void OnEnrollmentDone();
  void OnEnableAdbSideloadingScreenExit();
  void OnEnableDebuggingScreenExit();
  void OnKioskEnableScreenExit();
  void OnKioskAutolaunchScreenExit(KioskAutolaunchScreen::Result result);
  void OnDemoPreferencesScreenExit(DemoPreferencesScreen::Result result);
  void OnDemoSetupScreenExit(DemoSetupScreen::Result result);
  void OnLocaleSwitchScreenExit(LocaleSwitchScreen::Result result);
  void OnTermsOfServiceScreenExit(TermsOfServiceScreen::Result result);
  void OnFingerprintSetupScreenExit(FingerprintSetupScreen::Result result);
  void OnSyncConsentScreenExit(SyncConsentScreen::Result result);
  void OnPinSetupScreenExit(PinSetupScreen::Result result);
  void OnArcTermsOfServiceScreenExit(ArcTermsOfServiceScreen::Result result);
  void OnArcTermsOfServiceAccepted();
  void OnRecommendAppsScreenExit(RecommendAppsScreen::Result result);
  void OnAppDownloadingScreenExit();
  void OnAssistantOptInFlowScreenExit(AssistantOptInFlowScreen::Result result);
  void OnMultiDeviceSetupScreenExit(MultiDeviceSetupScreen::Result result);
  void OnGestureNavigationScreenExit(GestureNavigationScreen::Result result);
  void OnMarketingOptInScreenExit(MarketingOptInScreen::Result result);
  void OnResetScreenExit();
  void OnDeviceModificationCanceled();
  void OnSupervisionTransitionScreenExit();
  void OnUpdateRequiredScreenExit();
  void OnOobeFlowFinished();
  void OnPackagedLicenseScreenExit(PackagedLicenseScreen::Result result);
  void OnActiveDirectoryPasswordChangeScreenExit();
  void OnFamilyLinkNoticeScreenExit(FamilyLinkNoticeScreen::Result result);
  void OnUserCreationScreenExit(UserCreationScreen::Result result);
  void OnGaiaScreenExit(GaiaScreen::Result result);
  void OnPasswordChangeScreenExit(GaiaPasswordChangedScreen::Result result);
  void OnActiveDirectoryLoginScreenExit();
  void OnSignInFatalErrorScreenExit();
  void OnEduCoexistenceLoginScreenExit(
      EduCoexistenceLoginScreen::Result result);
  void OnParentalHandoffScreenExit(ParentalHandoffScreen::Result result);
  void OnOfflineLoginScreenExit(OfflineLoginScreen::Result result);

  // Callback invoked once it has been determined whether the device is disabled
  // or not.
  void OnDeviceDisabledChecked(bool device_disabled);

  // Callback function after setting MetricsReporting.
  void OnChangedMetricsReportingState(bool enabled);

  // Shows update screen and starts update process.
  void InitiateOOBEUpdate();
  void StartOOBEUpdate();

  // Retrieve filtered OOBE configuration and apply relevant values.
  void UpdateOobeConfiguration();

  // Actions that should be done right after EULA is accepted,
  // before update check.
  void PerformPostEulaActions();

  // Actions that should be done right after update stage is finished.
  void PerformOOBECompletedActions();

  ErrorScreen* GetErrorScreen();
  void ShowErrorScreen();

  void OnHIDScreenNecessityCheck(bool screen_needed);

  // Notification of a change in the state of an accessibility setting.
  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details);

  // Switches from one screen to another.
  void SetCurrentScreen(BaseScreen* screen);

  // Update the status area visibility for `screen`.
  void UpdateStatusAreaVisibilityForScreen(OobeScreenId screen_id);

  // Launched kiosk app configured for auto-launch.
  void AutoLaunchKioskApp(KioskAppType app_type);

  // Called when LocalState is initialized.
  void OnLocalStateInitialized(bool /* succeeded */);

  // Returns local state.
  PrefService* GetLocalState();

  static void set_local_state_for_testing(PrefService* local_state) {
    local_state_for_testing_ = local_state;
  }

  // Starts a network request to resolve the timezone. Skips the request
  // completely when the timezone is overridden through the command line.
  void StartNetworkTimezoneResolve();
  void StartTimezoneResolve();

  // Creates provider on demand.
  TimeZoneProvider* GetTimezoneProvider();

  // TimeZoneRequest::TimeZoneResponseCallback implementation.
  void OnTimezoneResolved(std::unique_ptr<TimeZoneResponseData> timezone,
                          bool server_error);

  // Called from SimpleGeolocationProvider when location is resolved.
  void OnLocationResolved(const Geoposition& position,
                          bool server_error,
                          const base::TimeDelta elapsed);

  // Returns true if callback has been installed.
  // Returns false if timezone has already been resolved.
  bool SetOnTimeZoneResolvedForTesting(base::OnceClosure callback);

  // Start the enrollment screen using the config from
  // `prescribed_enrollment_config_`. If `force_interactive` is true,
  // the user will be presented with a manual enrollment screen requiring
  // Gaia credentials. If it is false, the screen may return after trying
  // attestation-based enrollment if appropriate.
  void StartEnrollmentScreen(bool force_interactive);
  void ShowEnrollmentScreenIfEligible();

  void NotifyScreenChanged();

  // Returns auto enrollment controller (lazily initializes one if it doesn't
  // exist already).
  AutoEnrollmentController* GetAutoEnrollmentController();

  std::unique_ptr<AutoEnrollmentController> auto_enrollment_controller_;
  std::unique_ptr<ScreenManager> screen_manager_;
  std::unique_ptr<WizardContext> wizard_context_;

  // Whether to skip any screens that may normally be shown after login
  // (registration, Terms of Service, user image selection).
  static bool skip_post_login_screens_;

  static bool skip_enrollment_prompts_;

  // Screen that's currently active.
  BaseScreen* current_screen_ = nullptr;

  // Screen that was active before, or nullptr for login screen.
  BaseScreen* previous_screen_ = nullptr;

  // True if this is a branded build (i.e. Google Chrome).
  static bool is_branded_build_;

  // True if full OOBE flow should be shown.
  bool is_out_of_box_ = false;

  // Value of the screen name that WizardController was started with.
  OobeScreenId first_screen_for_testing_ = OobeScreen::SCREEN_UNKNOWN;

  // The prescribed enrollment configuration for the device.
  policy::EnrollmentConfig prescribed_enrollment_config_;

  // Whether the auto-enrollment check should be retried or the cached result
  // returned if present.
  bool retry_auto_enrollment_check_ = false;

  // Time when the EULA was accepted. Used to measure the duration from the EULA
  // acceptance until the Sign-In screen is displayed.
  base::TimeTicks time_eula_accepted_;

  // Whether OOBE has yet been marked as completed.
  bool oobe_marked_completed_ = false;

  bool login_screen_started_ = false;

  // Non-owning pointer to local state used for testing.
  static PrefService* local_state_for_testing_;

  FRIEND_TEST_ALL_PREFIXES(EnrollmentScreenTest, TestCancel);
  FRIEND_TEST_ALL_PREFIXES(WizardControllerFlowTest, Accelerators);
  FRIEND_TEST_ALL_PREFIXES(WizardControllerFlowTest,
                           ControlFlowSkipUpdateEnroll);
  FRIEND_TEST_ALL_PREFIXES(WizardControllerDeviceStateTest,
                           ControlFlowNoForcedReEnrollmentOnFirstBoot);

  friend class AutoEnrollmentLocalPolicyServer;
  friend class WizardControllerBrokenLocalStateTest;
  friend class WizardControllerDeviceStateTest;
  friend class WizardControllerFlowTest;
  friend class WizardControllerOobeConfigurationTest;
  friend class WizardControllerOobeResumeTest;
  friend class WizardControllerScreenPriorityTest;
  friend class WizardControllerSupervisionTransitionOobeTest;

  base::CallbackListSubscription accessibility_subscription_;

  std::unique_ptr<SimpleGeolocationProvider> geolocation_provider_;
  std::unique_ptr<TimeZoneProvider> timezone_provider_;

  // Helper for network realted operations.
  std::unique_ptr<login::NetworkStateHelper> network_state_helper_;

  // Controller of the demo mode setup. It has the lifetime of the single demo
  // mode setup flow.
  std::unique_ptr<DemoSetupController> demo_setup_controller_;

  // Maps screen names to last time of their shows.
  std::map<OobeScreenId, base::TimeTicks> screen_show_times_;

  // Tests check result of timezone resolve.
  bool timezone_resolved_ = false;
  base::OnceClosure on_timezone_resolved_for_testing_;

  bool is_initialized_ = false;

  base::ObserverList<ScreenObserver> screen_observers_;

  base::WeakPtrFactory<WizardController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WizardController);
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after //chrome/browser/chromeos
// source migration is finished.
using ::chromeos::WizardController;

#endif  // CHROME_BROWSER_ASH_LOGIN_WIZARD_CONTROLLER_H_
