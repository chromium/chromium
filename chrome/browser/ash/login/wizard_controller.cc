// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/wizard_controller.h"

#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_types.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/customization/customization_document.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/login/choobe_flow_controller.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/enrollment/auto_enrollment_check_screen.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/hwid_checker.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/quickstart_controller.h"
// Make sure to include new screen to all relevant metric enums.
// LINT.IfChange(UsageMetrics)
#include "chrome/browser/ash/login/screens/account_selection_screen.h"
#include "chrome/browser/ash/login/screens/add_child_screen.h"
#include "chrome/browser/ash/login/screens/ai_intro_screen.h"
#include "chrome/browser/ash/login/screens/app_downloading_screen.h"
#include "chrome/browser/ash/login/screens/app_launch_splash_screen.h"
#include "chrome/browser/ash/login/screens/arc_vm_data_migration_screen.h"
#include "chrome/browser/ash/login/screens/assistant_optin_flow_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/categories_selection_screen.h"
#include "chrome/browser/ash/login/screens/choobe_screen.h"
#include "chrome/browser/ash/login/screens/consolidated_consent_screen.h"
#include "chrome/browser/ash/login/screens/consumer_update_screen.h"
#include "chrome/browser/ash/login/screens/demo_preferences_screen.h"
#include "chrome/browser/ash/login/screens/demo_setup_screen.h"
#include "chrome/browser/ash/login/screens/device_disabled_screen.h"
#include "chrome/browser/ash/login/screens/display_size_screen.h"
#include "chrome/browser/ash/login/screens/drive_pinning_screen.h"
#include "chrome/browser/ash/login/screens/edu_coexistence_login_screen.h"
#include "chrome/browser/ash/login/screens/enable_adb_sideloading_screen.h"
#include "chrome/browser/ash/login/screens/enable_debugging_screen.h"
#include "chrome/browser/ash/login/screens/encryption_migration_screen.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/screens/family_link_notice_screen.h"
#include "chrome/browser/ash/login/screens/fingerprint_setup_screen.h"
#include "chrome/browser/ash/login/screens/gaia_info_screen.h"
#include "chrome/browser/ash/login/screens/gaia_screen.h"
#include "chrome/browser/ash/login/screens/gemini_intro_screen.h"
#include "chrome/browser/ash/login/screens/gesture_navigation_screen.h"
#include "chrome/browser/ash/login/screens/hardware_data_collection_screen.h"
#include "chrome/browser/ash/login/screens/hid_detection_screen.h"
#include "chrome/browser/ash/login/screens/install_attributes_error_screen.h"
#include "chrome/browser/ash/login/screens/local_state_error_screen.h"
#include "chrome/browser/ash/login/screens/locale_switch_screen.h"
#include "chrome/browser/ash/login/screens/management_transition_screen.h"
#include "chrome/browser/ash/login/screens/marketing_opt_in_screen.h"
#include "chrome/browser/ash/login/screens/multidevice_setup_screen.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ash/login/screens/network_screen.h"
#include "chrome/browser/ash/login/screens/offline_login_screen.h"
#include "chrome/browser/ash/login/screens/online_authentication_screen.h"
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
#include "chrome/browser/ash/login/screens/perks_discovery_screen.h"
#include "chrome/browser/ash/login/screens/personalized_recommend_apps_screen.h"
#include "chrome/browser/ash/login/screens/pin_setup_screen.h"
#include "chrome/browser/ash/login/screens/quick_start_screen.h"
#include "chrome/browser/ash/login/screens/recommend_apps_screen.h"
#include "chrome/browser/ash/login/screens/remote_activity_notification_screen.h"
#include "chrome/browser/ash/login/screens/reset_screen.h"
#include "chrome/browser/ash/login/screens/saml_confirm_password_screen.h"
#include "chrome/browser/ash/login/screens/signin_fatal_error_screen.h"
#include "chrome/browser/ash/login/screens/smart_privacy_protection_screen.h"
#include "chrome/browser/ash/login/screens/split_modifier_keyboard_info_screen.h"
#include "chrome/browser/ash/login/screens/sync_consent_screen.h"
#include "chrome/browser/ash/login/screens/theme_selection_screen.h"
#include "chrome/browser/ash/login/screens/touchpad_scroll_screen.h"
#include "chrome/browser/ash/login/screens/tpm_error_screen.h"
#include "chrome/browser/ash/login/screens/update_required_screen.h"
#include "chrome/browser/ash/login/screens/update_screen.h"
#include "chrome/browser/ash/login/screens/user_allowlist_check_screen.h"
#include "chrome/browser/ash/login/screens/user_creation_screen.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ash/login/screens/wrong_hwid_screen.h"
// Add new screens before this block. Add screens and exit reasons to
// OOBE histograms.
// LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/ash/net/delay_network_call.h"
#include "chrome/browser/ash/net/rollback_network_config/rollback_network_config_service.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_controller.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/ash/system/device_disabling_manager.h"
#include "chrome/browser/ash/system/timezone_resolver_manager.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/system/system_tray_client_impl.h"
#include "chrome/browser/ui/webui/ash/login/account_selection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/add_child_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/ai_intro_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/app_downloading_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/arc_vm_data_migration_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/assistant_optin_flow_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/auto_enrollment_check_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/categories_selection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/choobe_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/consolidated_consent_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/consumer_update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/demo_preferences_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/demo_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/device_disabled_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/display_size_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/drive_pinning_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/enable_adb_sideloading_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/encryption_migration_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/enrollment_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/enter_old_password_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/family_link_notice_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/fingerprint_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_info_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gemini_intro_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gesture_navigation_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/guest_tos_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/hardware_data_collection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/hid_detection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/install_attributes_error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/local_password_setup_handler.h"
#include "chrome/browser/ui/webui/ash/login/local_state_error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/locale_switch_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/management_transition_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/marketing_opt_in_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/multidevice_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/offline_login_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/online_authentication_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/login/os_install_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/os_trial_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/osauth/apply_online_password_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/osauth/factor_setup_success_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/osauth/local_data_loss_warning_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/osauth/osauth_error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/packaged_license_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/parental_handoff_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/password_selection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/perks_discovery_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/personalized_recommend_apps_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/pin_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/recommend_apps_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/recovery_eligibility_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/remote_activity_notification_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/reset_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/saml_confirm_password_handler.h"
#include "chrome/browser/ui/webui/ash/login/signin_fatal_error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/smart_privacy_protection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/split_modifier_keyboard_info_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/sync_consent_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/theme_selection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/touchpad_scroll_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/tpm_error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/update_required_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_allowlist_check_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/wrong_hwid_screen_handler.h"
#include "chrome/browser/ui/webui/help/help_utils_chromeos.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/language_packs/language_pack_manager.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "chromeos/ash/components/timezone/timezone_provider.h"
#include "chromeos/ash/components/timezone/timezone_request.h"
#include "chromeos/ash/services/cros_healthd/private/cpp/dlc_utils.h"
#include "chromeos/ash/services/rollback_network_config/public/mojom/rollback_network_config.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "remoting/host/chromeos/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/accelerators/accelerator.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {

namespace {

using ::content::BrowserThread;

bool g_using_zero_delays = false;

// Total timezone resolving process timeout.
const unsigned int kResolveTimeZoneTimeoutSeconds = 60;

constexpr const char kDefaultExitReason[] = "Next";
constexpr const char kResetScreenExitReason[] = "Cancel";

// TODO(crbug.com/40738890) Remove after stepping stone is set after M87.
constexpr char kLegacyUpdateScreenName[] = "update";

// Stores the list of all screens that should be shown when resuming OOBE.
const StaticOobeScreenId kResumableOobeScreens[] = {
    WelcomeView::kScreenId,
    NetworkScreenView::kScreenId,
    UpdateView::kScreenId,
    EnrollmentScreenView::kScreenId,
    AutoEnrollmentCheckScreenView::kScreenId,
    UserCreationView::kScreenId,
    AddChildScreenView::kScreenId,
    ConsumerUpdateScreenView::kScreenId,
};

const StaticOobeScreenId kResumablePostLoginScreens[] = {
    TermsOfServiceScreenView::kScreenId,
    SyncConsentScreenView::kScreenId,
    HWDataCollectionView::kScreenId,
    FingerprintSetupScreenView::kScreenId,
    GestureNavigationScreenView::kScreenId,
    RecommendAppsScreenView::kScreenId,
    AiIntroScreenView::kScreenId,
    GeminiIntroScreenView::kScreenId,
    PinSetupScreenView::kScreenId,
    MarketingOptInScreenView::kScreenId,
    MultiDeviceSetupScreenView::kScreenId,
    ConsolidatedConsentScreenView::kScreenId,
    ThemeSelectionScreenView::kScreenId,
    ChoobeScreenView::kScreenId,
    DisplaySizeScreenView::kScreenId,
    TouchpadScrollScreenView::kScreenId,
    CategoriesSelectionScreenView::kScreenId,
    PerksDiscoveryScreenView::kScreenId,
    SplitModifierKeyboardInfoScreenView::kScreenId,
};

const StaticOobeScreenId kScreensWithHiddenStatusArea[] = {
    EnableAdbSideloadingScreenView::kScreenId,
    EnableDebuggingScreenView::kScreenId,
    ManagementTransitionScreenView::kScreenId,
    TpmErrorView::kScreenId,
    InstallAttributesErrorView::kScreenId,
    WrongHWIDScreenView::kScreenId,
    LocalStateErrorScreenView::kScreenId,
};

std::string GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

bool IsResumableOobeScreen(OobeScreenId screen_id) {
  for (const auto& resumable_screen : kResumableOobeScreens) {
    if (screen_id == resumable_screen) {
      return true;
    }
  }
  return false;
}

bool ShouldHideStatusArea(OobeScreenId screen_id) {
  for (const auto& s : kScreensWithHiddenStatusArea) {
    if (screen_id == s) {
      return true;
    }
  }
  return false;
}

LoginDisplayHost* GetLoginDisplayHost() {
  return LoginDisplayHost::default_host();
}

OobeUI* GetOobeUI() {
  auto* host = LoginDisplayHost::default_host();
  return host ? host->GetOobeUI() : nullptr;
}

OobeScreenId PrefToScreenId(const std::string& pref_value) {
  if (pref_value == kLegacyUpdateScreenName) {
    return UpdateView::kScreenId;
  }
  return OobeScreenId(pref_value);
}

bool IsGaiaPageDefaultsToSAML() {
  int authentication_behavior = 0;
  bool authentication_behavior_set = CrosSettings::Get()->GetInteger(
      kLoginAuthenticationBehavior, &authentication_behavior);
  return authentication_behavior_set && authentication_behavior;
}

bool IsContextNeededForScreen(OobeScreenId screen_id) {
  return screen_id == SamlConfirmPasswordView::kScreenId ||
         screen_id == CryptohomeRecoveryScreenView::kScreenId ||
         screen_id == LocalDataLossWarningScreenView::kScreenId;
}

std::string SetupTypeToString(WizardContext::AuthChangeFlow flow_type) {
  switch (flow_type) {
    case WizardContext::AuthChangeFlow::kInitialSetup:
      return "Initial Setup";
    case WizardContext::AuthChangeFlow::kReauthentication:
      return "ReAuth";
    case WizardContext::AuthChangeFlow::kRecovery:
      return "Recovery";
  }
}

}  // namespace

// static
const int WizardController::kMinAudibleOutputVolumePercent = 10;

// static
bool WizardController::skip_enrollment_prompts_for_testing_ = false;

// static
WizardController* WizardController::default_controller() {
  auto* host = LoginDisplayHost::default_host();
  return (host && host->IsWizardControllerCreated())
             ? host->GetWizardController()
             : nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, public:

PrefService* WizardController::local_state_for_testing_ = nullptr;

WizardController::WizardController(WizardContext* wizard_context)
    : quickstart_controller_(
          std::make_unique<quick_start::QuickStartController>()),
      screen_manager_(std::make_unique<ScreenManager>()),
      wizard_context_(wizard_context),
      shared_url_loader_factory_(
          g_browser_process->shared_url_loader_factory()) {
  wizard_context_->skip_post_login_screens_for_tests =
      switches::ShouldSkipOobePostLogin();
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  if (accessibility_manager) {
    // accessibility_manager could be null in Tests.
    accessibility_subscription_ = accessibility_manager->RegisterCallback(
        base::BindRepeating(&WizardController::OnAccessibilityStatusChanged,
                            weak_factory_.GetWeakPtr()));
  }
  if (GetOobeUI()) {
    // could be null in unit tests.
    screen_manager_->Init(CreateScreens());
    // If the frontend element is constructed and OobeScreensHandlerFactory is
    // created with a pending receiver, bind the receiver when the
    // WizardController is created.
    if (GetOobeUI()->GetOobeScreensHandlerFactory()) {
      GetOobeUI()->GetOobeScreensHandlerFactory()->BindScreensHandlerFactory();
    }
    // OOBE UI can be recreated in case of CrossOriginOpenerPolicyByDefault.
    // TODO(crbug.com/40138102): Remove this logic after WebUI split is done,
    // as screens should work with late binding/early unbinding in that case.
    oobe_ui_observation_.Observe(GetOobeUI());
  }
}

WizardController::~WizardController() {
  for (ScreenObserver& obs : screen_observers_) {
    obs.OnShutdown();
  }

  if (GetOobeUI() && GetOobeUI()->GetOobeScreensHandlerFactory()) {
      GetOobeUI()
        ->GetOobeScreensHandlerFactory()
        ->UnbindScreensHandlerFactory();
  }

  previous_screens_.clear();
  screen_manager_.reset();
}

void WizardController::Init(OobeScreenId first_screen) {
  DCHECK(!is_initialized());
  is_initialized_ = true;

  prescribed_enrollment_config_ =
      policy::EnrollmentConfig::GetPrescribedEnrollmentConfig();

  VLOG(1) << "Starting OOBE wizard with screen: " << first_screen;

  bool oobe_complete = StartupUtils::IsOobeCompleted();
  if (!oobe_complete) {
    UpdateOobeConfiguration();
    is_out_of_box_ = true;
  }

  wizard_context_->is_add_person_flow =
      oobe_complete && StartupUtils::IsDeviceOwned();

  // This is a hacky way to check for local state corruption, because
  // it depends on the fact that the local state is loaded
  // synchronously and at the first demand.
  // ash::InstallAttributes::IsEnterpriseManaged() check is required
  // because currently powerwash is disabled for enterprise-enrolled devices.
  //
  // TODO (ygorshenin@): implement handling of the local state
  // corruption in the case of asynchronous loading.
  bool is_enterprise_managed =
      ash::InstallAttributes::Get()->IsEnterpriseManaged();
  if (!is_enterprise_managed) {
    const PrefService::PrefInitializationStatus status =
        GetLocalState()->GetInitializationStatus();
    if (status == PrefService::INITIALIZATION_STATUS_ERROR) {
      OnLocalStateInitialized(false);
      return;
    }
    if (status == PrefService::INITIALIZATION_STATUS_WAITING) {
      GetLocalState()->AddPrefInitObserver(
          base::BindOnce(&WizardController::OnLocalStateInitialized,
                         weak_factory_.GetWeakPtr()));
    }
  }

  if (!oobe_complete) {
    bool updated =
        GetLocalState()->GetBoolean(prefs::kOobeConsumerUpdateCompleted) ||
        GetLocalState()->GetBoolean(prefs::kOobeCriticalUpdateCompleted);
    if (!updated) {
      GetLoginDisplayHost()->GetOobeMetricsHelper()->RecordChromeVersion();
    }
  }

  const bool device_is_owned =
      is_enterprise_managed ||
      !user_manager::UserManager::Get()->GetUsers().empty();
  // Do not show the HID Detection screen if device is owned.
  if (!device_is_owned && HIDDetectionScreen::CanShowScreen() &&
      first_screen == ash::OOBE_SCREEN_UNKNOWN) {
    // TODO(https://crbug.com/1275960): Move logic into
    // HIDDetectionScreen::MaybeSkip.
    GetScreen<HIDDetectionScreen>()->CheckIsScreenRequired(
        base::BindOnce(&WizardController::OnHIDScreenNecessityCheck,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  AdvanceToScreenAfterHIDDetection(first_screen);
}

void WizardController::OnDestroyingOobeUI() {
  previous_screens_.clear();

  // Reset `current_screen_` to prevent its usage after OobeUI is gone.
  ResetCurrentScreen();

  // Reset screens, they should not access handlers anymore.
  // TODO(https://crbug.com/1309022): This should probably be removed when all
  // the screen/handlers migrated to the new patterns.
  screen_manager_->Shutdown();
  oobe_ui_observation_.Reset();
}

void WizardController::HideCurrentScreen() {
  SetCurrentScreen(nullptr);
}

void WizardController::ContinueOobeFlow() {
  // Use the saved screen preference from Local State if exist.
  const std::string screen_pref =
      GetLocalState()->GetString(prefs::kOobeScreenPending);
  const OobeScreenId screen_id = PrefToScreenId(screen_pref);
  GetLoginDisplayHost()->GetOobeMetricsHelper()->RecordPreLoginOobeResume(
      screen_id);

  if (!screen_pref.empty() && HasScreen(screen_id)) {
    AdvanceToScreen(screen_id);
  } else {
    ShowPackagedLicenseScreen();
  }
}

void WizardController::AdvanceToScreenAfterHIDDetection(
    OobeScreenId first_screen) {
  OobeScreenId actual_first_screen = first_screen;
  if (actual_first_screen == ash::OOBE_SCREEN_UNKNOWN) {
    if (!is_out_of_box_) {
      DeviceSettingsService::Get()->GetOwnershipStatusAsync(
          base::BindOnce(&WizardController::OnOwnershipStatusCheckDone,
                         weak_factory_.GetWeakPtr()));
      return;
    }

    // Use the saved screen preference from Local State.
    const std::string screen_pref =
        GetLocalState()->GetString(prefs::kOobeScreenPending);
    if (!screen_pref.empty() && HasScreen(PrefToScreenId(screen_pref))) {
      actual_first_screen = PrefToScreenId(screen_pref);
    } else {
      actual_first_screen = WelcomeView::kScreenId;
    }
  }

  first_screen_for_testing_ = actual_first_screen;

  if (!IsMachineHWIDCorrect() && !StartupUtils::IsDeviceRegistered() &&
      first_screen == ash::OOBE_SCREEN_UNKNOWN) {
    ShowWrongHWIDScreen();
  } else {
    AdvanceToScreen(actual_first_screen);
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOobeSkipToLogin)) {
    SkipToLoginForTesting();
  }
}

ErrorScreen* WizardController::GetErrorScreen() {
  return GetOobeUI()->GetErrorScreen();
}

bool WizardController::HasScreen(OobeScreenId screen_id) {
  return screen_manager_->HasScreen(screen_id);
}

// static
bool WizardController::IsErrorScreen(OobeScreenId screen_id) {
  return screen_id == ErrorScreenView::kScreenId;
}

BaseScreen* WizardController::GetScreen(OobeScreenId screen_id) {
  if (WizardController::IsErrorScreen(screen_id)) {
    return GetErrorScreen();
  }
  return screen_manager_->GetScreen(screen_id);
}

OobeScreenId WizardController::GetScreenByName(const std::string& screen_name) {
  if (screen_name == ErrorScreenView::kScreenId.name) {
    return ErrorScreenView::kScreenId;
  }
  return screen_manager_->GetScreenByName(screen_name);
}

void WizardController::SetCurrentScreenForTesting(BaseScreen* screen) {
  current_screen_ = screen;
}

void WizardController::SetSharedURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> factory) {
  shared_url_loader_factory_ = factory;
}

std::vector<std::pair<OobeScreenId, std::unique_ptr<BaseScreen>>>
WizardController::CreateScreens() {
  OobeUI* oobe_ui = GetOobeUI();

  std::vector<std::pair<OobeScreenId, std::unique_ptr<BaseScreen>>> result;

  auto append = [&](std::unique_ptr<BaseScreen> screen) {
    result.emplace_back(screen->screen_id(), std::move(screen));
  };

  if (oobe_ui->display_type() == OobeUI::kOobeDisplay) {
    append(std::make_unique<WelcomeScreen>(
        oobe_ui->GetView<WelcomeScreenHandler>()->AsWeakPtr(),
        base::BindRepeating(&WizardController::OnWelcomeScreenExit,
                            weak_factory_.GetWeakPtr())));

    append(std::make_unique<DemoPreferencesScreen>(
        oobe_ui->GetView<DemoPreferencesScreenHandler>()->AsWeakPtr(),
        base::BindRepeating(&WizardController::OnDemoPreferencesScreenExit,
                            weak_factory_.GetWeakPtr())));
  }

  append(std::make_unique<QuickStartScreen>(
      oobe_ui->GetView<QuickStartScreenHandler>()->AsWeakPtr(),
      quick_start_controller(),
      base::BindRepeating(&WizardController::OnQuickStartScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<NetworkScreen>(
      oobe_ui->GetView<NetworkScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnNetworkScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<UpdateScreen>(
      oobe_ui->GetView<UpdateScreenHandler>()->AsWeakPtr(),
      oobe_ui->GetErrorScreen(),
      base::BindRepeating(&WizardController::OnUpdateScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<EnrollmentScreen>(
      oobe_ui->GetView<EnrollmentScreenHandler>()->AsWeakPtr(),
      oobe_ui->GetErrorScreen(),
      base::BindRepeating(&WizardController::OnEnrollmentScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<ResetScreen>(
      oobe_ui->GetView<ResetScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnResetScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<DemoSetupScreen>(
      oobe_ui->GetView<DemoSetupScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnDemoSetupScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<EnableAdbSideloadingScreen>(
      oobe_ui->GetView<EnableAdbSideloadingScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnEnableAdbSideloadingScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<EnableDebuggingScreen>(
      oobe_ui->GetView<EnableDebuggingScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnEnableDebuggingScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<LocaleSwitchScreen>(
      oobe_ui->GetView<LocaleSwitchScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnLocaleSwitchScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<RecoveryEligibilityScreen>(
      base::BindRepeating(&WizardController::OnRecoveryEligibilityScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<CryptohomeRecoverySetupScreen>(
      oobe_ui->GetView<CryptohomeRecoverySetupScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(
          &WizardController::OnCryptohomeRecoverySetupScreenExit,
          weak_factory_.GetWeakPtr())));
  append(std::make_unique<TermsOfServiceScreen>(
      oobe_ui->GetView<TermsOfServiceScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnTermsOfServiceScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<SyncConsentScreen>(
      oobe_ui->GetView<SyncConsentScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnSyncConsentScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<RecommendAppsScreen>(
      oobe_ui->GetView<RecommendAppsScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnRecommendAppsScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<AppDownloadingScreen>(
      oobe_ui->GetView<AppDownloadingScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnAppDownloadingScreenExit,
                          weak_factory_.GetWeakPtr())));

  if (features::IsOobeAiIntroEnabled()) {
    append(std::make_unique<AiIntroScreen>(
        oobe_ui->GetView<AiIntroScreenHandler>()->AsWeakPtr(),
        base::BindRepeating(&WizardController::OnAiIntroScreenExit,
                            weak_factory_.GetWeakPtr())));
  }

  if (features::IsOobeGeminiIntroEnabled()) {
    append(std::make_unique<GeminiIntroScreen>(
        oobe_ui->GetView<GeminiIntroScreenHandler>()->AsWeakPtr(),
        base::BindRepeating(&WizardController::OnGeminiIntroScreenExit,
                            weak_factory_.GetWeakPtr())));
  }

  append(std::make_unique<WrongHWIDScreen>(
      oobe_ui->GetView<WrongHWIDScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnWrongHWIDScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<LocalStateErrorScreen>(
      oobe_ui->GetView<LocalStateErrorScreenHandler>()->AsWeakPtr()));

  if (base::FeatureList::IsEnabled(arc::kEnableArcVmDataMigration)) {
    append(std::make_unique<ArcVmDataMigrationScreen>(
        oobe_ui->GetView<ArcVmDataMigrationScreenHandler>()->AsWeakPtr()));
  }

  if (HIDDetectionScreen::CanShowScreen()) {
    append(std::make_unique<HIDDetectionScreen>(
        oobe_ui->GetView<HIDDetectionScreenHandler>()->AsWeakPtr(),
        base::BindRepeating(&WizardController::OnHidDetectionScreenExit,
                            weak_factory_.GetWeakPtr())));
  }

  append(std::make_unique<AutoEnrollmentCheckScreen>(
      oobe_ui->GetView<AutoEnrollmentCheckScreenHandler>()->AsWeakPtr(),
      oobe_ui->GetErrorScreen(),
      base::BindRepeating(&WizardController::OnAutoEnrollmentCheckScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<DeviceDisabledScreen>(
      oobe_ui->GetView<DeviceDisabledScreenHandler>()->AsWeakPtr()));
  append(std::make_unique<EncryptionMigrationScreen>(
      oobe_ui->GetView<EncryptionMigrationScreenHandler>()->AsWeakPtr()));
  append(std::make_unique<ManagementTransitionScreen>(
      oobe_ui->GetView<ManagementTransitionScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnManagementTransitionScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<UpdateRequiredScreen>(
      oobe_ui->GetView<UpdateRequiredScreenHandler>()->AsWeakPtr(),
      oobe_ui->GetErrorScreen(),
      base::BindRepeating(&WizardController::OnUpdateRequiredScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<AssistantOptInFlowScreen>(
      oobe_ui->GetView<AssistantOptInFlowScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnAssistantOptInFlowScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<MultiDeviceSetupScreen>(
      oobe_ui->GetView<MultiDeviceSetupScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnMultiDeviceSetupScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<PinSetupScreen>(
      oobe_ui->GetView<PinSetupScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnPinSetupScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<FingerprintSetupScreen>(
      oobe_ui->GetView<FingerprintSetupScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnFingerprintSetupScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<GestureNavigationScreen>(
      oobe_ui->GetView<GestureNavigationScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnGestureNavigationScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<MarketingOptInScreen>(
      oobe_ui->GetView<MarketingOptInScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnMarketingOptInScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<PackagedLicenseScreen>(
      oobe_ui->GetView<PackagedLicenseScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnPackagedLicenseScreenExit,
                          weak_factory_.GetWeakPtr())));

  if (features::IsOobeGaiaInfoScreenEnabled()) {
    append(std::make_unique<GaiaInfoScreen>(
        oobe_ui->GetView<GaiaInfoScreenHandler>()->AsWeakPtr(),
        base::BindRepeating(&WizardController::OnGaiaInfoScreenExit,
                            weak_factory_.GetWeakPtr())));
  }

  append(std::make_unique<GaiaScreen>(
      oobe_ui->GetView<GaiaScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnGaiaScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<OnlineAuthenticationScreen>(
      oobe_ui->GetView<OnlineAuthenticationScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnOnlineAuthenticationScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<UserAllowlistCheckScreen>(
      oobe_ui->GetView<UserAllowlistCheckScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnUserAllowlistCheckScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<SamlConfirmPasswordScreen>(
      oobe_ui->GetView<SamlConfirmPasswordHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnSamlConfirmPasswordScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<OfflineLoginScreen>(
      oobe_ui->GetView<OfflineLoginScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnOfflineLoginScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<TpmErrorScreen>(
      oobe_ui->GetView<TpmErrorScreenHandler>()->AsWeakPtr()));

  append(std::make_unique<InstallAttributesErrorScreen>(
      oobe_ui->GetView<InstallAttributesErrorScreenHandler>()->AsWeakPtr()));

  append(std::make_unique<FamilyLinkNoticeScreen>(
      oobe_ui->GetView<FamilyLinkNoticeScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnFamilyLinkNoticeScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<UserCreationScreen>(
      oobe_ui->GetView<UserCreationScreenHandler>()->AsWeakPtr(),
      oobe_ui->GetErrorScreen(),
      base::BindRepeating(&WizardController::OnUserCreationScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<AddChildScreen>(
      oobe_ui->GetView<AddChildScreenHandler>()->AsWeakPtr(),
      oobe_ui->GetErrorScreen(),
      base::BindRepeating(&WizardController::OnAddChildScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<EduCoexistenceLoginScreen>(
      base::BindRepeating(&WizardController::OnEduCoexistenceLoginScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<SignInFatalErrorScreen>(
      oobe_ui->GetView<SignInFatalErrorScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnSignInFatalErrorScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<ParentalHandoffScreen>(
      oobe_ui->GetView<ParentalHandoffScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnParentalHandoffScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<ConsolidatedConsentScreen>(
      oobe_ui->GetView<ConsolidatedConsentScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnConsolidatedConsentScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<GuestTosScreen>(
      oobe_ui->GetView<GuestTosScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnGuestTosScreenExit,
                          weak_factory_.GetWeakPtr())));

  if (switches::IsOsInstallAllowed()) {
    append(std::make_unique<OsInstallScreen>(
        oobe_ui->GetView<OsInstallScreenHandler>()->AsWeakPtr(),
        base::BindRepeating(&WizardController::OnOsInstallScreenExit,
                            weak_factory_.GetWeakPtr())));
    append(std::make_unique<OsTrialScreen>(
        oobe_ui->GetView<OsTrialScreenHandler>()->AsWeakPtr(),
        base::BindRepeating(&WizardController::OnOsTrialScreenExit,
                            weak_factory_.GetWeakPtr())));
  }

  if (switches::IsRevenBranding()) {
    append(std::make_unique<HWDataCollectionScreen>(
        oobe_ui->GetView<HWDataCollectionScreenHandler>()->AsWeakPtr(),
        base::BindRepeating(&WizardController::OnHWDataCollectionScreenExit,
                            weak_factory_.GetWeakPtr())));
  }

  append(std::make_unique<SmartPrivacyProtectionScreen>(
      oobe_ui->GetView<SmartPrivacyProtectionScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnSmartPrivacyProtectionScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<ThemeSelectionScreen>(
      oobe_ui->GetView<ThemeSelectionScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnThemeSelectionScreenExit,
                          weak_factory_.GetWeakPtr())));

  if (base::FeatureList::IsEnabled(
          remoting::features::kEnableCrdAdminRemoteAccessV2)) {
    append(std::make_unique<RemoteActivityNotificationScreen>(
        oobe_ui->GetView<RemoteActivityNotificationScreenHandler>()
            ->AsWeakPtr(),
        base::BindRepeating(
            &WizardController::OnRemoteActivityNotificationScreenExit,
            weak_factory_.GetWeakPtr())));
  }

  append(std::make_unique<CryptohomeRecoveryScreen>(
      oobe_ui->GetView<CryptohomeRecoveryScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnCryptohomeRecoveryScreenExit,
                          weak_factory_.GetWeakPtr())));

  if (features::IsOobeChoobeEnabled()) {
    append(std::make_unique<ChoobeScreen>(
        oobe_ui->GetView<ChoobeScreenHandler>()->AsWeakPtr(),
        base::BindRepeating(&WizardController::OnChoobeScreenExit,
                            weak_factory_.GetWeakPtr())));
  }

  if (features::IsOobeTouchpadScrollEnabled()) {
    append(std::make_unique<TouchpadScrollScreen>(
        oobe_ui->GetView<TouchpadScrollScreenHandler>()->AsWeakPtr(),
        base::BindRepeating(&WizardController::OnTouchpadScreenExit,
                            weak_factory_.GetWeakPtr())));
  }
  if (features::IsOobeDisplaySizeEnabled()) {
    append(std::make_unique<DisplaySizeScreen>(
        oobe_ui->GetView<DisplaySizeScreenHandler>()->AsWeakPtr(),
        base::BindRepeating(&WizardController::OnDisplaySizeScreenExit,
                            weak_factory_.GetWeakPtr())));
  }

  if (drive::util::IsOobeDrivePinningScreenEnabled()) {
    append(std::make_unique<DrivePinningScreen>(
        oobe_ui->GetView<DrivePinningScreenHandler>()->AsWeakPtr(),
        base::BindRepeating(&WizardController::OnDrivePinningScreenExit,
                            weak_factory_.GetWeakPtr())));
  }

  if (features::IsOobeSoftwareUpdateEnabled()) {
    append(std::make_unique<ConsumerUpdateScreen>(
        oobe_ui->GetView<ConsumerUpdateScreenHandler>()->AsWeakPtr(),
        oobe_ui->GetErrorScreen(),
        base::BindRepeating(&WizardController::OnConsumerUpdateScreenExit,
                            weak_factory_.GetWeakPtr())));
  }

  // We don't guard creation of those screens via feature flag to prevent cases
  // when we didn't apply Finch config, but user restarted their device
  // and our screen being marked as resumable is expected to be shown, but Finch
  // decided that feature should be turned off.
  // Instead, we will check for feature flag inside each screen's `MaybeSkip()`.
  append(std::make_unique<CategoriesSelectionScreen>(
      oobe_ui->GetView<CategoriesSelectionScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnCategoriesSelectionScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<PersonalizedRecommendAppsScreen>(
      oobe_ui->GetView<PersonalizedRecommendAppsScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(
          &WizardController::OnPersonalizedRecomendAppsScreenExit,
          weak_factory_.GetWeakPtr())));

  append(std::make_unique<SplitModifierKeyboardInfoScreen>(
      oobe_ui->GetView<SplitModifierKeyboardInfoScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(
          &WizardController::OnSplitModifierKeyboardInfoScreenExit,
          weak_factory_.GetWeakPtr())));

  append(std::make_unique<PasswordSelectionScreen>(
      oobe_ui->GetView<PasswordSelectionScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnPasswordSelectionScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<ApplyOnlinePasswordScreen>(
      oobe_ui->GetView<ApplyOnlinePasswordScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnApplyOnlinePasswordScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<LocalPasswordSetupScreen>(
      oobe_ui->GetView<LocalPasswordSetupHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnLocalPasswordSetupScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<LocalDataLossWarningScreen>(
      oobe_ui->GetView<LocalDataLossWarningScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnLocalDataLossWarningScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<EnterOldPasswordScreen>(
      oobe_ui->GetView<EnterOldPasswordScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnEnterOldPasswordScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<PerksDiscoveryScreen>(
      oobe_ui->GetView<PerksDiscoveryScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnPerksDiscoveryScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<OSAuthErrorScreen>(
      oobe_ui->GetView<OSAuthErrorScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnOSAuthErrorScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<FactorSetupSuccessScreen>(
      oobe_ui->GetView<FactorSetupSuccessScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnFactorSetupSuccessScreenExit,
                          weak_factory_.GetWeakPtr())));

  if (features::IsOobeAddUserDuringEnrollmentEnabled()) {
    append(std::make_unique<AccountSelectionScreen>(
        oobe_ui->GetView<AccountSelectionScreenHandler>()->AsWeakPtr(),
        base::BindRepeating(&WizardController::OnAccountSelectionScreenExit,
                            weak_factory_.GetWeakPtr())));
  }

  append(std::make_unique<AppLaunchSplashScreen>(
      oobe_ui->GetView<AppLaunchSplashScreenHandler>()->AsWeakPtr(),
      oobe_ui->GetErrorScreen(),
      base::BindRepeating(&WizardController::OnAppLaunchSplashScreenExit,
                          weak_factory_.GetWeakPtr())));

  return result;
}

void WizardController::ShowWelcomeScreen() {
  SetCurrentScreen(GetScreen(WelcomeView::kScreenId));
}

void WizardController::ShowQuickStartScreen() {
  CHECK(wizard_context_->quick_start_enabled);
  SetCurrentScreen(GetScreen(QuickStartView::kScreenId));
}

void WizardController::ShowNetworkScreen() {
  SetCurrentScreen(GetScreen(NetworkScreenView::kScreenId));
}

void WizardController::OnOwnershipStatusCheckDone(
    DeviceSettingsService::OwnershipStatus status) {
  if (status == DeviceSettingsService::OwnershipStatus::kOwnershipNone) {
    ContinueOobeFlow();
  } else {
    ShowLoginScreen();
  }
}

void WizardController::ShowSignInFatalErrorScreen(
    SignInFatalErrorScreen::Error error,
    base::Value::Dict params) {
  GetScreen<SignInFatalErrorScreen>()->SetErrorState(error, std::move(params));
  AdvanceToScreen(SignInFatalErrorView::kScreenId);
}

void WizardController::OnSignInFatalErrorScreenExit() {
  OnScreenExit(SignInFatalErrorView::kScreenId, kDefaultExitReason);
  if (base::Contains(previous_screens_, current_screen_) &&
      IsContextNeededForScreen(
          previous_screens_[current_screen_]->screen_id())) {
    // If the last screen user have visited before reaching SignInFatalError
    // screen was a screen that needs user context, we should not go back there
    // because the context is lost at this point. We should go to the Gaia
    // screen instead.
    previous_screens_[current_screen_] = GetScreen(GaiaView::kScreenId);
  }

  // It's possible to get on the SignInFatalError screen both from the user pods
  // and from the Gaia sign-in screen. The screen exits when user presses
  // "try again". Go to the previous screen if it is set. Otherwise go to the
  // login screen with pods.
  if (MaybeSetToPreviousScreen()) {
    return;
  }
  ShowLoginScreen();
}

void WizardController::ShowLoginScreen() {
  VLOG(1) << "Showing login screen.";
  UpdateStatusAreaVisibilityForScreen(GaiaView::kScreenId);
  GetLoginDisplayHost()->StartSignInScreen();
}

void WizardController::ShowGaiaInfoScreen() {
  SetCurrentScreen(GetScreen(GaiaInfoScreenView::kScreenId));
}

void WizardController::ShowAddChildScreen() {
  SetCurrentScreen(GetScreen(AddChildScreenView::kScreenId));
}

void WizardController::ShowConsumerUpdateScreen() {
  SetCurrentScreen(GetScreen(ConsumerUpdateScreenView::kScreenId));
}

void WizardController::ShowLocalPasswordSetupScreen() {
  SetCurrentScreen(GetScreen(LocalPasswordSetupView::kScreenId));
}

void WizardController::ShowApplyOnlinePasswordScreen() {
  SetCurrentScreen(GetScreen(ApplyOnlinePasswordScreenView::kScreenId));
}

void WizardController::ShowLocalDataLossWarningScreen() {
  SetCurrentScreen(GetScreen(LocalDataLossWarningScreenView::kScreenId));
}

void WizardController::ShowEnterOldPasswordScreen() {
  SetCurrentScreen(GetScreen(EnterOldPasswordScreenView::kScreenId));
}

void WizardController::ShowEnrollmentScreen() {
  MaybeAbortQuickStartFlow(quick_start::QuickStartController::AbortFlowReason::
                               ENTERPRISE_ENROLLMENT);

  // Update the enrollment configuration and start the screen.
  GetLoginDisplayHost()->GetOobeMetricsHelper()->RecordEnrollingUserType();
  prescribed_enrollment_config_ =
      policy::EnrollmentConfig::GetPrescribedEnrollmentConfig();
  StartEnrollmentScreen();
}

void WizardController::ShowDemoModePreferencesScreen() {
  SetCurrentScreen(GetScreen(DemoPreferencesScreenView::kScreenId));
}

void WizardController::ShowDemoModeSetupScreen() {
  SetCurrentScreen(GetScreen(DemoSetupScreenView::kScreenId));
}

void WizardController::ShowDrivePinningScreen() {
  if (drive::util::IsOobeDrivePinningAvailable()) {
    SetCurrentScreen(GetScreen(DrivePinningScreenView::kScreenId));
  } else {
    OnDrivePinningScreenExit(DrivePinningScreen::Result::NOT_APPLICABLE);
  }
}

void WizardController::ShowResetScreen() {
  SetCurrentScreen(GetScreen(ResetView::kScreenId));
}

void WizardController::ShowEnableAdbSideloadingScreen() {
  SetCurrentScreen(GetScreen(EnableAdbSideloadingScreenView::kScreenId));
}

void WizardController::ShowEnableDebuggingScreen() {
  SetCurrentScreen(GetScreen(EnableDebuggingScreenView::kScreenId));
}

void WizardController::ShowTermsOfServiceScreen() {
  SetCurrentScreen(GetScreen(TermsOfServiceScreenView::kScreenId));
}

void WizardController::ShowFamilyLinkNoticeScreen() {
  AdvanceToScreen(FamilyLinkNoticeView::kScreenId);
}

void WizardController::ShowSyncConsentScreen() {
  // First screen after login. Perform a timezone request so that any screens
  // relying on geolocation can tailor their contents according to the user's
  // region. Currently used on the MarketingOptInScreen.
  StartNetworkTimezoneResolve();

  SetCurrentScreen(GetScreen(SyncConsentScreenView::kScreenId));
}

void WizardController::StartAuthFactorsSetup() {
  ShowCryptohomeRecoverySetupScreen();
}

void WizardController::ShowCryptohomeRecoverySetupScreen() {
  SetCurrentScreen(GetScreen(CryptohomeRecoverySetupScreenView::kScreenId));
}

void WizardController::ShowPasswordSelectionScreen() {
  SetCurrentScreen(GetScreen(PasswordSelectionScreenView::kScreenId));
}

void WizardController::ShowOSAuthErrorScreen() {
  CHECK(wizard_context_->osauth_error.has_value());
  SetCurrentScreen(GetScreen(OSAuthErrorScreenView::kScreenId));
}

void WizardController::ShowFactorSetupSuccessScreen() {
  SetCurrentScreen(GetScreen(FactorSetupSuccessScreenView::kScreenId));
}

void WizardController::ShowFingerprintSetupScreen() {
  SetCurrentScreen(GetScreen(FingerprintSetupScreenView::kScreenId));
}

void WizardController::ShowPinSetupScreen() {
  // The PIN Setup screen can be used for setting up PIN as a main factor, or as
  // a secondary one. At this point, the mode must be known.
  CHECK(wizard_context_->knowledge_factor_setup.pin_setup_mode.has_value());
  SetCurrentScreen(GetScreen(PinSetupScreenView::kScreenId));
}

void WizardController::ShowThemeSelectionScreen() {
  SetCurrentScreen(GetScreen(ThemeSelectionScreenView::kScreenId));
}

void WizardController::ShowMarketingOptInScreen() {
  SetCurrentScreen(GetScreen(MarketingOptInScreenView::kScreenId));
}

void WizardController::ShowRecommendAppsScreen() {
  SetCurrentScreen(GetScreen(RecommendAppsScreenView::kScreenId));
}

void WizardController::ShowRemoteActivityNotificationScreen() {
  SetCurrentScreen(GetScreen(RemoteActivityNotificationView::kScreenId));
}

void WizardController::ShowAppDownloadingScreen() {
  SetCurrentScreen(GetScreen(AppDownloadingScreenView::kScreenId));
}

void WizardController::ShowAiIntroScreen() {
  SetCurrentScreen(GetScreen(AiIntroScreenView::kScreenId));
}

void WizardController::ShowGeminiIntroScreen() {
  SetCurrentScreen(GetScreen(GeminiIntroScreenView::kScreenId));
}

void WizardController::ShowWrongHWIDScreen() {
  SetCurrentScreen(GetScreen(WrongHWIDScreenView::kScreenId));
}

void WizardController::ShowAutoEnrollmentCheckScreen() {
  AutoEnrollmentCheckScreen* screen = GetScreen<AutoEnrollmentCheckScreen>();
  screen->set_auto_enrollment_controller(GetAutoEnrollmentController());
  SetCurrentScreen(screen);
}

void WizardController::ShowHIDDetectionScreen() {
  SetCurrentScreen(GetScreen(HIDDetectionView::kScreenId));
}

void WizardController::ShowDeviceDisabledScreen() {
  SetCurrentScreen(GetScreen(DeviceDisabledScreenView::kScreenId));
}

void WizardController::ShowEncryptionMigrationScreen() {
  SetCurrentScreen(GetScreen(EncryptionMigrationScreenView::kScreenId));
}

void WizardController::ShowManagementTransitionScreen() {
  SetCurrentScreen(GetScreen(ManagementTransitionScreenView::kScreenId));
}

void WizardController::ShowUpdateRequiredScreen() {
  SetCurrentScreen(GetScreen(UpdateRequiredView::kScreenId));
}

void WizardController::ShowAssistantOptInFlowScreen() {
  SetCurrentScreen(GetScreen(AssistantOptInFlowScreenView::kScreenId));
}

void WizardController::ShowMultiDeviceSetupScreen() {
  SetCurrentScreen(GetScreen(MultiDeviceSetupScreenView::kScreenId));
}

void WizardController::ShowGestureNavigationScreen() {
  SetCurrentScreen(GetScreen(GestureNavigationScreenView::kScreenId));
}

void WizardController::ShowPackagedLicenseScreen() {
  SetCurrentScreen(GetScreen(PackagedLicenseView::kScreenId));
}

void WizardController::ShowEduCoexistenceLoginScreen() {
  SetCurrentScreen(GetScreen(EduCoexistenceLoginScreen::kScreenId));
}

void WizardController::ShowParentalHandoffScreen() {
  SetCurrentScreen(GetScreen(ParentalHandoffScreenView::kScreenId));
}

void WizardController::ShowOsInstallScreen() {
  SetCurrentScreen(GetScreen(OsInstallScreenView::kScreenId));
}

void WizardController::ShowOsTrialScreen() {
  SetCurrentScreen(GetScreen(OsTrialScreenView::kScreenId));
}

void WizardController::ShowConsolidatedConsentScreen() {
  SetCurrentScreen(GetScreen(ConsolidatedConsentScreenView::kScreenId));
}

void WizardController::ShowChoobeScreen() {
  DCHECK(features::IsOobeChoobeEnabled());
  SetCurrentScreen(GetScreen(ChoobeScreenView::kScreenId));
}

void WizardController::ShowCategoriesSelectionScreen() {
  SetCurrentScreen(GetScreen(CategoriesSelectionScreenView::kScreenId));
}

void WizardController::ShowPersonalizedRecomendAppsScreen() {
  SetCurrentScreen(GetScreen(PersonalizedRecommendAppsScreenView::kScreenId));
}

void WizardController::ShowPerksDiscoveryScreen() {
  SetCurrentScreen(GetScreen(PerksDiscoveryScreenView::kScreenId));
}

void WizardController::ShowSplitModifierKeyboardInfoScreen() {
  SetCurrentScreen(GetScreen(SplitModifierKeyboardInfoScreenView::kScreenId));
}

void WizardController::ShowTouchpadScrollScreen() {
  // If the `OobeChoobe` or `OobeDisplaySize` feature is disabled, the
  // DisplaySizeScreen object will not be created. In this case,
  // `OnTouchpadScreenExit()` function is called with the exit result
  // `kNotApplicable` to proceed to the next screen.
  if (features::IsOobeTouchpadScrollEnabled()) {
    SetCurrentScreen(GetScreen(TouchpadScrollScreenView::kScreenId));
  } else {
    OnTouchpadScreenExit(TouchpadScrollScreen::Result::kNotApplicable);
  }
}

void WizardController::ShowDisplaySizeScreen() {
  if (features::IsOobeDisplaySizeEnabled()) {
    SetCurrentScreen(GetScreen(DisplaySizeScreenView::kScreenId));
  } else {
    OnDisplaySizeScreenExit(DisplaySizeScreen::Result::kNotApplicable);
  }
}

void WizardController::ShowGuestTosScreen() {
  SetCurrentScreen(GetScreen(GuestTosScreenView::kScreenId));
}

void WizardController::ShowArcVmDataMigrationScreen() {
  SetCurrentScreen(GetScreen(ArcVmDataMigrationScreenView::kScreenId));
}

void WizardController::ShowCryptohomeRecoveryScreen(
    std::unique_ptr<UserContext> user_context) {
  wizard_context_->user_context = std::move(user_context);
  SetCurrentScreen(GetScreen(CryptohomeRecoveryScreenView::kScreenId));
}

void WizardController::ShowAccountSelectionScreen() {
  SetCurrentScreen(GetScreen(AccountSelectionScreenView::kScreenId));
}

void WizardController::ShowAppLaunchSplashScreen() {
  SetCurrentScreen(GetScreen(AppLaunchSplashScreenView::kScreenId));
}

void WizardController::OnUserCreationScreenExit(
    UserCreationScreen::Result result) {
  OnScreenExit(UserCreationView::kScreenId,
               UserCreationScreen::GetResultString(result));
  switch (result) {
    case UserCreationScreen::Result::SIGNIN_SCHOOL:
      MaybeAbortQuickStartFlow(
          quick_start::QuickStartController::AbortFlowReason::SIGNIN_SCHOOL);
      [[fallthrough]];
    case UserCreationScreen::Result::SIGNIN_TRIAGE:
      GetLocalState()->SetBoolean(prefs::kOobeIsConsumerSegment, true);
      StartupUtils::SaveScreenAfterConsumerUpdate(GaiaView::kScreenId.name);
      ShowConsumerUpdateScreen();
      break;
    case UserCreationScreen::Result::SIGNIN:
      if (features::IsOobeSoftwareUpdateEnabled()) {
        if (features::IsOobeGaiaInfoScreenEnabled()) {
          GetLocalState()->SetBoolean(prefs::kOobeIsConsumerSegment, true);
          StartupUtils::SaveScreenAfterConsumerUpdate(
              GaiaInfoScreenView::kScreenId.name);
          ShowConsumerUpdateScreen();
        } else {
          GetLocalState()->SetBoolean(prefs::kOobeIsConsumerSegment, true);
          StartupUtils::SaveScreenAfterConsumerUpdate(GaiaView::kScreenId.name);
          ShowConsumerUpdateScreen();
        }
      } else {
        if (features::IsOobeGaiaInfoScreenEnabled()) {
          ShowGaiaInfoScreen();
        } else {
          AdvanceToScreen(GaiaView::kScreenId);
        }
      }
      break;
    case UserCreationScreen::Result::SKIPPED:
      if (features::IsOobeAddUserDuringEnrollmentEnabled() &&
          wizard_context_->timebound_user_context_holder) {
        ShowAccountSelectionScreen();
      } else {
        AdvanceToScreen(GaiaView::kScreenId);
      }
      break;
    case UserCreationScreen::Result::ADD_CHILD:
      MaybeAbortQuickStartFlow(
          quick_start::QuickStartController::AbortFlowReason::ADD_CHILD);
      if (features::IsOobeSoftwareUpdateEnabled()) {
        StartupUtils::SaveScreenAfterConsumerUpdate(
            AddChildScreenView::kScreenId.name);
        ShowConsumerUpdateScreen();
      } else {
        ShowAddChildScreen();
      }
      break;
    case UserCreationScreen::Result::ENTERPRISE_ENROLL_TRIAGE:
    case UserCreationScreen::Result::ENTERPRISE_ENROLL_SHORTCUT:
      ShowEnrollmentScreenIfEligible();
      break;
    case UserCreationScreen::Result::KIOSK_ENTERPRISE_ENROLL:
      wizard_context_->enrollment_preference_ =
          WizardContext::EnrollmentPreference::kKiosk;
      ShowEnrollmentScreenIfEligible();
      break;
    case UserCreationScreen::Result::CANCEL:
      LoginDisplayHost::default_host()->HideOobeDialog();
      break;
  }
}

void WizardController::OnConsumerUpdateScreenExit(
    ConsumerUpdateScreen::Result result) {
  OnScreenExit(ConsumerUpdateScreenView::kScreenId,
               ConsumerUpdateScreen::GetResultString(result));

  if (result == ConsumerUpdateScreen::Result::BACK) {
    AdvanceToScreen(UserCreationView::kScreenId);
    return;
  }

  const std::string screen_name =
      GetLocalState()->GetString(prefs::kOobeScreenAfterConsumerUpdate);
  if (screen_name == GaiaInfoScreenView::kScreenId.name) {
    if (features::IsOobeGaiaInfoScreenEnabled() &&
        HasScreen(PrefToScreenId(screen_name))) {
      AdvanceToScreen(PrefToScreenId(screen_name));
    } else {
      AdvanceToScreen(GaiaView::kScreenId);
    }
  } else if (HasScreen(PrefToScreenId(screen_name))) {
    AdvanceToScreen(PrefToScreenId(screen_name));
  } else {
    // Fallback for resuming consumer update screen from local state. This
    // handles cases where screen names/structure changed between versions.
    // 'OnUserCreationScreenExit' would update the state for compatibility.
    AdvanceToScreen(UserCreationView::kScreenId);
  }
}

void WizardController::OnGaiaScreenExit(GaiaScreen::Result result) {
  OnScreenExit(GaiaView::kScreenId, GaiaScreen::GetResultString(result));
  switch (result) {
    case GaiaScreen::Result::BACK_CHILD:
      ShowAddChildScreen();
      break;
    case GaiaScreen::Result::BACK:
    case GaiaScreen::Result::CANCEL: {
      if (features::IsOobeSoftwareUpdateEnabled()) {
        // When `OobeSoftwareUpdate` is enabled, clicking the back button should
        // return the user to the user creation screen if it is enabled or the
        // user is still in the oobe flow.
        if ((wizard_context_->is_user_creation_enabled ||
             !wizard_context_->is_add_person_flow) &&
            result == GaiaScreen::Result::BACK) {
          AdvanceToScreen(UserCreationView::kScreenId);
          break;
        }
      }
      if (features::IsOobeGaiaInfoScreenEnabled()) {
        if (wizard_context_->is_user_creation_enabled) {
          // `Result::BACK` and `Result::BACK_CHILD` are only triggered when
          // pressing back button. It goes back to GaiaInfoScreenView if user
          // creation is enabled; otherwise, it behaves the same as
          // `Result::CANCEL` which is triggered by pressing ESC key.
          if (result == GaiaScreen::Result::BACK) {
            if (wizard_context_->is_add_person_flow) {
              AdvanceToScreen(UserCreationView::kScreenId);
            } else {
              AdvanceToScreen(GaiaInfoScreenView::kScreenId);
            }
            break;
          }
        }
      } else {
        // TODO: delete this part after removing the feature flag (b:282728089)
        if (result == GaiaScreen::Result::BACK &&
            wizard_context_->is_user_creation_enabled) {
          // `Result::BACK` is only triggered when pressing back button. It goes
          // back to UserCreationScreen if screen is enabled; otherwise, it
          // behaves the same as `Result::CANCEL` which is triggered by pressing
          // ESC key.
          AdvanceToScreen(UserCreationView::kScreenId);
          break;
        }
      }

      // If a default redirection to third party IdP is set we can hide the
      // dialog.
      const bool gaia_page_defaults_to_saml = IsGaiaPageDefaultsToSAML();
      if ((LoginDisplayHost::default_host()->HasUserPods() &&
           !wizard_context_->is_user_creation_enabled) ||
          (!LoginDisplayHost::default_host()->HasUserPods() &&
           gaia_page_defaults_to_saml)) {
        GetScreen<GaiaScreen>()->Reset();
        LoginDisplayHost::default_host()->HideOobeDialog(
            gaia_page_defaults_to_saml);
      } else {
        GetScreen<GaiaScreen>()->LoadOnlineGaia();
      }
      break;
    }
    case GaiaScreen::Result::ENTERPRISE_ENROLL:
      ShowEnrollmentScreenIfEligible();
      break;
    case GaiaScreen::Result::ENTER_QUICK_START:
      [[fallthrough]];
    case GaiaScreen::Result::QUICK_START_ONGOING:
      ShowQuickStartScreen();
      break;
  }
}

void WizardController::OnUserAllowlistCheckScreenExit(
    UserAllowlistCheckScreen::Result result) {
  CHECK(result == UserAllowlistCheckScreen::Result::RETRY);
  OnScreenExit(UserAllowlistCheckScreenView::kScreenId,
               UserAllowlistCheckScreen::GetResultString(result));
  GetScreen<GaiaScreen>()->Reset();
  AdvanceToScreen(GaiaView::kScreenId);
}

void WizardController::OnOnlineAuthenticationScreenExit(
    OnlineAuthenticationScreen::Result result) {}

void WizardController::OnGaiaInfoScreenExit(GaiaInfoScreen::Result result) {
  OnScreenExit(GaiaInfoScreenView::kScreenId,
               GaiaInfoScreen::GetResultString(result));
  switch (result) {
    case GaiaInfoScreen::Result::kBack:
      AdvanceToScreen(UserCreationView::kScreenId);
      break;
    case GaiaInfoScreen::Result::kManual:
      [[fallthrough]];
    case GaiaInfoScreen::Result::kNotApplicable:
      AdvanceToScreen(GaiaView::kScreenId);
      break;
    case GaiaInfoScreen::Result::kEnterQuickStart:
      [[fallthrough]];
    case GaiaInfoScreen::Result::kQuickStartOngoing:
      ShowQuickStartScreen();
      break;
  }
}

void WizardController::OnAddChildScreenExit(AddChildScreen::Result result) {
  OnScreenExit(AddChildScreenView::kScreenId,
               AddChildScreen::GetResultString(result));
  switch (result) {
    case AddChildScreen::Result::CHILD_SIGNIN:
      wizard_context_->gaia_config.gaia_path =
          WizardContext::GaiaPath::kChildSignin;
      AdvanceToScreen(GaiaView::kScreenId);
      break;
    case AddChildScreen::Result::CHILD_ACCOUNT_CREATE:
      wizard_context_->gaia_config.gaia_path =
          WizardContext::GaiaPath::kChildSignup;
      AdvanceToScreen(GaiaView::kScreenId);
      break;
    case AddChildScreen::Result::ENTERPRISE_ENROLL:
      ShowEnrollmentScreenIfEligible();
      break;
    case AddChildScreen::Result::KIOSK_ENTERPRISE_ENROLL:
      wizard_context_->enrollment_preference_ =
          WizardContext::EnrollmentPreference::kKiosk;
      ShowEnrollmentScreenIfEligible();
      break;
    case AddChildScreen::Result::SKIPPED:
      AdvanceToScreen(GaiaView::kScreenId);
      break;
    case AddChildScreen::Result::BACK:
      AdvanceToScreen(UserCreationView::kScreenId);
      if (features::IsOobeSoftwareUpdateEnabled()) {
        GetScreen<UserCreationScreen>()->SetChildSetupStep();
      }
      break;
  }
}

void WizardController::OnSamlConfirmPasswordScreenExit(
    SamlConfirmPasswordScreen::Result result) {
  OnScreenExit(SamlConfirmPasswordView::kScreenId,
               SamlConfirmPasswordScreen::GetResultString(result));
  switch (result) {
    case SamlConfirmPasswordScreen::Result::kCancel:
      LoginDisplayHost::default_host()->StartSignInScreen();
      return;
    case SamlConfirmPasswordScreen::Result::kTooManyAttempts:
      ShowSignInFatalErrorScreen(
          SignInFatalErrorScreen::Error::kScrapedPasswordVerificationFailure,
          base::Value::Dict());
  }
}

void WizardController::OnEduCoexistenceLoginScreenExit(
    EduCoexistenceLoginScreen::Result result) {
  OnScreenExit(EduCoexistenceLoginScreen::kScreenId,
               EduCoexistenceLoginScreen::GetResultString(result));
  ShowConsolidatedConsentScreen();
}

void WizardController::OnParentalHandoffScreenExit(
    ParentalHandoffScreen::Result result) {
  OnScreenExit(ParentalHandoffScreenView::kScreenId,
               ParentalHandoffScreen::GetResultString(result));
  ShowMultiDeviceSetupScreen();
}

void WizardController::OnConsolidatedConsentScreenExit(
    ConsolidatedConsentScreen::Result result) {
  OnScreenExit(ConsolidatedConsentScreenView::kScreenId,
               ConsolidatedConsentScreen::GetResultString(result));

  if (drive::util::IsOobeDrivePinningAvailable()) {
    GetScreen<DrivePinningScreen>()->StartCalculatingRequiredSpace();
  }

  if (wizard_context_->is_cloud_ready_update_flow) {
    AdvanceToScreen(HWDataCollectionView::kScreenId);
    return;
  }
  switch (result) {
    case ConsolidatedConsentScreen::Result::ACCEPTED:
    case ConsolidatedConsentScreen::Result::NOT_APPLICABLE:
      ShowSyncConsentScreen();
      break;
    case ConsolidatedConsentScreen::Result::ACCEPTED_DEMO_ONLINE:
      DCHECK(demo_setup_controller_);
      ShowAutoEnrollmentCheckScreen();
      break;
    case ConsolidatedConsentScreen::Result::BACK_DEMO:
      DCHECK(demo_setup_controller_);
      ShowDemoModePreferencesScreen();
      break;
  }
}

void WizardController::OnOfflineLoginScreenExit(
    OfflineLoginScreen::Result result) {
  OnScreenExit(OfflineLoginView::kScreenId,
               OfflineLoginScreen::GetResultString(result));
  switch (result) {
    case OfflineLoginScreen::Result::BACK:
      // Go back to online login, if still no connection it will trigger
      // ErrorScreen with fix options. If UserCreationScreen isn't available
      // it will exit with Result::SKIPPED and open GaiaScreen instead.
      AdvanceToScreen(UserCreationView::kScreenId);
      break;
    case OfflineLoginScreen::Result::RELOAD_ONLINE_LOGIN:
      AdvanceToScreen(GaiaView::kScreenId);
      break;
  }
}

void WizardController::OnOsInstallScreenExit() {
  OnScreenExit(OsInstallScreenView::kScreenId, kDefaultExitReason);
  // The screen exits when user goes back. There could be a previous_screen_ or
  // we could get to OsInstallScreen directly from the login screen.
  // (When installation is finished or error occurs - user can only shut down)
  if (LoginDisplayHost::default_host()->HasUserPods()) {
    LoginDisplayHost::default_host()->HideOobeDialog();
    return;
  }
  const bool did_advance = MaybeSetToPreviousScreen();
  DCHECK(did_advance);
}

void WizardController::OnOsTrialScreenExit(OsTrialScreen::Result result) {
  OnScreenExit(OsTrialScreenView::kScreenId,
               OsTrialScreen::GetResultString(result));
  switch (result) {
    case OsTrialScreen::Result::kBack:
      // The OS Trial screen is only shown when OS Installation is started from
      // the welcome screen, so if the back button was clicked we go back to
      // the welcome screen.
      ShowWelcomeScreen();
      break;
    case OsTrialScreen::Result::kNextTry:
      ShowNetworkScreen();
      break;
    case OsTrialScreen::Result::kNextInstall:
      ShowOsInstallScreen();
      break;
  }
}

void WizardController::OnHWDataCollectionScreenExit(
    HWDataCollectionScreen::Result result) {
  OnScreenExit(HWDataCollectionView::kScreenId,
               HWDataCollectionScreen::GetResultString(result));
  if (wizard_context_->is_cloud_ready_update_flow) {
    OnOobeFlowFinished();
    return;
  }
  StartAuthFactorsSetup();
}

void WizardController::OnSmartPrivacyProtectionScreenExit(
    SmartPrivacyProtectionScreen::Result result) {
  OnScreenExit(SmartPrivacyProtectionView::kScreenId,
               SmartPrivacyProtectionScreen::GetResultString(result));
  ShowParentalHandoffScreen();
}

void WizardController::OnGuestTosScreenExit(GuestTosScreen::Result result) {
  OnScreenExit(GuestTosScreenView::kScreenId,
               GuestTosScreen::GetResultString(result));
  switch (result) {
    case GuestTosScreen::Result::ACCEPT:
      ash::LoginDisplayHost::default_host()->GetExistingUserController()->Login(
          UserContext(user_manager::UserType::kGuest,
                      user_manager::GuestAccountId()),
          SigninSpecifics());
      break;
    case GuestTosScreen::Result::BACK:
      if (MaybeSetToPreviousScreen()) {
        break;
      }
      if (LoginDisplayHost::default_host()->HasUserPods()) {
        LoginDisplayHost::default_host()->HideOobeDialog();
      }
      break;
    case GuestTosScreen::Result::CANCEL:
      if (LoginDisplayHost::default_host()->HasUserPods()) {
        LoginDisplayHost::default_host()->HideOobeDialog();
      }
  }
}

void WizardController::OnThemeSelectionScreenExit(
    ThemeSelectionScreen::Result result) {
  OnScreenExit(ThemeSelectionScreenView::kScreenId,
               ThemeSelectionScreen::GetResultString(result));

  switch (result) {
    case ThemeSelectionScreen::Result::kProceed:
    case ThemeSelectionScreen::Result::kNotApplicable:
      if (wizard_context_->return_to_choobe_screen) {
        wizard_context_->return_to_choobe_screen = false;
        ShowChoobeScreen();
      } else {
        if (choobe_flow_controller_) {
          choobe_flow_controller_->OnChoobeFlowExit();
          choobe_flow_controller_.reset();
        }
        ShowMarketingOptInScreen();
      }
      break;
  }
}

void WizardController::OnCryptohomeRecoveryScreenExit(
    CryptohomeRecoveryScreen::Result result) {
  OnScreenExit(CryptohomeRecoveryScreenView::kScreenId,
               CryptohomeRecoveryScreen::GetResultString(result));
  switch (result) {
    case CryptohomeRecoveryScreen::Result::kAuthenticated: {
      switch (wizard_context_->knowledge_factor_setup.auth_setup_flow) {
        case WizardContext::AuthChangeFlow::kInitialSetup:
          NOTREACHED_IN_MIGRATION()
              << "Recovery can not be used during initial setup.";
          return;
        case WizardContext::AuthChangeFlow::kRecovery:
          ShowPasswordSelectionScreen();
          return;
        case WizardContext::AuthChangeFlow::kReauthentication:
          // Proceed with login
          ObtainContextAndLoginAuthenticated();
          return;
      }
    }
    case CryptohomeRecoveryScreen::Result::kGaiaLogin:
      // TODO(b/257073746): We probably want to differentiate between retry with
      // or without login.
      wizard_context_->gaia_config.prefilled_account =
          wizard_context_->user_context->GetAccountId();
      AdvanceToScreen(GaiaView::kScreenId);
      break;
    case CryptohomeRecoveryScreen::Result::kFallbackOnline:
      ShowEnterOldPasswordScreen();
      break;
    case CryptohomeRecoveryScreen::Result::kFallbackLocal: {
      switch (wizard_context_->knowledge_factor_setup.auth_setup_flow) {
        case WizardContext::AuthChangeFlow::kInitialSetup:
          NOTREACHED_IN_MIGRATION()
              << "Recovery is not used during initial setup";
          return;
        case WizardContext::AuthChangeFlow::kReauthentication:
          AttemptLocalAuthenticationWithContext(
              std::move(wizard_context_->user_context));
          return;
        case WizardContext::AuthChangeFlow::kRecovery:
          // Recovery flow indicates that user does not remember
          // their local password, so there is no step to retry.
          wizard_context_->knowledge_factor_setup.data_loss_back_option =
              WizardContext::DataLossBackOptions::kNone;
          ShowLocalDataLossWarningScreen();
          return;
      }
    }
    case CryptohomeRecoveryScreen::Result::kError:
      ShowOSAuthErrorScreen();
      break;
  }
}

void WizardController::OnLocalDataLossWarningScreenExit(
    LocalDataLossWarningScreen::Result result) {
  OnScreenExit(LocalDataLossWarningScreenView::kScreenId,
               LocalDataLossWarningScreen::GetResultString(result));
  switch (result) {
    case LocalDataLossWarningScreen::Result::kRemoveUser: {
      std::unique_ptr<UserContext> context =
          std::move(wizard_context_->user_context);
      ash::LoginDisplayHost::default_host()->CompleteLogin(*context);
      break;
    }
    case LocalDataLossWarningScreen::Result::kCryptohomeError:
      ShowOSAuthErrorScreen();
      break;
    case LocalDataLossWarningScreen::Result::kCancel:
      LoginDisplayHost::default_host()->CancelPasswordChangedFlow();
      break;
    case LocalDataLossWarningScreen::Result::kBackToOnlineAuth:
      ShowEnterOldPasswordScreen();
      break;
    case LocalDataLossWarningScreen::Result::kBackToLocalAuth:
      AttemptLocalAuthenticationWithContext(
          std::move(wizard_context_->user_context));
      break;
  }
}

void WizardController::OnEnterOldPasswordScreenExit(
    EnterOldPasswordScreen::Result result) {
  OnScreenExit(EnterOldPasswordScreenView::kScreenId,
               EnterOldPasswordScreen::GetResultString(result));
  switch (result) {
    case EnterOldPasswordScreen::Result::kForgotOldPassword:
      wizard_context_->knowledge_factor_setup.data_loss_back_option =
          WizardContext::DataLossBackOptions::kBackToOnlineAuth;
      ShowLocalDataLossWarningScreen();
      break;
    case EnterOldPasswordScreen::Result::kCryptohomeError:
      ShowOSAuthErrorScreen();
      break;
    case EnterOldPasswordScreen::Result::kAuthenticated: {
      switch (wizard_context_->knowledge_factor_setup.auth_setup_flow) {
        case WizardContext::AuthChangeFlow::kInitialSetup:
          NOTREACHED_IN_MIGRATION()
              << "Old password is not used during initial setup";
          break;
        case WizardContext::AuthChangeFlow::kRecovery:
        case WizardContext::AuthChangeFlow::kReauthentication:
          ShowApplyOnlinePasswordScreen();
          return;
      }
      break;
    }
  }
}

void WizardController::OnChoobeScreenExit(ChoobeScreen::Result result) {
  OnScreenExit(ChoobeScreenView::kScreenId,
               ChoobeScreen::GetResultString(result));

  switch (result) {
    case ChoobeScreen::Result::SELECTED:
    case ChoobeScreen::Result::NOT_APPLICABLE:
      ShowTouchpadScrollScreen();
      break;
    case ChoobeScreen::Result::SKIPPED:
      choobe_flow_controller_->OnChoobeFlowExit();
      choobe_flow_controller_.reset();
      ShowMarketingOptInScreen();
      break;
  }
}

void WizardController::OnTouchpadScreenExit(
    TouchpadScrollScreen::Result result) {
  OnScreenExit(TouchpadScrollScreenView::kScreenId,
               TouchpadScrollScreen::GetResultString(result));

  ShowDrivePinningScreen();
}

void WizardController::OnCategoriesSelectionScreenExit(
    CategoriesSelectionScreen::Result result) {
  OnScreenExit(CategoriesSelectionScreenView::kScreenId,
               CategoriesSelectionScreen::GetResultString(result));
  switch (result) {
    case CategoriesSelectionScreen::Result::kError:
    case CategoriesSelectionScreen::Result::kNotApplicable:
    case CategoriesSelectionScreen::Result::kDataMalformed:
    case CategoriesSelectionScreen::Result::kTimeout:
      ShowRecommendAppsScreen();
      break;
    case CategoriesSelectionScreen::Result::kNext:
    case CategoriesSelectionScreen::Result::kSkip:
      ShowPersonalizedRecomendAppsScreen();
      break;
  }
}

void WizardController::OnPerksDiscoveryScreenExit(
    PerksDiscoveryScreen::Result result) {
  OnScreenExit(PerksDiscoveryScreenView::kScreenId,
               PerksDiscoveryScreen::GetResultString(result));
  if (features::IsOobeAiIntroEnabled()) {
    ShowAiIntroScreen();
  } else if (features::IsOobeGeminiIntroEnabled()) {
    ShowGeminiIntroScreen();
  } else {
    ShowAssistantOptInFlowScreen();
  }
}

void WizardController::OnPersonalizedRecomendAppsScreenExit(
    PersonalizedRecommendAppsScreen::Result result) {
  OnScreenExit(PersonalizedRecommendAppsScreenView::kScreenId,
               PersonalizedRecommendAppsScreen::GetResultString(result));

  switch (result) {
    case PersonalizedRecommendAppsScreen::Result::kBack:
      ShowCategoriesSelectionScreen();
      break;
    case PersonalizedRecommendAppsScreen::Result::kNext:
    case PersonalizedRecommendAppsScreen::Result::kNotApplicable:
    case PersonalizedRecommendAppsScreen::Result::kSkip:
    case PersonalizedRecommendAppsScreen::Result::kDataMalformed:
    case PersonalizedRecommendAppsScreen::Result::kError:
    case PersonalizedRecommendAppsScreen::Result::kTimeout:
        ShowPerksDiscoveryScreen();
      break;
  }
}

void WizardController::OnSplitModifierKeyboardInfoScreenExit(
    SplitModifierKeyboardInfoScreen::Result result) {
  OnScreenExit(SplitModifierKeyboardInfoScreenView::kScreenId,
               SplitModifierKeyboardInfoScreen::GetResultString(result));

  ShowGestureNavigationScreen();
}

void WizardController::OnDrivePinningScreenExit(
    DrivePinningScreen::Result result) {
  OnScreenExit(DrivePinningScreenView::kScreenId,
               DrivePinningScreen::GetResultString(result));
  if (features::IsOobeDisplaySizeEnabled()) {
    ShowDisplaySizeScreen();
  } else {
    OnDisplaySizeScreenExit(DisplaySizeScreen::Result::kNotApplicable);
  }
}

void WizardController::OnDisplaySizeScreenExit(
    DisplaySizeScreen::Result result) {
  OnScreenExit(DisplaySizeScreenView::kScreenId,
               DisplaySizeScreen::GetResultString(result));

  switch (result) {
    case DisplaySizeScreen::Result::kNotApplicable:
    case DisplaySizeScreen::Result::kNext:
      ShowThemeSelectionScreen();
  }
}

void WizardController::OnAccountSelectionScreenExit(
    AccountSelectionScreen::Result result) {
  OnScreenExit(AccountSelectionScreenView::kScreenId,
               AccountSelectionScreen::GetResultString(result));
  switch (result) {
    case AccountSelectionScreen::Result::kNotApplicable:
    case AccountSelectionScreen::Result::kGaiaFallback:
      AdvanceToScreen(GaiaView::kScreenId);
  }
}

void WizardController::SkipToLoginForTesting() {
  VLOG(1) << "WizardController::SkipToLoginForTesting()";

  // This method should only be used on test images.
  base::SysInfo::CrashIfChromeOSNonTestImage();

  if (current_screen_ && current_screen_->screen_id() == GaiaView::kScreenId) {
    return;
  }
  wizard_context_->skip_to_login_for_tests = true;

  StartNetworkTimezoneResolve();
  DelayNetworkCall(ServicesCustomizationDocument::GetInstance()
                       ->EnsureCustomizationAppliedClosure());
  OnDeviceDisabledChecked(/*device_disabled=*/false);
}

void WizardController::OnScreenExit(OobeScreenId screen,
                                    const std::string& exit_reason) {
  VLOG(1) << "Wizard screen " << screen
          << " exited with reason: " << exit_reason << " during setup type: "
          << SetupTypeToString(
                 wizard_context_->knowledge_factor_setup.auth_setup_flow);
  // Do not perform checks and record stats for the skipped screen.
  if (exit_reason == BaseScreen::kNotApplicable) {
    return;
  }
  DCHECK(current_screen_->screen_id() == screen);

  GetLoginDisplayHost()->GetOobeMetricsHelper()->RecordScreenExit(screen,
                                                                  exit_reason);
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, ExitHandlers:
void WizardController::OnWrongHWIDScreenExit() {
  OnScreenExit(WrongHWIDScreenView::kScreenId, kDefaultExitReason);
  OnDeviceModificationCanceled();
}

void WizardController::OnHidDetectionScreenExit(
    HIDDetectionScreen::Result result) {
  OnScreenExit(HIDDetectionView::kScreenId,
               HIDDetectionScreen::GetResultString(result));

  if (result == HIDDetectionScreen::Result::SKIPPED_FOR_TESTS &&
      current_screen_) {
    return;
  }

  AdvanceToScreenAfterHIDDetection(ash::OOBE_SCREEN_UNKNOWN);
}

void WizardController::OnWelcomeScreenExit(WelcomeScreen::Result result) {
  OnScreenExit(WelcomeView::kScreenId, WelcomeScreen::GetResultString(result));

  switch (result) {
    case WelcomeScreen::Result::kSetupDemo:
      StartDemoModeSetup();
      return;
    case WelcomeScreen::Result::kEnableDebugging:
      ShowEnableDebuggingScreen();
      return;
    case WelcomeScreen::Result::kNextOSInstall:
      ShowOsTrialScreen();
      return;
    case WelcomeScreen::Result::kNext:
      ShowNetworkScreen();
      return;
    case WelcomeScreen::Result::kQuickStart:
      ShowQuickStartScreen();
      return;
  }
}

void WizardController::OnQuickStartScreenExit(QuickStartScreen::Result result) {
  OnScreenExit(QuickStartView::kScreenId,
               QuickStartScreen::GetResultString(result));
  switch (result) {
    case QuickStartScreen::Result::CANCEL_AND_RETURN_TO_WELCOME:
      ShowWelcomeScreen();
      return;
    case QuickStartScreen::Result::WIFI_CREDENTIALS_RECEIVED:
    case QuickStartScreen::Result::CANCEL_AND_RETURN_TO_NETWORK:
      ShowNetworkScreen();
      return;
    case QuickStartScreen::Result::CANCEL_AND_RETURN_TO_GAIA_INFO:
      AdvanceToScreen(GaiaInfoScreenView::kScreenId);
      return;
    case ash::QuickStartScreen::Result::FALLBACK_URL_ON_GAIA:
      wizard_context_->gaia_config.gaia_path =
          WizardContext::GaiaPath::kQuickStartFallback;
      wizard_context_->gaia_config.quick_start_fallback_path_contents =
          quickstart_controller_->GetFallbackUrl();
      AdvanceToScreen(GaiaView::kScreenId);
      return;
    case QuickStartScreen::Result::CANCEL_AND_RETURN_TO_SIGNIN:
      AdvanceToScreen(GaiaView::kScreenId);
      return;
    // Last step of the QuickStart flow. This is triggered immediately
    // after the 'RecoveryEligibility' screen and continues OOBE into
    // the TermsOfServiceScreen
    case QuickStartScreen::Result::SETUP_COMPLETE_NEXT_BUTTON:
      quickstart_controller_->RecordFlowFinished();
      AdvanceToScreen(TermsOfServiceScreenView::kScreenId);
  }
}

// The network screen is part of 3 different flows:
// * Regular Flow: Welcome Screen -> Network Screen -> Update Screen.
// * Demo Flow: Welcome Screen -> Network Screen -> Demo Preferences Screen.
// * OS Install Flow: OS Trial Screen -> Network Screen -> Update Screen.
void WizardController::OnNetworkScreenExit(NetworkScreen::Result result) {
  OnScreenExit(NetworkScreenView::kScreenId,
               NetworkScreen::GetResultString(result));

  // Demo mode flow.
  if (DemoSetupController::IsOobeDemoSetupFlowInProgress()) {
    switch (result) {
      case NetworkScreen::Result::CONNECTED:
      case NetworkScreen::Result::NOT_APPLICABLE:
        demo_setup_controller_->set_demo_config(
            DemoSession::DemoModeConfig::kOnline);
        ShowDemoModePreferencesScreen();
        break;
      case NetworkScreen::Result::BACK:
        demo_setup_controller_.reset();
        ShowWelcomeScreen();
        break;
      case NetworkScreen::Result::QUICK_START:
        NOTREACHED_IN_MIGRATION();
        break;
    }
    return;
  }

  // OS Install flow.
  if (switches::IsOsInstallAllowed()) {
    switch (result) {
      case NetworkScreen::Result::CONNECTED:
      case NetworkScreen::Result::NOT_APPLICABLE:
        MaybeTakeTPMOwnership();
        PerformPostNetworkScreenActions();
        InitiateOOBEUpdate();
        break;
      case NetworkScreen::Result::BACK:
        ShowOsTrialScreen();
        break;
      case NetworkScreen::Result::QUICK_START:
        NOTREACHED_IN_MIGRATION();
        break;
    }
    return;
  }

  // Regular flow.
  switch (result) {
    case NetworkScreen::Result::CONNECTED:
    case NetworkScreen::Result::NOT_APPLICABLE:
      MaybeTakeTPMOwnership();
      PerformPostNetworkScreenActions();
      InitiateOOBEUpdate();
      break;
    case NetworkScreen::Result::BACK:
      ShowWelcomeScreen();
      break;
    case NetworkScreen::Result::QUICK_START:
      ShowQuickStartScreen();
      break;
  }
}

void WizardController::OnUpdateScreenExit(UpdateScreen::Result result) {
  OnScreenExit(UpdateView::kScreenId, UpdateScreen::GetResultString(result));

  switch (result) {
    case UpdateScreen::Result::UPDATE_NOT_REQUIRED:
    case UpdateScreen::Result::UPDATE_CHECK_TIMEOUT:
    case UpdateScreen::Result::UPDATE_SKIPPED:
    case UpdateScreen::Result::UPDATE_OPT_OUT_INFO_SHOWN:
      OnUpdateCompleted();
      break;
    case UpdateScreen::Result::UPDATE_ERROR:
      // Ignore update errors if the OOBE flow has already completed - this
      // prevents the user getting blocked from getting to the login screen.
      if (is_out_of_box_) {
        ShowNetworkScreen();
      } else {
        OnUpdateCompleted();
      }
      break;
  }
}

void WizardController::OnUpdateCompleted() {
  // Install language packs based on the user selected language.
  if (ash::features::IsLanguagePacksInOobeEnabled()) {
    const std::string locale = GetApplicationLocale();
    language_packs::LanguagePackManager::UpdatePacksForOobe(locale,
                                                            base::DoNothing());
  }

  if (demo_setup_controller_) {
    ShowConsolidatedConsentScreen();
    return;
  }
  ShowAutoEnrollmentCheckScreen();
}

void WizardController::OnAutoEnrollmentCheckScreenExit(
    AutoEnrollmentCheckScreen::Result result) {
  OnScreenExit(AutoEnrollmentCheckScreenView::kScreenId,
               AutoEnrollmentCheckScreen::GetResultString(result));
  VLOG(1) << "WizardController::OnAutoEnrollmentCheckScreenExit()";
  // Check whether the device is disabled. OnDeviceDisabledChecked() will be
  // invoked when the result of this check is known. Until then, the current
  // screen will remain visible and will continue showing a spinner.
  g_browser_process->platform_part()
      ->device_disabling_manager()
      ->CheckWhetherDeviceDisabledDuringOOBE(
          base::BindRepeating(&WizardController::OnDeviceDisabledChecked,
                              weak_factory_.GetWeakPtr()));
}

void WizardController::OnEnrollmentScreenExit(EnrollmentScreen::Result result) {
  OnScreenExit(EnrollmentScreenView::kScreenId,
               EnrollmentScreen::GetResultString(result));
  VLOG(1) << "WizardController::OnEnrollmentScreenExit(result= "
          << EnrollmentScreen::GetResultString(result) << ").";
  switch (result) {
    case EnrollmentScreen::Result::COMPLETED:
      OnEnrollmentDone();
      break;
    case EnrollmentScreen::Result::BACK:
    case EnrollmentScreen::Result::SKIPPED_FOR_TESTS:
      // The following `PerformOOBECompletedAction()` call will occur in both
      // manual and auto enrollment. However, in the manual enrollment case,
      // `PerformOOBECompletedAction()` method would be already called before
      // with `CompletedPreLoginOobeFlowType::kRegular` argument.
      // OOBECompletedActions are only performed in the first call.
      PerformOOBECompletedActions(
          OobeMetricsHelper::CompletedPreLoginOobeFlowType::kAutoEnrollment);
      DCHECK(!prescribed_enrollment_config_.is_forced());
      // set  the userCreationScreen with the default step creation and
      // pre-select 'For personal use'.
      GetScreen<UserCreationScreen>()->SetDefaultStep();
      ShowLoginScreen();
      break;
    case EnrollmentScreen::Result::TPM_ERROR:
      DCHECK(switches::IsTpmDynamic());
      wizard_context_->tpm_owned_error = true;
      AdvanceToScreen(TpmErrorView::kScreenId);
      break;
    case EnrollmentScreen::Result::TPM_DBUS_ERROR:
      DCHECK(switches::IsTpmDynamic());
      wizard_context_->tpm_dbus_error = true;
      AdvanceToScreen(TpmErrorView::kScreenId);
      break;
    case EnrollmentScreen::Result::BACK_TO_AUTO_ENROLLMENT_CHECK:
      ShowAutoEnrollmentCheckScreen();
      break;
  }
}

void WizardController::OnEnrollmentDone() {
  // The following `PerformOOBECompletedAction()` call will occur in both
  // manual and auto enrollment. However, in the manual enrollment case,
  // `PerformOOBECompletedAction()` method would be already called before
  // with `CompletedPreLoginOobeFlowType::kRegular` argument.
  // OOBECompletedActions are only performed in the first call.
  PerformOOBECompletedActions(
      OobeMetricsHelper::CompletedPreLoginOobeFlowType::kAutoEnrollment);

  // Restart to make the login page pick up the policy changes resulting from
  // enrollment recovery.  (Not pretty, but this codepath is rarely
  // exercised.)
  if (prescribed_enrollment_config_.mode ==
      policy::EnrollmentConfig::MODE_RECOVERY) {
    LOG(WARNING) << "Restart Chrome to pick up the policy changes";
    EnrollmentScreen* screen = EnrollmentScreen::Get(screen_manager());
    screen->OnBrowserRestart();
    chrome::AttemptRestart();
    return;
  }

  // We need a log to understand when the device finished enrollment.
  VLOG(1) << "Enrollment done";

  if (auto app = KioskController::Get().GetAutoLaunchApp(); app.has_value()) {
    AutoLaunchKioskApp(app.value());
  } else if (ash::InstallAttributes::Get()->IsEnterpriseManaged()) {
    // Could be not managed in tests.
    DCHECK_EQ(LoginDisplayHost::default_host()->GetOobeUI()->display_type(),
              OobeUI::kOobeDisplay);
    SwitchWebUItoMojo();
  } else {
    ShowLoginScreen();
  }
}

void WizardController::OnEnableAdbSideloadingScreenExit() {
  OnScreenExit(EnableAdbSideloadingScreenView::kScreenId, kDefaultExitReason);

  OnDeviceModificationCanceled();
}

void WizardController::OnEnableDebuggingScreenExit() {
  OnScreenExit(EnableDebuggingScreenView::kScreenId, kDefaultExitReason);

  OnDeviceModificationCanceled();
}

void WizardController::OnDemoPreferencesScreenExit(
    DemoPreferencesScreen::Result result) {
  OnScreenExit(DemoPreferencesScreenView::kScreenId,
               DemoPreferencesScreen::GetResultString(result));

  DCHECK(demo_setup_controller_);

  switch (result) {
    case DemoPreferencesScreen::Result::COMPLETED:
      demo_setup_controller_->set_demo_config(
          DemoSession::DemoModeConfig::kOnline);
      MaybeTakeTPMOwnership();
      PerformPostNetworkScreenActions();
      InitiateOOBEUpdate();
      break;
    case DemoPreferencesScreen::Result::CANCELED:
      ShowNetworkScreen();
      break;
  }
}

void WizardController::OnDemoSetupScreenExit(DemoSetupScreen::Result result) {
  OnScreenExit(DemoSetupScreenView::kScreenId,
               DemoSetupScreen::GetResultString(result));

  DCHECK(demo_setup_controller_);
  demo_setup_controller_.reset();

  switch (result) {
    case DemoSetupScreen::Result::kCompleted:
      PerformOOBECompletedActions(
          OobeMetricsHelper::CompletedPreLoginOobeFlowType::kDemo);
      SwitchWebUItoMojo();
      break;
    case DemoSetupScreen::Result::kCanceled:
      ShowWelcomeScreen();
      break;
  }
}

void WizardController::OnLocaleSwitchScreenExit(
    LocaleSwitchScreen::Result result) {
  OnScreenExit(LocaleSwitchView::kScreenId,
               LocaleSwitchScreen::GetResultString(result));
  AdvanceToScreen(RecoveryEligibilityView::kScreenId);
}

void WizardController::OnRecoveryEligibilityScreenExit(
    RecoveryEligibilityScreen::Result result) {
  OnScreenExit(RecoveryEligibilityView::kScreenId,
               RecoveryEligibilityScreen::GetResultString(result));
  // QuickStart's 'Setup Complete' screen step is the first screen
  // that a user sees after logging in. It just shows a 'Next' button
  // which exits the screen into the TermsOfServiceScreen
  if (wizard_context_->quick_start_enabled &&
      wizard_context_->quick_start_setup_ongoing) {
    AdvanceToScreen(QuickStartView::kScreenId);
  } else {
    AdvanceToScreen(TermsOfServiceScreenView::kScreenId);
  }
}

void WizardController::OnTermsOfServiceScreenExit(
    TermsOfServiceScreen::Result result) {
  OnScreenExit(TermsOfServiceScreenView::kScreenId,
               TermsOfServiceScreen::GetResultString(result));

  switch (result) {
    case TermsOfServiceScreen::Result::ACCEPTED:
    case TermsOfServiceScreen::Result::NOT_APPLICABLE:
      if (wizard_context_->screen_after_managed_tos ==
          ash::OOBE_SCREEN_UNKNOWN) {
        OnOobeFlowFinished();
        return;
      }
      AdvanceToScreen(wizard_context_->screen_after_managed_tos);
      break;
    case TermsOfServiceScreen::Result::DECLINED:
      // End the session and return to the login screen.
      SessionManagerClient::Get()->StopSession(
          login_manager::SessionStopReason::TERMS_DECLINED);
      break;
  }
}

void WizardController::OnFamilyLinkNoticeScreenExit(
    FamilyLinkNoticeScreen::Result result) {
  OnScreenExit(FamilyLinkNoticeView::kScreenId,
               FamilyLinkNoticeScreen::GetResultString(result));

  ShowEduCoexistenceLoginScreen();
}

void WizardController::OnSyncConsentScreenExit(
    SyncConsentScreen::Result result) {
  OnScreenExit(SyncConsentScreenView::kScreenId,
               SyncConsentScreen::GetResultString(result));
  if (switches::IsRevenBranding()) {
    AdvanceToScreen(HWDataCollectionView::kScreenId);
    return;
  }

  StartAuthFactorsSetup();
}

// Start of local authentication setup screen exit handlers.

void WizardController::OnCryptohomeRecoverySetupScreenExit(
    CryptohomeRecoverySetupScreen::Result result) {
  OnScreenExit(CryptohomeRecoverySetupScreenView::kScreenId,
               CryptohomeRecoverySetupScreen::GetResultString(result));
  if (ash::switches::IsOobePinOnlyPrototypeEnabled()) {
    // First step of the AuthFactor setup flow. Offer PIN as a main factor. If
    // there isn't hardware support, the screen exits gracefully.
    CHECK(!wizard_context_->knowledge_factor_setup.pin_setup_mode.has_value());
    wizard_context_->knowledge_factor_setup.pin_setup_mode =
        WizardContext::PinSetupMode::kSetupAsPrimaryFactor;
    ShowPinSetupScreen();
  } else {
    ShowPasswordSelectionScreen();
  }
}

void WizardController::OnPasswordSelectionScreenExit(
    PasswordSelectionScreen::Result result) {
  OnScreenExit(PasswordSelectionScreenView::kScreenId,
               PasswordSelectionScreen::GetResultString(result));
  switch (result) {
    // TODO(b/291808449): add an edge case for Enterprise users
    // without GAIA/SAML password.
    case PasswordSelectionScreen::Result::NOT_APPLICABLE:
      ShowFingerprintSetupScreen();
      return;
    case PasswordSelectionScreen::Result::BACK: {
      // TODO(b/291808449): It should not be possible to go back
      const bool did_go_back = MaybeSetToPreviousScreen();
      DCHECK(did_go_back);
      return;
    }
    case PasswordSelectionScreen::Result::LOCAL_PASSWORD_CHOICE:
    case PasswordSelectionScreen::Result::LOCAL_PASSWORD_FORCED:
      ShowLocalPasswordSetupScreen();
      return;
    case PasswordSelectionScreen::Result::GAIA_PASSWORD_CHOICE:
    case PasswordSelectionScreen::Result::GAIA_PASSWORD_FALLBACK:
    case PasswordSelectionScreen::Result::GAIA_PASSWORD_ENTERPRISE:
      ShowApplyOnlinePasswordScreen();
      return;
  }
}

void WizardController::OnLocalPasswordSetupScreenExit(
    LocalPasswordSetupScreen::Result result) {
  OnScreenExit(LocalPasswordSetupView::kScreenId,
               LocalPasswordSetupScreen::GetResultString(result));
  switch (result) {
    case LocalPasswordSetupScreen::Result::kBack:
      ShowPasswordSelectionScreen();
      return;
    case LocalPasswordSetupScreen::Result::kDone:
    case LocalPasswordSetupScreen::Result::kNotApplicable:
      ShowFactorSetupSuccessScreen();
      return;
  }
}

void WizardController::OnApplyOnlinePasswordScreenExit(
    ApplyOnlinePasswordScreen::Result result) {
  OnScreenExit(ApplyOnlinePasswordScreenView::kScreenId,
               ApplyOnlinePasswordScreen::GetResultString(result));
  switch (result) {
    case ApplyOnlinePasswordScreen::Result::kError:
      ShowOSAuthErrorScreen();
      return;
    case ApplyOnlinePasswordScreen::Result::kSuccess:
    case ApplyOnlinePasswordScreen::Result::kNotApplicable: {
      switch (wizard_context_->knowledge_factor_setup.auth_setup_flow) {
        case WizardContext::AuthChangeFlow::kInitialSetup:
          ShowFingerprintSetupScreen();
          return;
        case WizardContext::AuthChangeFlow::kRecovery:
          ShowFactorSetupSuccessScreen();
          return;
        case WizardContext::AuthChangeFlow::kReauthentication:
          NOTREACHED_IN_MIGRATION()
              << "Reauthentication should have been switched to "
                 "Recovery if there was password update";
      }
    }
      return;
  }
}

void WizardController::OnOSAuthErrorScreenExit(
    OSAuthErrorScreen::Result result) {
  OnScreenExit(OSAuthErrorScreenView::kScreenId,
               OSAuthErrorScreen::GetResultString(result));
  switch (result) {
    case OSAuthErrorScreen::Result::kFallbackOnline:
      ShowEnterOldPasswordScreen();
      break;
    case OSAuthErrorScreen::Result::kFallbackLocal: {
      switch (wizard_context_->knowledge_factor_setup.auth_setup_flow) {
        case WizardContext::AuthChangeFlow::kInitialSetup:
          NOTREACHED_IN_MIGRATION()
              << "Recovery is not used during initial setup";
          return;
        case WizardContext::AuthChangeFlow::kReauthentication:
          AttemptLocalAuthenticationWithContext(
              std::move(wizard_context_->user_context));
          return;
        case WizardContext::AuthChangeFlow::kRecovery:
          // Recovery flow means that the user forgot their
          // local password. It does not make sense to ask for it.
          ShowLocalDataLossWarningScreen();
          return;
      }
    }

    case OSAuthErrorScreen::Result::kProceedAuthenticated: {
      switch (wizard_context_->knowledge_factor_setup.auth_setup_flow) {
        case WizardContext::AuthChangeFlow::kInitialSetup:
        case WizardContext::AuthChangeFlow::kRecovery:
          ShowPasswordSelectionScreen();
          return;
        case WizardContext::AuthChangeFlow::kReauthentication:
          // Proceed with login
          ObtainContextAndLoginAuthenticated();
          return;
      }
    }
      return;
    case OSAuthErrorScreen::Result::kAbortSignin:
      ShowLoginScreen();
      return;
  }
}

void WizardController::OnFactorSetupSuccessScreenExit(
    FactorSetupSuccessScreen::Result result) {
  OnScreenExit(FactorSetupSuccessScreenView::kScreenId,
               FactorSetupSuccessScreen::GetResultString(result));
  switch (result) {
    case FactorSetupSuccessScreen::Result::kNotApplicable:
    case FactorSetupSuccessScreen::Result::kProceed: {
      switch (wizard_context_->knowledge_factor_setup.auth_setup_flow) {
        case WizardContext::AuthChangeFlow::kInitialSetup:
          ShowFingerprintSetupScreen();
          return;
        case WizardContext::AuthChangeFlow::kRecovery:
        case WizardContext::AuthChangeFlow::kReauthentication:
          // Proceed with login
          ObtainContextAndLoginAuthenticated();
          return;
      }
    }
    case FactorSetupSuccessScreen::Result::kTimedOut:
      ShowLoginScreen();
      return;
  }
}

void WizardController::OnFingerprintSetupScreenExit(
    FingerprintSetupScreen::Result result) {
  OnScreenExit(FingerprintSetupScreenView::kScreenId,
               FingerprintSetupScreen::GetResultString(result));
  if (!ash::switches::IsOobePinOnlyPrototypeEnabled()) {
    // First time surfacing the screen for the non PIN-only OOBE.
    CHECK(!wizard_context_->knowledge_factor_setup.pin_setup_mode.has_value());
    wizard_context_->knowledge_factor_setup.pin_setup_mode =
        WizardContext::PinSetupMode::kSetupAsSecondaryFactor;
  }
  ShowPinSetupScreen();
}

void WizardController::OnPinSetupScreenExit(PinSetupScreen::Result result) {
  OnScreenExit(PinSetupScreenView::kScreenId,
               PinSetupScreen::GetResultString(result));
  if (ash::switches::IsOobePinOnlyPrototypeEnabled()) {
    switch (result) {
      // Possible exit results when the PIN screen is shown for PIN-only setup.
      case PinSetupScreen::Result::kNotApplicableAsPrimaryFactor:
      case PinSetupScreen::Result::kUserChosePassword:
        // PIN as a main factor is not supported or not wanted, it will be
        // offered later again as a secondary factor after the fingerprint setup
        // screen.
        wizard_context_->knowledge_factor_setup.pin_setup_mode =
            WizardContext::PinSetupMode::kSetupAsSecondaryFactor;
        ShowPasswordSelectionScreen();
        break;
      case PinSetupScreen::Result::kDoneAsMainFactor:
        wizard_context_->knowledge_factor_setup.pin_setup_mode =
            WizardContext::PinSetupMode::kAlreadyPerformed;
        ShowFingerprintSetupScreen();
        break;
      // These are emitted when the screen is surfaced at the end of the flow,
      // offering PIN as an additional factor.
      case PinSetupScreen::Result::kDoneAsSecondaryFactor:
      case PinSetupScreen::Result::kUserSkip:
      case PinSetupScreen::Result::kNotApplicable:
      case PinSetupScreen::Result::kTimedOut:
        FinishAuthFactorsSetup();
        break;
    }
  } else {
    FinishAuthFactorsSetup();
  }
}

void WizardController::ObtainContextAndLoginAuthenticated() {
  CHECK(wizard_context_->extra_factors_token);
  auto token = std::move(wizard_context_->extra_factors_token);
  wizard_context_->extra_factors_token = std::nullopt;

  ash::AuthSessionStorage::Get()->Withdraw(
      *token, base::BindOnce(&WizardController::LoginAuthenticatedWithContext,
                             weak_factory_.GetWeakPtr()));
}

void WizardController::ObtainContextAndAttemptLocalAuthentication() {
  CHECK(wizard_context_->extra_factors_token);
  auto token = std::move(wizard_context_->extra_factors_token);
  wizard_context_->extra_factors_token = std::nullopt;

  ash::AuthSessionStorage::Get()->Withdraw(
      *token,
      base::BindOnce(&WizardController::AttemptLocalAuthenticationWithContext,
                     weak_factory_.GetWeakPtr()));
}

void WizardController::LoginAuthenticatedWithContext(
    std::unique_ptr<UserContext> context) {
  if (!context) {
    // Session has expired.
    LOG(ERROR) << "Session expired before login could proceed.";
    wizard_context_->osauth_error = WizardContext::OSAuthErrorKind::kFatal;
    ShowOSAuthErrorScreen();
    return;
  }
  ash::LoginDisplayHost::default_host()
      ->GetExistingUserController()
      ->LoginAuthenticated(std::move(context));
}

void WizardController::AttemptLocalAuthenticationWithContext(
    std::unique_ptr<UserContext> context) {
  ash::LoginDisplayHost::default_host()->GetSigninUI()->RunLocalAuthentication(
      std::move(context));
}

void WizardController::FinishAuthFactorsSetup() {
  // TODO(crbug.com/372213353): Ensure that AuthSession is terminated after this
  // step.
  if (wizard_context_->extra_factors_token.has_value()) {
    ash::AuthSessionStorage::Get()->Invalidate(
        wizard_context_->extra_factors_token.value(), base::DoNothing());
    wizard_context_->extra_factors_token = std::nullopt;
  }

  if (features::IsOobePersonalizedOnboardingEnabled()) {
    ShowCategoriesSelectionScreen();
  } else {
    ShowRecommendAppsScreen();
  }
}

// End of local authentication setup screen exit handlers.

void WizardController::OnRecommendAppsScreenExit(
    RecommendAppsScreen::Result result) {
  OnScreenExit(RecommendAppsScreenView::kScreenId,
               RecommendAppsScreen::GetResultString(result));

  switch (result) {
    case RecommendAppsScreen::Result::kSelected:
      ShowAppDownloadingScreen();
      break;
    case RecommendAppsScreen::Result::kSkipped:
    case RecommendAppsScreen::Result::kNotApplicable:
    case RecommendAppsScreen::Result::kLoadError:
        ShowPerksDiscoveryScreen();
      break;
  }
}

void WizardController::OnRemoteActivityNotificationScreenExit() {
  // Remember the user acknowledged the message.
  GetLocalState()->SetBoolean(::prefs::kRemoteAdminWasPresent, false);

  // Check if there are any local accounts present for the lock screen which
  // suggest that the OOBE flow was completed and the dialog should be hidden.
  if (LoginDisplayHost::default_host()->HasUserPods()) {
    LoginDisplayHost::default_host()->HideOobeDialog();
    return;
  }

  // When, there is no local accounts present and the OOBE flow must be
  // continued. Hence, we go back to the previous screen present in the flow.
  bool switched_screen = MaybeSetToPreviousScreen();
  CHECK(switched_screen);
}

void WizardController::OnAppDownloadingScreenExit() {
  OnScreenExit(AppDownloadingScreenView::kScreenId, kDefaultExitReason);
  ShowPerksDiscoveryScreen();
}

void WizardController::OnAiIntroScreenExit(AiIntroScreen::Result result) {
  OnScreenExit(AiIntroScreenView::kScreenId,
               AiIntroScreen::GetResultString(result));

  if (features::IsOobeGeminiIntroEnabled()) {
    ShowGeminiIntroScreen();
  } else {
    ShowAssistantOptInFlowScreen();
  }
}

void WizardController::OnGeminiIntroScreenExit(
    GeminiIntroScreen::Result result) {
  OnScreenExit(GeminiIntroScreenView::kScreenId,
               GeminiIntroScreen::GetResultString(result));

  if (result == GeminiIntroScreen::Result::kBack) {
    CHECK(!AiIntroScreen::ShouldBeSkipped());
    ShowAiIntroScreen();
    return;
  }

  ShowAssistantOptInFlowScreen();
}

void WizardController::OnAssistantOptInFlowScreenExit(
    AssistantOptInFlowScreen::Result result) {
  OnScreenExit(AssistantOptInFlowScreenView::kScreenId,
               AssistantOptInFlowScreen::GetResultString(result));
  AdvanceToScreen(SmartPrivacyProtectionView::kScreenId);
}

void WizardController::OnMultiDeviceSetupScreenExit(
    MultiDeviceSetupScreen::Result result) {
  OnScreenExit(MultiDeviceSetupScreenView::kScreenId,
               MultiDeviceSetupScreen::GetResultString(result));

  ShowSplitModifierKeyboardInfoScreen();
}

void WizardController::OnGestureNavigationScreenExit(
    GestureNavigationScreen::Result result) {
  OnScreenExit(GestureNavigationScreenView::kScreenId,
               GestureNavigationScreen::GetResultString(result));
  if (features::IsOobeChoobeEnabled()) {
    ShowChoobeScreen();
  } else {
    ShowThemeSelectionScreen();
  }
}

void WizardController::OnMarketingOptInScreenExit(
    MarketingOptInScreen::Result result) {
  OnScreenExit(MarketingOptInScreenView::kScreenId,
               MarketingOptInScreen::GetResultString(result));

  OnOobeFlowFinished();
}

void WizardController::OnResetScreenExit() {
  OnScreenExit(ResetView::kScreenId, kResetScreenExitReason);
  OnDeviceModificationCanceled();
}

void WizardController::OnDeviceModificationCanceled() {
  BaseScreen* previous_screen = nullptr;
  if (base::Contains(previous_screens_, current_screen_)) {
    previous_screen = previous_screens_[current_screen_];
  }

  ResetCurrentScreen();

  if (previous_screen) {
    if (IsSigninScreen(previous_screen->screen_id())) {
      ShowLoginScreen();
    } else {
      SetCurrentScreen(previous_screen);
    }
    return;
  }

  // No previous screen found. Most likely the device is owned,
  // in which case the login screen is the default.
  if (ash::InstallAttributes::Get()->IsDeviceLocked()) {
    ShowLoginScreen();
    return;
  }

  LOG(WARNING) << "No previous screen on unowned device";
  if (prescribed_enrollment_config_.should_enroll()) {
    ShowPackagedLicenseScreen();
  } else {
    ShowLoginScreen();
  }
}

void WizardController::OnManagementTransitionScreenExit() {
  OnScreenExit(ManagementTransitionScreenView::kScreenId, kDefaultExitReason);

  OnOobeFlowFinished();
}

void WizardController::OnUpdateRequiredScreenExit() {
  ResetCurrentScreen();
  ShowLoginScreen();
}

void WizardController::OnPackagedLicenseScreenExit(
    PackagedLicenseScreen::Result result) {
  OnScreenExit(PackagedLicenseView::kScreenId,
               PackagedLicenseScreen::GetResultString(result));
  switch (result) {
    case PackagedLicenseScreen::Result::DONT_ENROLL:
    case PackagedLicenseScreen::Result::NOT_APPLICABLE:
      ShowLoginScreen();
      break;
    case PackagedLicenseScreen::Result::ENROLL:
    case PackagedLicenseScreen::Result::NOT_APPLICABLE_SKIP_TO_ENROLL:
      ShowEnrollmentScreen();
      break;
  }
}

void WizardController::OnAppLaunchSplashScreenExit() {
  // TODO(b/343483938): Exit AppLaunchSplashScreen before launching the app.
  NOTIMPLEMENTED();
}

void WizardController::OnOobeFlowFinished() {
  if (GetLoginDisplayHost()
          ->GetWizardContext()
          ->defer_oobe_flow_finished_for_tests) {
    return;
  }
  SetCurrentScreen(nullptr);

  user_manager::KnownUser known_user(GetLocalState());
  const AccountId account_id =
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();
  known_user.SetOnboardingCompletedVersion(account_id,
                                           version_info::GetVersion());
  known_user.RemovePendingOnboardingScreen(account_id);

  PrefService* active_user_prefs =
      ProfileManager::GetActiveUserProfile()->GetPrefs();
  if (features::IsOobeChoobeEnabled()) {
    // Additional cleanup of CHOOBE prefs in case it was not already cleared.
    active_user_prefs->ClearPref(prefs::kChoobeSelectedScreens);
    active_user_prefs->ClearPref(prefs::kChoobeCompletedScreens);
  }

  GetLoginDisplayHost()->GetOobeMetricsHelper()->RecordOnboadingComplete(
      GetLocalState()->GetTime(prefs::kOobeStartTime),
      active_user_prefs->GetTime(prefs::kOobeOnboardingTime));

  GetLocalState()->ClearPref(prefs::kOobeStartTime);

  GetLocalState()->ClearPref(prefs::kOobeMetricsClientIdAtOobeStart);
  GetLocalState()->ClearPref(prefs::kOobeMetricsReportedAsEnabled);
  GetLocalState()->ClearPref(prefs::kOobeStatsReportingControllerReportedReset);

  // Launch browser and delete login host controller.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &UserSessionManager::DoBrowserLaunch,
          UserSessionManager::GetInstance()->GetUserSessionManagerAsWeakPtr(),
          ProfileManager::GetActiveUserProfile()));
}

void WizardController::OnDeviceDisabledChecked(bool device_disabled) {
  prescribed_enrollment_config_ =
      policy::EnrollmentConfig::GetPrescribedEnrollmentConfig();

  if (device_disabled) {
    demo_setup_controller_.reset();
    ShowDeviceDisabledScreen();
  } else if (demo_setup_controller_) {
    ShowDemoModeSetupScreen();
  } else if (wizard_context_->enrollment_triggered_early ||
             prescribed_enrollment_config_.should_enroll()) {
    VLOG(1) << "StartEnrollment from OnDeviceDisabledChecked("
            << "device_disabled=" << device_disabled << ") "
            << "skip_update_enroll_after_eula_="
            << wizard_context_->enrollment_triggered_early
            << ", prescribed_enrollment_config_.should_enroll()="
            << prescribed_enrollment_config_.should_enroll();
    StartEnrollmentScreen();
  } else {
    PerformOOBECompletedActions(
        OobeMetricsHelper::CompletedPreLoginOobeFlowType::kRegular);
    ShowPackagedLicenseScreen();
  }
}

void WizardController::InitiateOOBEUpdate() {
  // If this is a Cellular First device, instruct UpdateEngine to allow
  // updates over cellular data connections.
  if (switches::IsCellularFirstDevice()) {
    UpdateEngineClient::Get()->SetUpdateOverCellularPermission(
        true, base::BindOnce(&WizardController::StartOOBEUpdate,
                             weak_factory_.GetWeakPtr()));
  } else {
    StartOOBEUpdate();
  }
}

void WizardController::StartOOBEUpdate() {
  SetCurrentScreen(GetScreen(UpdateView::kScreenId));
}

void WizardController::StartNetworkTimezoneResolve() {
  // Bypass the network requests for the geolocation and the timezone if the
  // timezone is being overridden through the command line.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOobeTimezoneOverrideForTests)) {
    auto timezone = std::make_unique<TimeZoneResponseData>();
    timezone->status = TimeZoneResponseData::OK;
    timezone->timeZoneId =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kOobeTimezoneOverrideForTests);
    VLOG(1) << "Timezone is being overridden with : " << timezone->timeZoneId;
    OnTimezoneResolved(std::move(timezone), /*server_error*/ false);
    return;
  }

  DelayNetworkCall(base::BindOnce(&WizardController::StartTimezoneResolve,
                                  weak_factory_.GetWeakPtr()));
}

// Resolving the timezone consists of first determining the location,
// and then determining the timezone.
void WizardController::StartTimezoneResolve() {
  if (!g_browser_process->platform_part()
           ->GetTimezoneResolverManager()
           ->TimeZoneResolverShouldBeRunning()) {
    return;
  }

  SimpleGeolocationProvider::GetInstance()->RequestGeolocation(
      base::Seconds(kResolveTimeZoneTimeoutSeconds),
      false /* send_wifi_geolocation_data */,
      false /* send_cellular_geolocation_data */,
      base::BindOnce(&WizardController::OnLocationResolved,
                     weak_factory_.GetWeakPtr()),
      SimpleGeolocationProvider::ClientId::kWizardController);
}

void WizardController::PerformPostNetworkScreenActions() {
  StartNetworkTimezoneResolve();
  DelayNetworkCall(ServicesCustomizationDocument::GetInstance()
                       ->EnsureCustomizationAppliedClosure());
  GetAutoEnrollmentController()->Start();
}

void WizardController::PerformOOBECompletedActions(
    OobeMetricsHelper::CompletedPreLoginOobeFlowType flow_type) {
  // Avoid marking OOBE as completed multiple times if going from login screen
  // to enrollment screen (and back).
  if (StartupUtils::IsOobeCompleted()) {
    return;
  }

  StartupUtils::MarkOobeCompleted();
  GetLoginDisplayHost()->GetOobeMetricsHelper()->RecordPreLoginOobeComplete(
      flow_type);

  // Triggers DLC installation once OOBE is complete.
  cros_healthd::internal::TriggerDlcInstall();
}

void WizardController::SetCurrentScreen(BaseScreen* new_current) {
  VLOG(1) << "SetCurrentScreen: "
          << (new_current ? new_current->screen_id().name : "null");

  if (new_current && new_current->MaybeSkip(*wizard_context_)) {
    // choobe_flow_controller_ lives only while CHOOBE flow is active, metrics
    // regarding screens shown while CHOOBE is active is handled by
    // choobe_flow_controller_.
    if (features::IsOobeChoobeEnabled()) {
      if (ChoobeFlowController::IsOptionalScreen(new_current->screen_id()) &&
          choobe_flow_controller_) {
        return;
      }
    }

    GetLoginDisplayHost()->GetOobeMetricsHelper()->RecordScreenShownStatus(
        new_current->screen_id(),
        OobeMetricsHelper::ScreenShownStatus::kSkipped);
    return;
  }

  if (current_screen_ == new_current || GetOobeUI() == nullptr) {
    return;
  }

  // Check if we didn't come here via the previous screen logic.
  if (current_screen_ && new_current &&
      (!base::Contains(previous_screens_, current_screen_) ||
       previous_screens_[current_screen_] != new_current)) {
    previous_screens_[new_current] = current_screen_;
  }

  ResetCurrentScreen();

  current_screen_ = new_current;

  if (!current_screen_) {
    NotifyScreenChanged();
    return;
  }

  // First remember how far have we reached so that we can resume if needed.
  if (!demo_setup_controller_) {
    if (!wizard_context_->is_add_person_flow &&
        IsResumableOobeScreen(current_screen_->screen_id())) {
      StartupUtils::SaveOobePendingScreen(current_screen_->screen_id().name);
    } else if (IsResumablePostLoginScreen(current_screen_->screen_id()) &&
               !wizard_context_->is_cloud_ready_update_flow &&
               wizard_context_->screen_after_managed_tos !=
                   ash::OOBE_SCREEN_UNKNOWN) {
      // If screen_after_managed_tos == SCREEN_UNKNOWN means that the
      // onboarding has already been finished by the user and we don't need to
      // save the state here.
      user_manager::KnownUser(GetLocalState())
          .SetPendingOnboardingScreen(
              user_manager::UserManager::Get()->GetActiveUser()->GetAccountId(),
              current_screen_->screen_id().name);
    }
  }

  UpdateStatusAreaVisibilityForScreen(current_screen_->screen_id());
  GetLoginDisplayHost()->GetOobeMetricsHelper()->RecordScreenShownStatus(
      current_screen_->screen_id(),
      OobeMetricsHelper::ScreenShownStatus::kShown);
  current_screen_->Show(wizard_context_);
  NotifyScreenChanged();
}

void WizardController::UpdateStatusAreaVisibilityForScreen(
    OobeScreenId screen_id) {
  // SystemTrayClientImpl::Get() can be nullptr in unit tests.
  if (SystemTrayClientImpl::Get()) {
    SystemTrayClientImpl::Get()->SetPrimaryTrayVisible(
        !ShouldHideStatusArea(screen_id));
  }
}

void WizardController::OnHIDScreenNecessityCheck(bool screen_needed) {
  if (!GetOobeUI()) {
    return;
  }

  // Check for tests configurations.
  if (wizard_context_->skip_to_update_for_tests ||
      wizard_context_->skip_to_login_for_tests || current_screen_) {
    return;
  }

  if (screen_needed) {
    ShowHIDDetectionScreen();
  } else {
    AdvanceToScreenAfterHIDDetection(ash::OOBE_SCREEN_UNKNOWN);
  }
}

void WizardController::UpdateOobeConfiguration() {
  wizard_context_->configuration = configuration::FilterConfiguration(
      OobeConfiguration::Get()->configuration(),
      configuration::ConfigurationHandlerSide::HANDLER_CPP);
  auto* requisition_value = wizard_context_->configuration.FindString(
      configuration::kDeviceRequisition);

  if (requisition_value) {
    VLOG(1) << "Using Device Requisition from configuration"
            << *requisition_value;
    policy::EnrollmentRequisitionManager::SetDeviceRequisition(
        *requisition_value);
  } else if (policy::EnrollmentRequisitionManager::IsCuttlefishDevice()) {
    VLOG(1) << "Using default Device Requisition value for Cuttlefish build "
               "configuration"
            << policy::EnrollmentRequisitionManager::kCuttlefishRequisition;
    policy::EnrollmentRequisitionManager::SetDeviceRequisition(
        policy::EnrollmentRequisitionManager::kCuttlefishRequisition);
  } else if (policy::EnrollmentRequisitionManager::IsMeetDevice()) {
    VLOG(1) << "Using default Device Requisition value for CFM build "
               "configuration"
            << policy::EnrollmentRequisitionManager::kRemoraRequisition;
    policy::EnrollmentRequisitionManager::SetDeviceRequisition(
        policy::EnrollmentRequisitionManager::kRemoraRequisition);
  }

  auto* network_config =
      wizard_context_->configuration.FindString(configuration::kNetworkConfig);
  if (network_config) {
    auto rollback_network_config = std::make_unique<
        mojo::Remote<rollback_network_config::mojom::RollbackNetworkConfig>>();
    rollback_network_config::BindToInProcessInstance(
        rollback_network_config->BindNewPipeAndPassReceiver());
    rollback_network_config->get()->RollbackConfigImport(*network_config,
                                                         base::DoNothing());
  }
}

bool WizardController::CanNavigateTo(OobeScreenId screen_id) {
  if (!current_screen_) {
    return true;
  }

  if (wizard_context_->skip_to_login_for_tests) {
    VLOG(1) << "CanNavigateTo to screen " << screen_id
            << " returns true because skip_to_login_for_tests is set to true";
    return true;
  }

  BaseScreen* next_screen = GetScreen(screen_id);
  return next_screen->screen_priority() <= current_screen_->screen_priority();
}

void WizardController::AdvanceToScreen(OobeScreenId screen_id) {
  VLOG(1) << "AdvanceToScreen " << screen_id;
  if (!CanNavigateTo(screen_id)) {
    LOG(WARNING) << "Cannot advance to screen : " << screen_id
                 << " as it's priority is less than the current screen : "
                 << current_screen_->screen_id();
    return;
  }

  if (screen_id == WelcomeView::kScreenId) {
    ShowWelcomeScreen();
  } else if (screen_id == NetworkScreenView::kScreenId) {
    ShowNetworkScreen();
  } else if (screen_id == PackagedLicenseView::kScreenId) {
    ShowPackagedLicenseScreen();
  } else if (screen_id == UpdateView::kScreenId) {
    InitiateOOBEUpdate();
  } else if (screen_id == ResetView::kScreenId) {
    ShowResetScreen();
  } else if (screen_id == EnableAdbSideloadingScreenView::kScreenId) {
    ShowEnableAdbSideloadingScreen();
  } else if (screen_id == EnableDebuggingScreenView::kScreenId) {
    ShowEnableDebuggingScreen();
  } else if (screen_id == EnrollmentScreenView::kScreenId) {
    ShowEnrollmentScreen();
  } else if (screen_id == DemoSetupScreenView::kScreenId) {
    ShowDemoModeSetupScreen();
  } else if (screen_id == DemoPreferencesScreenView::kScreenId) {
    ShowDemoModePreferencesScreen();
  } else if (screen_id == TermsOfServiceScreenView::kScreenId) {
    ShowTermsOfServiceScreen();
  } else if (screen_id == SyncConsentScreenView::kScreenId) {
    ShowSyncConsentScreen();
  } else if (screen_id == RecommendAppsScreenView::kScreenId) {
    ShowRecommendAppsScreen();
  } else if (screen_id == RemoteActivityNotificationView::kScreenId) {
    ShowRemoteActivityNotificationScreen();
  } else if (screen_id == AppDownloadingScreenView::kScreenId) {
    ShowAppDownloadingScreen();
  } else if (screen_id == AiIntroScreenView::kScreenId) {
    ShowAiIntroScreen();
  } else if (screen_id == GeminiIntroScreenView::kScreenId) {
    ShowGeminiIntroScreen();
  } else if (screen_id == WrongHWIDScreenView::kScreenId) {
    ShowWrongHWIDScreen();
  } else if (screen_id == AutoEnrollmentCheckScreenView::kScreenId) {
    ShowAutoEnrollmentCheckScreen();
  } else if (screen_id == AppLaunchSplashScreenView::kScreenId) {
    ShowAppLaunchSplashScreen();
  } else if (screen_id == HIDDetectionView::kScreenId) {
    ShowHIDDetectionScreen();
  } else if (screen_id == DeviceDisabledScreenView::kScreenId) {
    ShowDeviceDisabledScreen();
  } else if (screen_id == EncryptionMigrationScreenView::kScreenId) {
    ShowEncryptionMigrationScreen();
  } else if (screen_id == UpdateRequiredView::kScreenId) {
    ShowUpdateRequiredScreen();
  } else if (screen_id == AssistantOptInFlowScreenView::kScreenId) {
    ShowAssistantOptInFlowScreen();
  } else if (screen_id == MultiDeviceSetupScreenView::kScreenId) {
    ShowMultiDeviceSetupScreen();
  } else if (screen_id == GestureNavigationScreenView::kScreenId) {
    ShowGestureNavigationScreen();
  } else if (screen_id == PinSetupScreenView::kScreenId) {
    ShowPinSetupScreen();
  } else if (screen_id == FingerprintSetupScreenView::kScreenId) {
    ShowFingerprintSetupScreen();
  } else if (screen_id == MarketingOptInScreenView::kScreenId) {
    ShowMarketingOptInScreen();
  } else if (screen_id == ManagementTransitionScreenView::kScreenId) {
    ShowManagementTransitionScreen();
  } else if (screen_id == GuestTosScreenView::kScreenId) {
    ShowGuestTosScreen();
  } else if (screen_id == ConsolidatedConsentScreenView::kScreenId) {
    ShowConsolidatedConsentScreen();
  } else if (screen_id == CryptohomeRecoverySetupScreenView::kScreenId) {
    ShowCryptohomeRecoverySetupScreen();
  } else if (screen_id == ArcVmDataMigrationScreenView::kScreenId) {
    ShowArcVmDataMigrationScreen();
  } else if (screen_id == TouchpadScrollScreenView::kScreenId) {
    ShowTouchpadScrollScreen();
  } else if (screen_id == GaiaInfoScreenView::kScreenId) {
    ShowGaiaInfoScreen();
  } else if (screen_id == DrivePinningScreenView::kScreenId) {
    ShowDrivePinningScreen();
  } else if (screen_id == DisplaySizeScreenView::kScreenId) {
    ShowDisplaySizeScreen();
  } else if (screen_id == ChoobeScreenView::kScreenId) {
    ShowChoobeScreen();
  } else if (screen_id == AddChildScreenView::kScreenId) {
    ShowAddChildScreen();
  } else if (screen_id == ConsumerUpdateScreenView::kScreenId) {
    ShowConsumerUpdateScreen();
  } else if (screen_id == PasswordSelectionScreenView::kScreenId) {
    ShowPasswordSelectionScreen();
  } else if (screen_id == ApplyOnlinePasswordScreenView::kScreenId) {
    ShowApplyOnlinePasswordScreen();
  } else if (screen_id == OSAuthErrorScreenView::kScreenId) {
    ShowOSAuthErrorScreen();
  } else if (screen_id == FactorSetupSuccessScreenView::kScreenId) {
    ShowFactorSetupSuccessScreen();
  } else if (screen_id == LocalPasswordSetupView::kScreenId) {
    ShowLocalPasswordSetupScreen();
  } else if (screen_id == CategoriesSelectionScreenView::kScreenId) {
    ShowCategoriesSelectionScreen();
  } else if (screen_id == PersonalizedRecommendAppsScreenView::kScreenId) {
    ShowPersonalizedRecomendAppsScreen();
  } else if (screen_id == PerksDiscoveryScreenView::kScreenId) {
    ShowPerksDiscoveryScreen();
  } else if (screen_id == SplitModifierKeyboardInfoScreenView::kScreenId) {
    ShowSplitModifierKeyboardInfoScreen();
  } else if (screen_id == TpmErrorView::kScreenId ||
             screen_id == InstallAttributesErrorView::kScreenId ||
             screen_id == FamilyLinkNoticeView::kScreenId ||
             screen_id == GaiaView::kScreenId ||
             screen_id == UserCreationView::kScreenId ||
             screen_id == SignInFatalErrorView::kScreenId ||
             screen_id == UserAllowlistCheckScreenView::kScreenId ||
             screen_id == LocaleSwitchView::kScreenId ||
             screen_id == RecoveryEligibilityView::kScreenId ||
             screen_id == OfflineLoginView::kScreenId ||
             screen_id == OsInstallScreenView::kScreenId ||
             screen_id == OsTrialScreenView::kScreenId ||
             screen_id == ParentalHandoffScreenView::kScreenId ||
             screen_id == HWDataCollectionView::kScreenId ||
             screen_id == SmartPrivacyProtectionView::kScreenId ||
             screen_id == ThemeSelectionScreenView::kScreenId ||
             screen_id == SamlConfirmPasswordView::kScreenId ||
             screen_id == LocalStateErrorScreenView::kScreenId ||
             screen_id == QuickStartView::kScreenId) {
    SetCurrentScreen(GetScreen(screen_id));
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

bool WizardController::HandleAccelerator(LoginAcceleratorAction action) {
  return current_screen_ && current_screen_->HandleAccelerator(action);
}

void WizardController::StartDemoModeSetup() {
  // Start Demo Mode by initiate demo set up controller and showing the first
  // network screen in demo mode setup flow.
  demo_setup_controller_ = std::make_unique<DemoSetupController>();
  ShowNetworkScreen();
}

void WizardController::CreateChoobeFlowController() {
  choobe_flow_controller_ = std::make_unique<ChoobeFlowController>();
}

void WizardController::SimulateDemoModeSetupForTesting(
    std::optional<DemoSession::DemoModeConfig> demo_config) {
  if (!demo_setup_controller_) {
    demo_setup_controller_ = std::make_unique<DemoSetupController>();
  }
  if (demo_config.has_value()) {
    demo_setup_controller_->set_demo_config(*demo_config);
  }
}

void WizardController::OnAccessibilityStatusChanged(
    const AccessibilityStatusEventDetails& details) {
  enum AccessibilityNotificationType type = details.notification_type;
  if (type == AccessibilityNotificationType::kManagerShutdown) {
    accessibility_subscription_ = {};
    return;
  } else if (type != AccessibilityNotificationType::kToggleSpokenFeedback ||
             !details.enabled) {
    return;
  }

  CrasAudioHandler* cras = CrasAudioHandler::Get();
  if (cras->IsOutputMuted()) {
    cras->SetOutputMute(false);
    cras->SetOutputVolumePercent(kMinAudibleOutputVolumePercent);
  } else if (cras->GetOutputVolumePercent() < kMinAudibleOutputVolumePercent) {
    cras->SetOutputVolumePercent(kMinAudibleOutputVolumePercent);
  }
}

void WizardController::AutoLaunchKioskApp(const KioskApp& app) {
  // Wait until `CrosSettings` is either trusted or permanently untrusted.
  const CrosSettingsProvider::TrustedStatus status =
      CrosSettings::Get()->PrepareTrustedValues(
          base::BindOnce(&WizardController::AutoLaunchKioskApp,
                         weak_factory_.GetWeakPtr(), app));
  if (status == CrosSettingsProvider::TEMPORARILY_UNTRUSTED) {
    return;
  }

  if (status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
    // If the `cros_settings_` are permanently untrusted, show an error
    // message and refuse to auto-launch the kiosk app.
    AdvanceToScreen(LocalStateErrorScreenView::kScreenId);
    return;
  }

  if (system::DeviceDisablingManager::IsDeviceDisabledDuringNormalOperation()) {
    // If the device is disabled, bail out. A device disabled screen will be
    // shown by the DeviceDisablingManager.
    return;
  }

  constexpr bool auto_launch = true;
  GetLoginDisplayHost()->StartKiosk(app.id(), auto_launch);
}

// static
void WizardController::SetZeroDelays() {
  g_using_zero_delays = true;
}

// static
bool WizardController::IsZeroDelayEnabled() {
  return g_using_zero_delays;
}

void WizardController::SkipPostLoginScreensForTesting() {
  wizard_context_->skip_post_login_screens_for_tests = true;
  auto* current_screen = default_controller()->current_screen();
  if (current_screen && !current_screen->MaybeSkip(*wizard_context_)) {
    LOG(WARNING) << __func__ << ": Ignore screen "
                 << current_screen->screen_id().name;
  }
}

// statis
bool WizardController::IsResumablePostLoginScreen(OobeScreenId screen_id) {
  for (const auto& resumable_screen : kResumablePostLoginScreens) {
    if (screen_id == resumable_screen) {
      return true;
    }
  }
  return false;
}

// static
void WizardController::SkipEnrollmentPromptsForTesting() {
  skip_enrollment_prompts_for_testing_ = true;
}

// static
bool WizardController::IsSigninScreen(OobeScreenId screen_id) {
  return screen_id == UserCreationView::kScreenId ||
         screen_id == GaiaView::kScreenId ||
         screen_id == SignInFatalErrorView::kScreenId;
}

void WizardController::AddObserver(ScreenObserver* obs) {
  screen_observers_.AddObserver(obs);
}

void WizardController::RemoveObserver(ScreenObserver* obs) {
  screen_observers_.RemoveObserver(obs);
}

void WizardController::OnLocalStateInitialized(bool /* succeeded */) {
  if (GetLocalState()->GetInitializationStatus() !=
      PrefService::INITIALIZATION_STATUS_ERROR) {
    return;
  }
  AdvanceToScreen(LocalStateErrorScreenView::kScreenId);
}

void WizardController::PrepareFirstRunPrefs() {
  // Showoff starts in parallel to OOBE onboarding. We need to store the prefs
  // early to make sure showoff has the correct data when launched.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  bool shouldShowParentalControl =
      wizard_context_->sign_in_as_child && !profile->IsChild() &&
      !profile->GetProfilePolicyConnector()->IsManaged();
  profile->GetPrefs()->SetBoolean(::prefs::kHelpAppShouldShowParentalControl,
                                  shouldShowParentalControl);
}

PrefService* WizardController::GetLocalState() {
  if (local_state_for_testing_) {
    return local_state_for_testing_;
  }
  return g_browser_process->local_state();
}

void WizardController::OnTimezoneResolved(
    std::unique_ptr<TimeZoneResponseData> timezone,
    bool server_error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(timezone);

  timezone_resolved_ = true;
  base::ScopedClosureRunner inform_test(
      std::move(on_timezone_resolved_for_testing_));

  VLOG(1) << "Resolved local timezone={" << timezone->ToStringForDebug()
          << "}.";

  if (timezone->status != TimeZoneResponseData::OK) {
    LOG(WARNING) << "Resolve TimeZone: failed to resolve timezone.";
    return;
  }

  if (ash::InstallAttributes::Get()->IsEnterpriseManaged()) {
    std::string policy_timezone;
    if (CrosSettings::Get()->GetString(kSystemTimezonePolicy,
                                       &policy_timezone) &&
        !policy_timezone.empty()) {
      VLOG(1) << "Resolve TimeZone: TimeZone settings are overridden"
              << " by DevicePolicy.";
      return;
    }
  }

  if (!timezone->timeZoneId.empty()) {
    VLOG(1) << "Resolve TimeZone: setting timezone to '" << timezone->timeZoneId
            << "'";
    system::SetSystemAndSigninScreenTimezone(timezone->timeZoneId);
  }
}

TimeZoneProvider* WizardController::GetTimezoneProvider() {
  if (!timezone_provider_) {
    timezone_provider_ = std::make_unique<TimeZoneProvider>(
        shared_url_loader_factory_, DefaultTimezoneProviderURL());
  }
  return timezone_provider_.get();
}

void WizardController::OnLocationResolved(const Geoposition& position,
                                          bool server_error,
                                          const base::TimeDelta elapsed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const base::TimeDelta timeout = base::Seconds(kResolveTimeZoneTimeoutSeconds);
  // Ignore invalid position.
  if (!position.Valid()) {
    return;
  }

  if (elapsed >= timeout) {
    LOG(WARNING) << "Resolve TimeZone: got location after timeout ("
                 << elapsed.InSecondsF() << " seconds elapsed). Ignored.";
    return;
  }

  if (!g_browser_process->platform_part()
           ->GetTimezoneResolverManager()
           ->TimeZoneResolverShouldBeRunning()) {
    return;
  }

  // WizardController owns TimezoneProvider, so timezone request is silently
  // cancelled on destruction.
  GetTimezoneProvider()->RequestTimezone(
      position, timeout - elapsed,
      base::BindOnce(&WizardController::OnTimezoneResolved,
                     weak_factory_.GetWeakPtr()));
}

bool WizardController::SetOnTimeZoneResolvedForTesting(
    base::OnceClosure callback) {
  if (timezone_resolved_) {
    return false;
  }

  on_timezone_resolved_for_testing_ = std::move(callback);
  return true;
}

void WizardController::StartEnrollmentScreen() {
  VLOG(1) << "Showing enrollment screen.";

  // Determine the effective enrollment configuration. If OOBE Configuration
  // exits, it might also affect enrollment configuration.
  policy::EnrollmentConfig effective_config =
      prescribed_enrollment_config_.GetEffectiveConfig();
  effective_config.enrollment_nudge_email =
      GetScreen<GaiaScreen>()->EnrollmentNudgeEmail();

  EnrollmentScreen* screen = EnrollmentScreen::Get(screen_manager());
  screen->SetEnrollmentConfig(std::move(effective_config));
  UpdateStatusAreaVisibilityForScreen(EnrollmentScreenView::kScreenId);
  SetCurrentScreen(screen);
}

void WizardController::ShowEnrollmentScreenIfEligible() {
  const bool enterprise_managed =
      ash::InstallAttributes::Get()->IsEnterpriseManaged();
  const bool has_users = !user_manager::UserManager::Get()->GetUsers().empty();
  if (!has_users && !enterprise_managed) {
    AdvanceToScreen(EnrollmentScreenView::kScreenId);
  }
}

bool WizardController::MaybeSetToPreviousScreen() {
  DCHECK(current_screen_);
  if (!base::Contains(previous_screens_, current_screen_)) {
    return false;
  }
  auto* old_current_screen = current_screen_.get();
  auto* previous_screen = previous_screens_[current_screen_].get();

  if (previous_screen->screen_id() == GaiaView::kScreenId) {
    wizard_context_->gaia_config.gaia_path =
        wizard_context_->gaia_config.last_gaia_path_shown;
  }

  SetCurrentScreen(previous_screen);
  return old_current_screen != current_screen_;
}

void WizardController::NotifyScreenChanged() {
  for (ScreenObserver& obs : screen_observers_) {
    obs.OnCurrentScreenChanged(current_screen_);
  }
}

policy::AutoEnrollmentController*
WizardController::GetAutoEnrollmentController() {
  if (!auto_enrollment_controller_) {
    auto_enrollment_controller_ =
        std::make_unique<policy::AutoEnrollmentController>(
            shared_url_loader_factory_);
  }
  return auto_enrollment_controller_.get();
}

void WizardController::MaybeTakeTPMOwnership() {
  if (wizard_context_->is_branded_build || switches::IsTpmDynamic()) {
    return;
  }

  chromeos::TpmManagerClient::Get()->TakeOwnership(
      ::tpm_manager::TakeOwnershipRequest(), base::DoNothing());
}

void WizardController::ResetCurrentScreen() {
  if (current_screen_) {
    current_screen_->Hide();
    current_screen_ = nullptr;
  }
}

void WizardController::MaybeAbortQuickStartFlow(
    quick_start::QuickStartController::AbortFlowReason reason) {
  if (wizard_context_->quick_start_setup_ongoing) {
    quickstart_controller_->AbortFlow(reason);
  }
}

}  // namespace ash
