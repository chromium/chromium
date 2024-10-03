// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_WIZARD_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_WIZARD_CONTROLLER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/choobe_flow_controller.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/enrollment/auto_enrollment_check_screen.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen.h"
#include "chrome/browser/ash/login/oobe_metrics_helper.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/quickstart_controller.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/screens/account_selection_screen.h"
#include "chrome/browser/ash/login/screens/add_child_screen.h"
#include "chrome/browser/ash/login/screens/ai_intro_screen.h"
#include "chrome/browser/ash/login/screens/assistant_optin_flow_screen.h"
#include "chrome/browser/ash/login/screens/categories_selection_screen.h"
#include "chrome/browser/ash/login/screens/choobe_screen.h"
#include "chrome/browser/ash/login/screens/consolidated_consent_screen.h"
#include "chrome/browser/ash/login/screens/consumer_update_screen.h"
#include "chrome/browser/ash/login/screens/demo_preferences_screen.h"
#include "chrome/browser/ash/login/screens/demo_setup_screen.h"
#include "chrome/browser/ash/login/screens/display_size_screen.h"
#include "chrome/browser/ash/login/screens/drive_pinning_screen.h"
#include "chrome/browser/ash/login/screens/edu_coexistence_login_screen.h"
#include "chrome/browser/ash/login/screens/enable_adb_sideloading_screen.h"
#include "chrome/browser/ash/login/screens/enable_debugging_screen.h"
#include "chrome/browser/ash/login/screens/family_link_notice_screen.h"
#include "chrome/browser/ash/login/screens/fingerprint_setup_screen.h"
#include "chrome/browser/ash/login/screens/gaia_info_screen.h"
#include "chrome/browser/ash/login/screens/gaia_screen.h"
#include "chrome/browser/ash/login/screens/gemini_intro_screen.h"
#include "chrome/browser/ash/login/screens/gesture_navigation_screen.h"
#include "chrome/browser/ash/login/screens/guest_tos_screen.h"
#include "chrome/browser/ash/login/screens/hardware_data_collection_screen.h"
#include "chrome/browser/ash/login/screens/hid_detection_screen.h"
#include "chrome/browser/ash/login/screens/locale_switch_screen.h"
#include "chrome/browser/ash/login/screens/marketing_opt_in_screen.h"
#include "chrome/browser/ash/login/screens/multidevice_setup_screen.h"
#include "chrome/browser/ash/login/screens/network_screen.h"
#include "chrome/browser/ash/login/screens/offline_login_screen.h"
#include "chrome/browser/ash/login/screens/online_authentication_screen.h"
#include "chrome/browser/ash/login/screens/os_install_screen.h"
#include "chrome/browser/ash/login/screens/os_trial_screen.h"
#include "chrome/browser/ash/login/screens/osauth/apply_online_password_screen.h"
#include "chrome/browser/ash/login/screens/osauth/cryptohome_recovery_screen.h"
#include "chrome/browser/ash/login/screens/osauth/cryptohome_recovery_setup_screen.h"
#include "chrome/browser/ash/login/screens/osauth/enter_old_password_screen.h"
#include "chrome/browser/ash/login/screens/osauth/factor_setup_success_screen.h"
#include "chrome/browser/ash/login/screens/osauth/local_data_loss_warning_screen.h"
#include "chrome/browser/ash/login/screens/osauth/local_password_setup_screen.h"
#include "chrome/browser/ash/login/screens/osauth/osauth_error_screen.h"
#include "chrome/browser/ash/login/screens/osauth/password_selection_screen.h"
#include "chrome/browser/ash/login/screens/osauth/recovery_eligibility_screen.h"
#include "chrome/browser/ash/login/screens/packaged_license_screen.h"
#include "chrome/browser/ash/login/screens/parental_handoff_screen.h"
#include "chrome/browser/ash/login/screens/perks_discovery_screen.h"
#include "chrome/browser/ash/login/screens/personalized_recommend_apps_screen.h"
#include "chrome/browser/ash/login/screens/pin_setup_screen.h"
#include "chrome/browser/ash/login/screens/quick_start_screen.h"
#include "chrome/browser/ash/login/screens/recommend_apps_screen.h"
#include "chrome/browser/ash/login/screens/remote_activity_notification_screen.h"
#include "chrome/browser/ash/login/screens/saml_confirm_password_screen.h"
#include "chrome/browser/ash/login/screens/signin_fatal_error_screen.h"
#include "chrome/browser/ash/login/screens/smart_privacy_protection_screen.h"
#include "chrome/browser/ash/login/screens/split_modifier_keyboard_info_screen.h"
#include "chrome/browser/ash/login/screens/sync_consent_screen.h"
#include "chrome/browser/ash/login/screens/terms_of_service_screen.h"
#include "chrome/browser/ash/login/screens/theme_selection_screen.h"
#include "chrome/browser/ash/login/screens/touchpad_scroll_screen.h"
#include "chrome/browser/ash/login/screens/update_screen.h"
#include "chrome/browser/ash/login/screens/user_allowlist_check_screen.h"
#include "chrome/browser/ash/login/screens/user_creation_screen.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ui/webui/ash/login/online_authentication_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/login/user_allowlist_check_screen_handler.h"
#include "components/account_id/account_id.h"

class PrefService;
struct AccessibilityStatusEventDetails;

namespace policy {
class AutoEnrollmentController;
}  // namespace policy

namespace ash {

class BaseScreen;
class DemoSetupController;
class ErrorScreen;
struct Geoposition;
class KioskApp;
class SimpleGeolocationProvider;
class TimeZoneProvider;
struct TimeZoneResponseData;
enum class KioskAppType;

// Class that manages control flow between wizard screens. Wizard controller
// interacts with screen controllers to move the user between screens.
class WizardController : public OobeUI::Observer {
 public:
  class ScreenObserver : public base::CheckedObserver {
   public:
    virtual void OnCurrentScreenChanged(BaseScreen* new_screen) = 0;
    virtual void OnShutdown() = 0;
  };

  explicit WizardController(WizardContext* wizard_context);

  WizardController(const WizardController&) = delete;
  WizardController& operator=(const WizardController&) = delete;

  ~WizardController() override;

  // Returns the default wizard controller if it has been created. This is a
  // helper for LoginDisplayHost::default_host()->GetWizardController();
  static WizardController* default_controller();

  // Whether to skip any screens that may normally be shown after login
  // (registration, Terms of Service, user image selection).
  bool skip_post_login_screens() {
    return wizard_context_->skip_post_login_screens_for_tests;
  }

  // Whether to skip any prompts that may be normally shown during enrollment.
  static bool skip_enrollment_prompts_for_testing() {
    return skip_enrollment_prompts_for_testing_;
  }

  // Sets delays to zero. MUST be used only for tests.
  static void SetZeroDelays();

  // If true zero delays have been enabled (for browser tests).
  static bool IsZeroDelayEnabled();

  // Skips any screens that may normally be shown after login (registration,
  // Terms of Service, user image selection).
  void SkipPostLoginScreensForTesting();

  // Skips any enrollment prompts that may be normally shown.
  static void SkipEnrollmentPromptsForTesting();

  // Returns true if the onboarding flow can be resumed from `screen_id`.
  static bool IsResumablePostLoginScreen(OobeScreenId screen_id);

  bool is_initialized() { return is_initialized_; }

  // Shows the first screen defined by `first_screen` or by default if the
  // parameter is empty.
  void Init(OobeScreenId first_screen);

  // Advances to screen defined by `screen` and shows it. Might show HID
  // detection screen in case HID connection is needed and screen_id ==
  // ash::OOBE_SCREEN_UNKNOWN.
  void AdvanceToScreen(OobeScreenId screen_id);

  // Advances to screen defined by `screen` and shows it.
  void AdvanceToScreenAfterHIDDetection(OobeScreenId first_screen);

  // Returns `true` if accelerator `action` was handled by current screen
  // or WizardController itself.
  bool HandleAccelerator(LoginAcceleratorAction action);

  // Starts Demo Mode setup flow. The flow starts from network screen and reuses
  // some of regular OOBE screens. It consists of the following screens:
  //    ash::NetworkScreenView::kScreenId
  //    ash::DemoPreferencesScreenView::kScreenId
  //    ash::UpdateView::kScreenId
  //    ash::ConsolidatedConsentScreenView::kScreenId
  //    ash::DemoSetupScreenView::kScreenId
  void StartDemoModeSetup();

  // Creates ChoobeFlowController. Should only be called if CHOOBE flow will be
  // started or resumed.
  void CreateChoobeFlowController();

  // Simulates demo mode setup environment. If `demo_config` has a value, it
  // is explicitly set on DemoSetupController and going through demo settings
  // screens can be skipped.
  void SimulateDemoModeSetupForTesting(
      std::optional<DemoSession::DemoModeConfig> demo_config = std::nullopt);

  // Advances to login/update screen. Should be used in for testing only.
  void SkipToLoginForTesting();

  OobeScreenId get_screen_after_managed_tos_for_testing() {
    return wizard_context_->screen_after_managed_tos;
  }

  // Returns current DemoSetupController if demo setup flow is in progress or
  // nullptr otherwise.
  DemoSetupController* demo_setup_controller() const {
    return demo_setup_controller_.get();
  }

  // Returns ChoobeFlowController if CHOOBE flow is in progress or nullptr
  // otherwise.
  ChoobeFlowController* choobe_flow_controller() const {
    return choobe_flow_controller_.get();
  }

  // Main QuickStart controller, always present.
  quick_start::QuickStartController* quick_start_controller() {
    return quickstart_controller_.get();
  }

  // Returns a pointer to the current screen or nullptr if there's no such
  // screen.
  BaseScreen* current_screen() const { return current_screen_; }

  // Returns true if a given screen exists.
  bool HasScreen(OobeScreenId screen_id);

  // Returns a given screen. Creates it lazily.
  BaseScreen* GetScreen(OobeScreenId screen_id);

  // Returns a given OobescreenId with both name and external_api_prefix.
  OobeScreenId GetScreenByName(const std::string& screen_name);

  // Returns the current ScreenManager instance.
  ScreenManager* screen_manager() const { return screen_manager_.get(); }

  template <typename TScreen>
  TScreen* GetScreen() const {
    return static_cast<TScreen*>(
        screen_manager()->GetScreen(TScreen::TView::kScreenId));
  }

  // Volume percent at which spoken feedback is still audible.
  static const int kMinAudibleOutputVolumePercent;

  // Set the current screen. For Test use only.
  void SetCurrentScreenForTesting(BaseScreen* screen);

  void SetSharedURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> factory);

  // Configure and show the signin fatal error screen.
  void ShowSignInFatalErrorScreen(SignInFatalErrorScreen::Error error,
                                  base::Value::Dict params);

  // Show Family Link notice screen.
  void ShowFamilyLinkNoticeScreen();

  // Show Cryptohome recovery screen.
  void ShowCryptohomeRecoveryScreen(std::unique_ptr<UserContext> user_context);

  // Set pref value for first run.
  void PrepareFirstRunPrefs();

  // Returns true if we are in user creation screen or gaia signin screen.
  static bool IsSigninScreen(OobeScreenId screen_id);

  OobeScreenId first_screen_for_testing() const {
    return first_screen_for_testing_;
  }

  void AddObserver(ScreenObserver* obs);
  void RemoveObserver(ScreenObserver* obs);

  // OobeUI::Observer
  void OnCurrentScreenChanged(OobeScreenId, OobeScreenId) override {}
  void OnDestroyingOobeUI() override;

  // Sets the current screen to nullptr so the next time WizardController
  // will be started it will call `Show()` on the first screen.
  void HideCurrentScreen();

  // Allows tests to call `GetAutoEnrollmentController` without making those
  // tests friend classes with access to everything.
  policy::AutoEnrollmentController* GetAutoEnrollmentControllerForTesting() {
    return GetAutoEnrollmentController();
  }

  // Returns whether the screen id belongs to the `ErrorScreen`
  static bool IsErrorScreen(OobeScreenId);

 private:
  // Create BaseScreen instances. These are owned by `screen_manager_`.
  std::vector<std::pair<OobeScreenId, std::unique_ptr<BaseScreen>>>
  CreateScreens();

  // Show specific screen.
  void ShowWelcomeScreen();
  void ShowQuickStartScreen();
  void ShowNetworkScreen();
  void ShowEnrollmentScreen();
  void ShowDemoModeSetupScreen();
  void ShowDemoModePreferencesScreen();
  void ShowResetScreen();
  void ShowEnableAdbSideloadingScreen();
  void ShowEnableDebuggingScreen();
  void ShowTermsOfServiceScreen();
  void ShowSyncConsentScreen();
  void ShowFingerprintSetupScreen();
  void ShowRecommendAppsScreen();
  void ShowRemoteActivityNotificationScreen();
  void ShowAppDownloadingScreen();
  void ShowAiIntroScreen();
  void ShowGeminiIntroScreen();
  void ShowWrongHWIDScreen();
  void ShowAutoEnrollmentCheckScreen();
  void ShowHIDDetectionScreen();
  void ShowDeviceDisabledScreen();
  void ShowEncryptionMigrationScreen();
  void ShowManagementTransitionScreen();
  void ShowUpdateRequiredScreen();
  void ShowAssistantOptInFlowScreen();
  void ShowMultiDeviceSetupScreen();
  void ShowGestureNavigationScreen();
  void ShowPinSetupScreen();
  void ShowMarketingOptInScreen();
  void ShowPackagedLicenseScreen();
  void ShowEduCoexistenceLoginScreen();
  void ShowParentalHandoffScreen();
  void ShowOsInstallScreen();
  void ShowOsTrialScreen();
  void ShowConsolidatedConsentScreen();
  void ShowCryptohomeRecoverySetupScreen();
  void ShowAuthenticationSetupScreen();
  void ShowGuestTosScreen();
  void ShowArcVmDataMigrationScreen();
  void ShowThemeSelectionScreen();
  void ShowChoobeScreen();
  void ShowTouchpadScrollScreen();
  void ShowDisplaySizeScreen();
  void ShowDrivePinningScreen();
  void ShowGaiaInfoScreen();
  void ShowAddChildScreen();
  void ShowConsumerUpdateScreen();
  void ShowPasswordSelectionScreen();
  void ShowLocalPasswordSetupScreen();
  void ShowApplyOnlinePasswordScreen();
  void ShowOSAuthErrorScreen();
  void ShowEnterOldPasswordScreen();
  void ShowLocalDataLossWarningScreen();
  void ShowFactorSetupSuccessScreen();
  void ShowCategoriesSelectionScreen();
  void ShowPersonalizedRecomendAppsScreen();
  void ShowPerksDiscoveryScreen();
  void ShowSplitModifierKeyboardInfoScreen();
  void ShowAccountSelectionScreen();
  void ShowAppLaunchSplashScreen();

  // Shows images login screen.
  void ShowLoginScreen();

  // Show OOBE screen: resume if pending screen otherwise show welcome screen.
  void ContinueOobeFlow();

  // Check if advancing to `screen` is allowed using screen priorities.
  // Return true if the priority of `screen` is higher or equal to current
  // screen.
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
  void OnQuickStartScreenExit(QuickStartScreen::Result result);
  void OnNetworkScreenExit(NetworkScreen::Result result);
  void OnUpdateScreenExit(UpdateScreen::Result result);
  void OnUpdateCompleted();
  void OnAutoEnrollmentCheckScreenExit(
      AutoEnrollmentCheckScreen::Result result);
  void OnEnrollmentScreenExit(EnrollmentScreen::Result result);
  void OnEnrollmentDone();
  void OnEnableAdbSideloadingScreenExit();
  void OnEnableDebuggingScreenExit();
  void OnDemoPreferencesScreenExit(DemoPreferencesScreen::Result result);
  void OnDemoSetupScreenExit(DemoSetupScreen::Result result);
  void OnUserCreationScreenExit(UserCreationScreen::Result result);
  // Start of online authentication sub-group
  void OnGaiaScreenExit(GaiaScreen::Result result);
  void OnSamlConfirmPasswordScreenExit(
      SamlConfirmPasswordScreen::Result result);
  // End of online authentication sub-group
  void OnLocaleSwitchScreenExit(LocaleSwitchScreen::Result result);
  void OnRecoveryEligibilityScreenExit(
      RecoveryEligibilityScreen::Result result);
  void OnTermsOfServiceScreenExit(TermsOfServiceScreen::Result result);
  void OnSyncConsentScreenExit(SyncConsentScreen::Result result);
  // Start of Local authentication setup sub-group
  // Authentication part
  void OnCryptohomeRecoveryScreenExit(CryptohomeRecoveryScreen::Result result);
  void OnEnterOldPasswordScreenExit(EnterOldPasswordScreen::Result result);
  void OnLocalDataLossWarningScreenExit(
      LocalDataLossWarningScreen::Result result);
  // Factor setup part
  void StartAuthFactorsSetup();
  void OnCryptohomeRecoverySetupScreenExit(
      CryptohomeRecoverySetupScreen::Result result);
  void OnPasswordSelectionScreenExit(PasswordSelectionScreen::Result result);
  void OnFingerprintSetupScreenExit(FingerprintSetupScreen::Result result);
  void OnPinSetupScreenExit(PinSetupScreen::Result result);
  void ObtainContextAndLoginAuthenticated();
  void LoginAuthenticatedWithContext(std::unique_ptr<UserContext> user_context);
  void ObtainContextAndAttemptLocalAuthentication();
  void AttemptLocalAuthenticationWithContext(
      std::unique_ptr<UserContext> user_context);
  void FinishAuthFactorsSetup();
  // End of Local authentication setup sub-group
  void OnRecommendAppsScreenExit(RecommendAppsScreen::Result result);
  void OnRemoteActivityNotificationScreenExit();
  void OnAppDownloadingScreenExit();
  void OnAiIntroScreenExit(AiIntroScreen::Result result);
  void OnGeminiIntroScreenExit(GeminiIntroScreen::Result result);
  void OnAssistantOptInFlowScreenExit(AssistantOptInFlowScreen::Result result);
  void OnMultiDeviceSetupScreenExit(MultiDeviceSetupScreen::Result result);
  void OnGestureNavigationScreenExit(GestureNavigationScreen::Result result);
  void OnMarketingOptInScreenExit(MarketingOptInScreen::Result result);
  void OnResetScreenExit();
  void OnDeviceModificationCanceled();
  void OnManagementTransitionScreenExit();
  void OnUpdateRequiredScreenExit();
  void OnOobeFlowFinished();
  void OnPackagedLicenseScreenExit(PackagedLicenseScreen::Result result);
  void OnFamilyLinkNoticeScreenExit(FamilyLinkNoticeScreen::Result result);
  void OnOnlineAuthenticationScreenExit(OnlineAuthenticationScreen::Result);
  void OnUserAllowlistCheckScreenExit(UserAllowlistCheckScreen::Result);
  void OnSignInFatalErrorScreenExit();
  void OnEduCoexistenceLoginScreenExit(
      EduCoexistenceLoginScreen::Result result);
  void OnParentalHandoffScreenExit(ParentalHandoffScreen::Result result);
  void OnOfflineLoginScreenExit(OfflineLoginScreen::Result result);
  void OnOsInstallScreenExit();
  void OnOsTrialScreenExit(OsTrialScreen::Result result);
  void OnConsolidatedConsentScreenExit(
      ConsolidatedConsentScreen::Result result);
  void OnGuestTosScreenExit(GuestTosScreen::Result result);
  void OnHWDataCollectionScreenExit(HWDataCollectionScreen::Result result);
  void OnSmartPrivacyProtectionScreenExit(
      SmartPrivacyProtectionScreen::Result result);
  void OnThemeSelectionScreenExit(ThemeSelectionScreen::Result result);
  void OnChoobeScreenExit(ChoobeScreen::Result result);
  void OnTouchpadScreenExit(TouchpadScrollScreen::Result result);
  void OnDisplaySizeScreenExit(DisplaySizeScreen::Result result);
  void OnDrivePinningScreenExit(DrivePinningScreen::Result result);
  void OnGaiaInfoScreenExit(GaiaInfoScreen::Result result);
  void OnAddChildScreenExit(AddChildScreen::Result result);
  void OnConsumerUpdateScreenExit(ConsumerUpdateScreen::Result result);
  void OnLocalPasswordSetupScreenExit(LocalPasswordSetupScreen::Result result);
  void OnApplyOnlinePasswordScreenExit(
      ApplyOnlinePasswordScreen::Result result);
  void OnOSAuthErrorScreenExit(OSAuthErrorScreen::Result result);
  void OnFactorSetupSuccessScreenExit(FactorSetupSuccessScreen::Result result);
  void OnCategoriesSelectionScreenExit(
      CategoriesSelectionScreen::Result result);
  void OnPersonalizedRecomendAppsScreenExit(
      PersonalizedRecommendAppsScreen::Result result);
  void OnPerksDiscoveryScreenExit(PerksDiscoveryScreen::Result result);
  void OnAppLaunchSplashScreenExit();

  // Callback invoked once it has been determined whether the device is disabled
  // or not.
  void OnDeviceDisabledChecked(bool device_disabled);
  void OnSplitModifierKeyboardInfoScreenExit(
      SplitModifierKeyboardInfoScreen::Result result);
  void OnAccountSelectionScreenExit(AccountSelectionScreen::Result result);

  // Shows update screen and starts update process.
  void InitiateOOBEUpdate();
  void StartOOBEUpdate();

  // Retrieve filtered OOBE configuration and apply relevant values.
  void UpdateOobeConfiguration();

  // Actions that should be done right after Network Screen and before
  // the update check.
  void PerformPostNetworkScreenActions();

  // Actions that should be done after OOBE flow is finished.
  // If this is called, future boots before the device is owned will start in
  // the first sign-in screen.
  void PerformOOBECompletedActions(
      OobeMetricsHelper::CompletedPreLoginOobeFlowType flow_type);

  ErrorScreen* GetErrorScreen();

  void OnHIDScreenNecessityCheck(bool screen_needed);

  // Notification of a change in the state of an accessibility setting.
  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details);

  // Switches from one screen to another.
  void SetCurrentScreen(BaseScreen* screen);

  // Update the status area visibility for `screen`.
  void UpdateStatusAreaVisibilityForScreen(OobeScreenId screen_id);

  // Launch the given `app` configured for Kiosk auto-launch.
  void AutoLaunchKioskApp(const KioskApp& app);

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
  // `prescribed_enrollment_config_`.
  void StartEnrollmentScreen();
  void ShowEnrollmentScreenIfEligible();

  void NotifyScreenChanged();

  // Tries to switch to the screen which was shown before the current screen.
  // Returns `true` if the screen switched.
  bool MaybeSetToPreviousScreen();

  // Returns auto enrollment controller (lazily initializes one if it doesn't
  // exist already).
  policy::AutoEnrollmentController* GetAutoEnrollmentController();

  // Requests owning TPM for branded builds with --tpm-is-dynamic switch unset.
  // When --tpm-is-dynamic switch is set, pre-enrollment TPM check relies on
  // the TPM being un-owned until enrollment. b/187429309
  void MaybeTakeTPMOwnership();

  // Hides the current screen if it's not set to `nullptr` and sets it to
  // `nullptr`.
  void ResetCurrentScreen();

  // Aborts Quick Start if the flow is ongoing.
  void MaybeAbortQuickStartFlow(
      quick_start::QuickStartController::AbortFlowReason reason);

  std::unique_ptr<policy::AutoEnrollmentController> auto_enrollment_controller_;
  std::unique_ptr<ChoobeFlowController> choobe_flow_controller_;
  std::unique_ptr<quick_start::QuickStartController> quickstart_controller_;
  std::unique_ptr<ScreenManager> screen_manager_;

  // The `BaseScreen*` here point to the objects owned by the `screen_manager_`.
  // So it should be safe to store the pointers.
  base::flat_map<BaseScreen*, raw_ptr<BaseScreen, CtnExperimental>>
      previous_screens_;

  raw_ptr<WizardContext> wizard_context_;

  static bool skip_enrollment_prompts_for_testing_;

  // Screen that's currently active.
  raw_ptr<BaseScreen, DanglingUntriaged> current_screen_ = nullptr;

  // True if full OOBE flow should be shown.
  bool is_out_of_box_ = false;

  // Value of the screen name that WizardController was started with.
  OobeScreenId first_screen_for_testing_ = ash::OOBE_SCREEN_UNKNOWN;

  // The prescribed enrollment configuration for the device.
  policy::EnrollmentConfig prescribed_enrollment_config_;

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
  friend class WizardControllerOnboardingResumeTest;
  friend class WizardControllerScreenPriorityTest;
  friend class WizardControllerManagementTransitionOobeTest;
  friend class WizardControllerRemoteActivityNotificationTest;

  base::CallbackListSubscription accessibility_subscription_;

  std::unique_ptr<TimeZoneProvider> timezone_provider_;

  // Controller of the demo mode setup. It has the lifetime of the single demo
  // mode setup flow.
  std::unique_ptr<DemoSetupController> demo_setup_controller_;

  // Tests check result of timezone resolve.
  bool timezone_resolved_ = false;
  base::OnceClosure on_timezone_resolved_for_testing_;

  bool is_initialized_ = false;

  base::ScopedObservation<OobeUI, OobeUI::Observer> oobe_ui_observation_{this};

  base::ObserverList<ScreenObserver> screen_observers_;

  // Shared factory for outgoing network requests.
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  base::WeakPtrFactory<WizardController> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_WIZARD_CONTROLLER_H_
