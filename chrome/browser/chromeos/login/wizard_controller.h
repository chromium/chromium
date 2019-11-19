// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_H_

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
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_controller.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/screens/arc_terms_of_service_screen.h"
#include "chrome/browser/chromeos/login/screens/demo_preferences_screen.h"
#include "chrome/browser/chromeos/login/screens/demo_setup_screen.h"
#include "chrome/browser/chromeos/login/screens/enable_adb_sideloading_screen.h"
#include "chrome/browser/chromeos/login/screens/enable_debugging_screen.h"
#include "chrome/browser/chromeos/login/screens/eula_screen.h"
#include "chrome/browser/chromeos/login/screens/kiosk_autolaunch_screen.h"
#include "chrome/browser/chromeos/login/screens/network_screen.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps_screen.h"
#include "chrome/browser/chromeos/login/screens/terms_of_service_screen.h"
#include "chrome/browser/chromeos/login/screens/update_screen.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"

class PrefService;

namespace chromeos {

namespace login {
class NetworkStateHelper;
}  // namespace login

class DemoSetupController;
class ErrorScreen;
struct Geoposition;
class LoginDisplayHost;
class LoginScreenContext;
class SimpleGeolocationProvider;
class TimeZoneProvider;
struct TimeZoneResponseData;

// Class that manages control flow between wizard screens. Wizard controller
// interacts with screen controllers to move the user between screens.
class WizardController {
 public:
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
  static std::unique_ptr<base::AutoReset<bool>> ForceBrandedBuildForTesting();

  // Returns true if OOBE is operating under the
  // Zero-Touch Hands-Off Enrollment Flow.
  static bool UsingHandsOffEnrollment();

  // Shows the first screen defined by |first_screen| or by default if the
  // parameter is empty.
  void Init(OobeScreenId first_screen);

  // Advances to screen defined by |screen| and shows it.
  void AdvanceToScreen(OobeScreenId screen);

  // Starts Demo Mode setup flow. The flow starts from network screen and reuses
  // some of regular OOBE screens. It consists of the following screens:
  //    chromeos::DemoPreferencesScreenView::kScreenId
  //    chromeos::NetworkScreenView::kScreenId
  //    chromeos::EulaView::kScreenId
  //    chromeos::ArcTermsOfServiceScreenView::kScreenId
  //    chromeos::UpdateView::kScreenId
  //    chromeos::DemoSetupScreenView::kScreenId
  void StartDemoModeSetup();

  // Simulates demo mode setup environment. If |demo_config| has a value, it
  // is explicitly set on DemoSetupController and going through demo settings
  // screens can be skipped.
  void SimulateDemoModeSetupForTesting(
      base::Optional<DemoSession::DemoModeConfig> demo_config = base::nullopt);

  // Advances to login/update screen. Should be used in for testing only.
  void SkipToLoginForTesting(const LoginScreenContext& context);
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
  bool HasScreen(OobeScreenId screen);

  // Returns a given screen. Creates it lazily.
  BaseScreen* GetScreen(OobeScreenId screen);

  // Returns the current ScreenManager instance.
  ScreenManager* screen_manager() const { return screen_manager_.get(); }

  // Volume percent at which spoken feedback is still audible.
  static const int kMinAudibleOutputVolumePercent;

  // Set the current screen. For Test use only.
  void SetCurrentScreenForTesting(BaseScreen* screen);

  void SetSharedURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> factory);

 private:
  // Create BaseScreen instances. These are owned by |screen_manager_|.
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
  void ShowArcKioskSplashScreen();
  void ShowHIDDetectionScreen();
  void ShowDeviceDisabledScreen();
  void ShowEncryptionMigrationScreen();
  void ShowSupervisionTransitionScreen();
  void ShowUpdateRequiredScreen();
  void ShowAssistantOptInFlowScreen();
  void ShowMultiDeviceSetupScreen();
  void ShowDiscoverScreen();
  void ShowMarketingOptInScreen();

  // Shows images login screen.
  void ShowLoginScreen(const LoginScreenContext& context);

  // Shared actions to be performed on a screen exit.
  // |exit_code| is the screen specific exit code reported by the screen.
  void OnScreenExit(OobeScreenId screen, int exit_code);

  // Exit handlers:
  void OnWrongHWIDScreenExit();
  void OnHidDetectionScreenExit();
  void OnWelcomeScreenExit();
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
  void OnTermsOfServiceScreenExit(TermsOfServiceScreen::Result result);
  void OnTermsOfServiceAccepted();
  void OnSyncConsentScreenExit();
  void OnSyncConsentFinished();
  void OnFingerprintSetupScreenExit();
  void OnDiscoverScreenExit();
  void OnMarketingOptInScreenExit();
  void OnArcTermsOfServiceScreenExit(ArcTermsOfServiceScreen::Result result);
  void OnArcTermsOfServiceSkipped();
  void OnArcTermsOfServiceAccepted();
  void OnRecommendAppsScreenExit(RecommendAppsScreen::Result result);
  void OnAppDownloadingScreenExit();
  void OnAssistantOptInFlowScreenExit();
  void OnMultiDeviceSetupScreenExit();
  void OnResetScreenExit();
  void OnDeviceModificationCanceled();
  void OnSupervisionTransitionScreenExit();
  void OnOobeFlowFinished();

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

  // Notification of Guest Mode policy changes.
  void OnGuestModePolicyUpdated();

  // Switches from one screen to another.
  void SetCurrentScreen(BaseScreen* screen);

  // Update the status area visibility for |screen|.
  void UpdateStatusAreaVisibilityForScreen(OobeScreenId screen);

  // Launched kiosk app configured for auto-launch.
  void AutoLaunchKioskApp();

  // Called when LocalState is initialized.
  void OnLocalStateInitialized(bool /* succeeded */);

  // Returns local state.
  PrefService* GetLocalState();

  static void set_local_state_for_testing(PrefService* local_state) {
    local_state_for_testing_ = local_state;
  }

  OobeScreenId first_screen() const { return first_screen_; }

  // Called when network is UP.
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
  bool SetOnTimeZoneResolvedForTesting(const base::Closure& callback);

  // Start the enrollment screen using the config from
  // |prescribed_enrollment_config_|. If |force_interactive| is true,
  // the user will be presented with a manual enrollment screen requiring
  // Gaia credentials. If it is false, the screen may return after trying
  // attestation-based enrollment if appropriate.
  void StartEnrollmentScreen(bool force_interactive);

  void OnConfigurationLoaded(
      OobeScreenId first_screen,
      std::unique_ptr<base::DictionaryValue> configuration);

  // Returns auto enrollment controller (lazily initializes one if it doesn't
  // exist already).
  AutoEnrollmentController* GetAutoEnrollmentController();

  std::unique_ptr<AutoEnrollmentController> auto_enrollment_controller_;
  std::unique_ptr<ScreenManager> screen_manager_;

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
  OobeScreenId first_screen_ = OobeScreen::SCREEN_UNKNOWN;

  // If true then update check is cancelled and enrollment is started after
  // EULA is accepted.
  bool skip_update_enroll_after_eula_ = false;

  // The prescribed enrollment configuration for the device.
  policy::EnrollmentConfig prescribed_enrollment_config_;

  // Whether the auto-enrollment check should be retried or the cached result
  // returned if present.
  bool retry_auto_enrollment_check_ = false;

  // Time when the EULA was accepted. Used to measure the duration from the EULA
  // acceptance until the Sign-In screen is displayed.
  base::Time time_eula_accepted_;

  // Time when OOBE was started. Used to measure the total time from boot to
  // user Sign-In completed.
  base::Time time_oobe_started_;

  // Whether OOBE has yet been marked as completed.
  bool oobe_marked_completed_ = false;

  bool login_screen_started_ = false;

  // Non-owning pointer to local state used for testing.
  static PrefService* local_state_for_testing_;

  FRIEND_TEST_ALL_PREFIXES(EnrollmentScreenTest, TestCancel);
  FRIEND_TEST_ALL_PREFIXES(WizardControllerFlowTest, Accelerators);
  FRIEND_TEST_ALL_PREFIXES(WizardControllerDeviceStateTest,
                           ControlFlowNoForcedReEnrollmentOnFirstBoot);

  friend class AutoEnrollmentLocalPolicyServer;
  friend class WizardControllerBrokenLocalStateTest;
  friend class WizardControllerDeviceStateTest;
  friend class WizardControllerFlowTest;
  friend class WizardControllerOobeConfigurationTest;
  friend class WizardControllerOobeResumeTest;
  friend class WizardControllerSupervisionTransitionOobeTest;

  std::unique_ptr<AccessibilityStatusSubscription> accessibility_subscription_;
  std::unique_ptr<CrosSettings::ObserverSubscription>
      guest_mode_policy_subscription_;

  std::unique_ptr<SimpleGeolocationProvider> geolocation_provider_;
  std::unique_ptr<TimeZoneProvider> timezone_provider_;

  // Helper for network realted operations.
  std::unique_ptr<login::NetworkStateHelper> network_state_helper_;

  // Controller of the demo mode setup. It has the lifetime of the single demo
  // mode setup flow.
  std::unique_ptr<DemoSetupController> demo_setup_controller_;

  // Maps screen names to last time of their shows.
  std::map<OobeScreenId, base::Time> screen_show_times_;

  // Tests check result of timezone resolve.
  bool timezone_resolved_ = false;
  base::Closure on_timezone_resolved_for_testing_;

  // Configuration (dictionary) for automating OOBE screens.
  base::Value oobe_configuration_{base::Value::Type::DICTIONARY};

  BaseScreen* hid_screen_ = nullptr;

  base::WeakPtrFactory<WizardController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WizardController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_H_
