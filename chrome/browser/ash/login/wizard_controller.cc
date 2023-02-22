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
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/customization/customization_document.h"
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
#include "chrome/browser/ash/login/screens/active_directory_login_screen.h"
#include "chrome/browser/ash/login/screens/active_directory_password_change_screen.h"
#include "chrome/browser/ash/login/screens/app_downloading_screen.h"
#include "chrome/browser/ash/login/screens/arc_terms_of_service_screen.h"
#include "chrome/browser/ash/login/screens/arc_vm_data_migration_screen.h"
#include "chrome/browser/ash/login/screens/assistant_optin_flow_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/choobe_screen.h"
#include "chrome/browser/ash/login/screens/consolidated_consent_screen.h"
#include "chrome/browser/ash/login/screens/cryptohome_recovery_screen.h"
#include "chrome/browser/ash/login/screens/cryptohome_recovery_setup_screen.h"
#include "chrome/browser/ash/login/screens/demo_preferences_screen.h"
#include "chrome/browser/ash/login/screens/demo_setup_screen.h"
#include "chrome/browser/ash/login/screens/device_disabled_screen.h"
#include "chrome/browser/ash/login/screens/edu_coexistence_login_screen.h"
#include "chrome/browser/ash/login/screens/enable_adb_sideloading_screen.h"
#include "chrome/browser/ash/login/screens/enable_debugging_screen.h"
#include "chrome/browser/ash/login/screens/encryption_migration_screen.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/screens/family_link_notice_screen.h"
#include "chrome/browser/ash/login/screens/fingerprint_setup_screen.h"
#include "chrome/browser/ash/login/screens/gaia_password_changed_screen.h"
#include "chrome/browser/ash/login/screens/gaia_password_changed_screen_legacy.h"
#include "chrome/browser/ash/login/screens/gaia_screen.h"
#include "chrome/browser/ash/login/screens/gesture_navigation_screen.h"
#include "chrome/browser/ash/login/screens/hardware_data_collection_screen.h"
#include "chrome/browser/ash/login/screens/hid_detection_screen.h"
#include "chrome/browser/ash/login/screens/kiosk_autolaunch_screen.h"
#include "chrome/browser/ash/login/screens/kiosk_enable_screen.h"
#include "chrome/browser/ash/login/screens/lacros_data_backward_migration_screen.h"
#include "chrome/browser/ash/login/screens/lacros_data_migration_screen.h"
#include "chrome/browser/ash/login/screens/local_state_error_screen.h"
#include "chrome/browser/ash/login/screens/locale_switch_screen.h"
#include "chrome/browser/ash/login/screens/management_transition_screen.h"
#include "chrome/browser/ash/login/screens/marketing_opt_in_screen.h"
#include "chrome/browser/ash/login/screens/multidevice_setup_screen.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ash/login/screens/network_screen.h"
#include "chrome/browser/ash/login/screens/offline_login_screen.h"
#include "chrome/browser/ash/login/screens/packaged_license_screen.h"
#include "chrome/browser/ash/login/screens/pin_setup_screen.h"
#include "chrome/browser/ash/login/screens/recommend_apps_screen.h"
#include "chrome/browser/ash/login/screens/recovery_eligibility_screen.h"
#include "chrome/browser/ash/login/screens/reset_screen.h"
#include "chrome/browser/ash/login/screens/saml_confirm_password_screen.h"
#include "chrome/browser/ash/login/screens/signin_fatal_error_screen.h"
#include "chrome/browser/ash/login/screens/smart_privacy_protection_screen.h"
#include "chrome/browser/ash/login/screens/sync_consent_screen.h"
#include "chrome/browser/ash/login/screens/theme_selection_screen.h"
#include "chrome/browser/ash/login/screens/touchpad_scroll_screen.h"
#include "chrome/browser/ash/login/screens/tpm_error_screen.h"
#include "chrome/browser/ash/login/screens/update_required_screen.h"
#include "chrome/browser/ash/login/screens/update_screen.h"
#include "chrome/browser/ash/login/screens/user_creation_screen.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ash/login/screens/wrong_hwid_screen.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/ash/net/delay_network_call.h"
#include "chrome/browser/ash/net/rollback_network_config/rollback_network_config_service.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/ash/system/device_disabling_manager.h"
#include "chrome/browser/ash/system/timezone_resolver_manager.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/active_directory_login_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/active_directory_password_change_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/app_downloading_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/arc_terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/arc_vm_data_migration_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/assistant_optin_flow_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/auto_enrollment_check_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/choobe_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/consolidated_consent_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/demo_preferences_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/demo_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/device_disabled_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/enable_adb_sideloading_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/encryption_migration_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/enrollment_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/family_link_notice_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/fingerprint_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_password_changed_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gesture_navigation_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/guest_tos_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/hardware_data_collection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/hid_detection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/kiosk_autolaunch_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/kiosk_enable_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/lacros_data_backward_migration_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/lacros_data_migration_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/local_state_error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/locale_switch_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/management_transition_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/marketing_opt_in_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/multidevice_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/offline_login_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/login/os_install_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/os_trial_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/packaged_license_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/parental_handoff_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/pin_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/recommend_apps_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/recovery_eligibility_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/reset_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/saml_confirm_password_handler.h"
#include "chrome/browser/ui/webui/ash/login/signin_fatal_error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/smart_privacy_protection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/sync_consent_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/theme_selection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/touchpad_scroll_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/tpm_error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/update_required_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/update_screen_handler.h"
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
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "chromeos/ash/components/timezone/timezone_provider.h"
#include "chromeos/ash/components/timezone/timezone_request.h"
#include "chromeos/ash/services/rollback_network_config/public/mojom/rollback_network_config.mojom.h"
#include "components/crash/core/app/breakpad_linux.h"
#include "components/crash/core/app/crashpad.h"
#include "components/metrics/structured/neutrino_logging.h"
#include "components/metrics/structured/neutrino_logging_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_types.h"
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

// TODO(https://crbug.com/1161535) Remove after stepping stone is set after M87.
constexpr char kLegacyUpdateScreenName[] = "update";

// Stores the list of all screens that should be shown when resuming OOBE.
const StaticOobeScreenId kResumableOobeScreens[] = {
    WelcomeView::kScreenId,
    NetworkScreenView::kScreenId,
    UpdateView::kScreenId,
    EnrollmentScreenView::kScreenId,
    AutoEnrollmentCheckScreenView::kScreenId,
};

const StaticOobeScreenId kResumablePostLoginScreens[] = {
    TermsOfServiceScreenView::kScreenId,
    SyncConsentScreenView::kScreenId,
    HWDataCollectionView::kScreenId,
    FingerprintSetupScreenView::kScreenId,
    GestureNavigationScreenView::kScreenId,
    ArcTermsOfServiceScreenView::kScreenId,
    RecommendAppsScreenView::kScreenId,
    PinSetupScreenView::kScreenId,
    MarketingOptInScreenView::kScreenId,
    MultiDeviceSetupScreenView::kScreenId,
    ConsolidatedConsentScreenView::kScreenId,
    ThemeSelectionScreenView::kScreenId,
};

const StaticOobeScreenId kScreensWithHiddenStatusArea[] = {
    EnableAdbSideloadingScreenView::kScreenId,
    EnableDebuggingScreenView::kScreenId,
    KioskAutolaunchScreenView::kScreenId,
    KioskEnableScreenView::kScreenId,
    ManagementTransitionScreenView::kScreenId,
    TpmErrorView::kScreenId,
    WrongHWIDScreenView::kScreenId,
    LocalStateErrorScreenView::kScreenId,
};

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

struct Entry {
  StaticOobeScreenId screen;
  const char* uma_name;
};

// Some screens had multiple different names in the past (they have since been
// unified). We need to always use the same name for UMA stats, though.
constexpr const Entry kLegacyUmaOobeScreenNames[] = {
    {ArcTermsOfServiceScreenView::kScreenId, "arc_tos"},
    {EnrollmentScreenView::kScreenId, "enroll"},
    {WelcomeView::kScreenId, "network"},
    {TermsOfServiceScreenView::kScreenId, "tos"}};

std::string GetLegacyUmaOobeScreenName(const OobeScreenId& screen_id) {
  // Make sure to use initial UMA name if the name has changed.
  std::string uma_name = screen_id.name;
  for (const auto& entry : kLegacyUmaOobeScreenNames) {
    if (entry.screen.AsId() == screen_id) {
      uma_name = entry.uma_name;
      break;
    }
  }
  uma_name[0] = std::toupper(uma_name[0]);
  return uma_name;
}

void RecordUMAHistogramForOOBEStepShownStatus(
    OobeScreenId screen,
    WizardController::ScreenShownStatus status) {
  // Legacy histogram, requires old screen names.
  std::string screen_name = GetLegacyUmaOobeScreenName(screen);
  std::string histogram_name = "OOBE.StepShownStatus." + screen_name;
  base::UmaHistogramEnumeration(histogram_name, status);
}

void RecordUMAHistogramForOOBEStepCompletionTime(OobeScreenId screen,
                                                 const std::string& exit_reason,
                                                 base::TimeDelta step_time) {
  // Legacy histogram, requires old screen names.
  std::string uma_name = GetLegacyUmaOobeScreenName(screen);
  std::string histogram_name = "OOBE.StepCompletionTime." + uma_name;

  base::UmaHistogramMediumTimes(histogram_name, step_time);

  // Use for this histogram real screen names.
  std::string screen_name = screen.name;
  screen_name[0] = std::toupper(screen_name[0]);
  std::string histogram_name_with_reason =
      "OOBE.StepCompletionTimeByExitReason." + screen_name + "." + exit_reason;
  base::UmaHistogramCustomTimes(histogram_name_with_reason, step_time,
                                base::Milliseconds(10), base::Minutes(10), 100);
}

LoginDisplayHost* GetLoginDisplayHost() {
  return LoginDisplayHost::default_host();
}

OobeUI* GetOobeUI() {
  auto* host = LoginDisplayHost::default_host();
  return host ? host->GetOobeUI() : nullptr;
}

scoped_refptr<network::SharedURLLoaderFactory>&
GetSharedURLLoaderFactoryForTesting() {
  static base::NoDestructor<scoped_refptr<network::SharedURLLoaderFactory>>
      loader;
  return *loader;
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
    : screen_manager_(std::make_unique<ScreenManager>()),
      wizard_context_(wizard_context) {
  wizard_context_->skip_post_login_screens_for_tests =
      switches::ShouldSkipOobePostLogin();
  wizard_context_->is_add_person_flow =
      StartupUtils::IsOobeCompleted() && StartupUtils::IsDeviceOwned();
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
    // OOBE UI can be recreated in case of CrossOriginOpenerPolicyByDefault.
    // TODO(crbug.com/1100879): Remove this logic after WebUI split is done,
    // as screens should work with late binding/early unbinding in that case.
    oobe_ui_observation_.Observe(GetOobeUI());
  }
}

WizardController::~WizardController() {
  for (ScreenObserver& obs : screen_observers_) {
    obs.OnShutdown();
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

  // This is a hacky way to check for local state corruption, because
  // it depends on the fact that the local state is loaded
  // synchronously and at the first demand. IsDeviceEnterpriseManaged()
  // check is required because currently powerwash is disabled for
  // enterprise-enrolled devices.
  //
  // TODO (ygorshenin@): implement handling of the local state
  // corruption in the case of asynchronous loading.
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  const bool is_enterprise_managed = connector->IsDeviceEnterpriseManaged();
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
  current_screen_->Hide();
  current_screen_ = nullptr;
  // Reset screens, they should not access handlers anymore.
  // TODO(https://crbug.com/1309022): This should probably be removed when all
  // the screen/handlers migrated to the new patterns.
  screen_manager_->Shutdown();
  oobe_ui_observation_.Reset();
}

void WizardController::HideCurrentScreen() {
  SetCurrentScreen(nullptr);
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
  auto& testing_factory = GetSharedURLLoaderFactoryForTesting();
  testing_factory = std::move(factory);
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

    if (ash::features::IsOobeQuickStartEnabled()) {
      append(std::make_unique<QuickStartScreen>(
          oobe_ui->GetView<QuickStartScreenHandler>()->AsWeakPtr(),
          base::BindRepeating(&WizardController::OnQuickStartScreenExit,
                              weak_factory_.GetWeakPtr())));
    }
  }

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
  append(std::make_unique<KioskEnableScreen>(
      oobe_ui->GetView<KioskEnableScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnKioskEnableScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<KioskAutolaunchScreen>(
      oobe_ui->GetView<KioskAutolaunchScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnKioskAutolaunchScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<LocaleSwitchScreen>(
      oobe_ui->GetView<LocaleSwitchScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnLocaleSwitchScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<RecoveryEligibilityScreen>(
      base::BindRepeating(&WizardController::OnRecoveryEligibilityScreenExit,
                          weak_factory_.GetWeakPtr())));
  if (features::IsCryptohomeRecoveryEnabled()) {
    append(std::make_unique<CryptohomeRecoverySetupScreen>(
        oobe_ui->GetView<CryptohomeRecoverySetupScreenHandler>()->AsWeakPtr(),
        base::BindRepeating(
            &WizardController::OnCryptohomeRecoverySetupScreenExit,
            weak_factory_.GetWeakPtr())));
  }
  append(std::make_unique<TermsOfServiceScreen>(
      oobe_ui->GetView<TermsOfServiceScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnTermsOfServiceScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<SyncConsentScreen>(
      oobe_ui->GetView<SyncConsentScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnSyncConsentScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<ArcTermsOfServiceScreen>(
      oobe_ui->GetView<ArcTermsOfServiceScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnArcTermsOfServiceScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<RecommendAppsScreen>(
      oobe_ui->GetView<RecommendAppsScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnRecommendAppsScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<AppDownloadingScreen>(
      oobe_ui->GetView<AppDownloadingScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnAppDownloadingScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<WrongHWIDScreen>(
      oobe_ui->GetView<WrongHWIDScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnWrongHWIDScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<LacrosDataMigrationScreen>(
      oobe_ui->GetView<LacrosDataMigrationScreenHandler>()->AsWeakPtr()));
  append(std::make_unique<LacrosDataBackwardMigrationScreen>(
      oobe_ui->GetView<LacrosDataBackwardMigrationScreenHandler>()
          ->AsWeakPtr()));
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
  append(std::make_unique<GaiaScreen>(
      oobe_ui->GetView<GaiaScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnGaiaScreenExit,
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

  if (ash::features::IsCryptohomeRecoveryEnabled()) {
    auto gaia_password_change_screen =
        std::make_unique<GaiaPasswordChangedScreen>(
            base::BindRepeating(&WizardController::OnPasswordChangeScreenExit,
                                weak_factory_.GetWeakPtr()),
            oobe_ui->GetView<GaiaPasswordChangedScreenHandler>()->AsWeakPtr());
    append(std::move(gaia_password_change_screen));
  } else {
    auto gaia_password_change_screen =
        std::make_unique<GaiaPasswordChangedScreenLegacy>(
            base::BindRepeating(
                &WizardController::OnPasswordChangeLegacyScreenExit,
                weak_factory_.GetWeakPtr()),
            oobe_ui->GetView<GaiaPasswordChangedScreenHandler>()->AsWeakPtr());
    append(std::move(gaia_password_change_screen));
  }

  append(std::make_unique<ActiveDirectoryPasswordChangeScreen>(
      oobe_ui->GetView<ActiveDirectoryPasswordChangeScreenHandler>()
          ->AsWeakPtr(),
      base::BindRepeating(
          &WizardController::OnActiveDirectoryPasswordChangeScreenExit,
          weak_factory_.GetWeakPtr())));

  append(std::make_unique<FamilyLinkNoticeScreen>(
      oobe_ui->GetView<FamilyLinkNoticeScreenHandler>()->AsWeakPtr(),
      base::BindRepeating(&WizardController::OnFamilyLinkNoticeScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<UserCreationScreen>(
      oobe_ui->GetView<UserCreationScreenHandler>()->AsWeakPtr(),
      oobe_ui->GetErrorScreen(),
      base::BindRepeating(&WizardController::OnUserCreationScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<ActiveDirectoryLoginScreen>(
      oobe_ui->GetView<ActiveDirectoryLoginScreenHandler>()->AsWeakPtr(),
      oobe_ui->GetErrorScreen(),
      base::BindRepeating(&WizardController::OnActiveDirectoryLoginScreenExit,
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

  if (features::IsOobeConsolidatedConsentEnabled()) {
    append(std::make_unique<ConsolidatedConsentScreen>(
        oobe_ui->GetView<ConsolidatedConsentScreenHandler>()->AsWeakPtr(),
        base::BindRepeating(&WizardController::OnConsolidatedConsentScreenExit,
                            weak_factory_.GetWeakPtr())));

    append(std::make_unique<GuestTosScreen>(
        oobe_ui->GetView<GuestTosScreenHandler>()->AsWeakPtr(),
        base::BindRepeating(&WizardController::OnGuestTosScreenExit,
                            weak_factory_.GetWeakPtr())));
  }

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

  if (features::IsCryptohomeRecoveryEnabled()) {
    append(std::make_unique<CryptohomeRecoveryScreen>(
        oobe_ui->GetView<CryptohomeRecoveryScreenHandler>()->AsWeakPtr(),
        base::BindRepeating(&WizardController::OnCryptohomeRecoveryScreenExit,
                            weak_factory_.GetWeakPtr())));
  }

  if (features::IsOobeChoobeEnabled()) {
    append(std::make_unique<ChoobeScreen>(
        oobe_ui->GetView<ChoobeScreenHandler>()->AsWeakPtr(),
        base::BindRepeating(&WizardController::OnChoobeScreenExit,
                            weak_factory_.GetWeakPtr())));
  }

  if (features::IsOobeChoobeEnabled() &&
      features::IsOobeTouchpadScrollEnabled()) {
    append(std::make_unique<TouchpadScrollScreen>(
        oobe_ui->GetView<TouchpadScrollScreenHandler>()->AsWeakPtr(),
        base::BindRepeating(&WizardController::OnTouchpadScreenExit,
                            weak_factory_.GetWeakPtr())));
  }

  return result;
}

void WizardController::ShowWelcomeScreen() {
  SetCurrentScreen(GetScreen(WelcomeView::kScreenId));
}

void WizardController::ShowQuickStartScreen() {
  CHECK(ash::features::IsOobeQuickStartEnabled());
  SetCurrentScreen(GetScreen(QuickStartView::kScreenId));
}

void WizardController::ShowNetworkScreen() {
  SetCurrentScreen(GetScreen(NetworkScreenView::kScreenId));
}

void WizardController::OnOwnershipStatusCheckDone(
    DeviceSettingsService::OwnershipStatus status) {
  if (status == DeviceSettingsService::OWNERSHIP_NONE) {
    ShowPackagedLicenseScreen();
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
      previous_screens_[current_screen_]->screen_id() ==
          SamlConfirmPasswordView::kScreenId) {
    // If the last screen user have visited before reaching SignInFatalError
    // screen was SamlConfirmPassword screen we should not go back there because
    // the context is lost at this point. We should go to the Gaia screen
    // instead.
    previous_screens_[current_screen_] = GetScreen(GaiaView::kScreenId);
    GetScreen<GaiaScreen>()->LoadOnline(EmptyAccountId());
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

void WizardController::ShowGaiaPasswordChangedScreenLegacy(
    const AccountId& account_id,
    bool has_error) {
  DCHECK(!ash::features::IsCryptohomeRecoveryEnabled());
  GaiaPasswordChangedScreenLegacy* screen =
      GetScreen<GaiaPasswordChangedScreenLegacy>();
  screen->Configure(account_id, has_error);
  if (current_screen_ != screen) {
    SetCurrentScreen(screen);
  } else {
    screen->Show(wizard_context_);
  }
}

void WizardController::ShowGaiaPasswordChangedScreen(
    std::unique_ptr<UserContext> user_context) {
  DCHECK(features::IsCryptohomeRecoveryEnabled());
  wizard_context_->user_context = std::move(user_context);
  SetCurrentScreen(GetScreen<GaiaPasswordChangedScreen>());
}

void WizardController::ShowEnrollmentScreen() {
  // Update the enrollment configuration and start the screen.
  prescribed_enrollment_config_ =
      policy::EnrollmentConfig::GetPrescribedEnrollmentConfig();
  StartEnrollmentScreen(false);
}

void WizardController::ShowDemoModePreferencesScreen() {
  SetCurrentScreen(GetScreen(DemoPreferencesScreenView::kScreenId));
}

void WizardController::ShowDemoModeSetupScreen() {
  SetCurrentScreen(GetScreen(DemoSetupScreenView::kScreenId));
}

void WizardController::ShowResetScreen() {
  SetCurrentScreen(GetScreen(ResetView::kScreenId));
}

void WizardController::ShowKioskEnableScreen() {
  SetCurrentScreen(GetScreen(KioskEnableScreenView::kScreenId));
}

void WizardController::ShowKioskAutolaunchScreen() {
  SetCurrentScreen(GetScreen(KioskAutolaunchScreenView::kScreenId));
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

void WizardController::ShowFingerprintSetupScreen() {
  SetCurrentScreen(GetScreen(FingerprintSetupScreenView::kScreenId));
}

void WizardController::ShowThemeSelectionScreen() {
  SetCurrentScreen(GetScreen(ThemeSelectionScreenView::kScreenId));
}

void WizardController::ShowMarketingOptInScreen() {
  SetCurrentScreen(GetScreen(MarketingOptInScreenView::kScreenId));
}

void WizardController::ShowArcTermsOfServiceScreen() {
  SetCurrentScreen(GetScreen(ArcTermsOfServiceScreenView::kScreenId));
}

void WizardController::ShowRecommendAppsScreen() {
  SetCurrentScreen(GetScreen(RecommendAppsScreenView::kScreenId));
}

void WizardController::ShowAppDownloadingScreen() {
  SetCurrentScreen(GetScreen(AppDownloadingScreenView::kScreenId));
}

void WizardController::ShowWrongHWIDScreen() {
  SetCurrentScreen(GetScreen(WrongHWIDScreenView::kScreenId));
}

void WizardController::ShowAutoEnrollmentCheckScreen() {
  AutoEnrollmentCheckScreen* screen = GetScreen<AutoEnrollmentCheckScreen>();
  if (retry_auto_enrollment_check_) {
    screen->ClearState();
  }
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

void WizardController::ShowPinSetupScreen() {
  SetCurrentScreen(GetScreen(PinSetupScreenView::kScreenId));
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
  GetChoobeFlowController()->Start();
  SetCurrentScreen(GetScreen(ChoobeScreenView::kScreenId));
}

void WizardController::ShowTouchpadScrollScreen() {
  SetCurrentScreen(GetScreen(TouchpadScrollScreenView::kScreenId));
}

void WizardController::ShowCryptohomeRecoverySetupScreen() {
  CHECK(features::IsCryptohomeRecoveryEnabled());
  SetCurrentScreen(GetScreen(CryptohomeRecoverySetupScreenView::kScreenId));
}

void WizardController::ShowAuthenticationSetupScreen() {
  if (features::IsCryptohomeRecoveryEnabled()) {
    ShowCryptohomeRecoverySetupScreen();
  } else {
    ShowFingerprintSetupScreen();
  }
}

void WizardController::ShowActiveDirectoryPasswordChangeScreen(
    const std::string& username) {
  GetScreen<ActiveDirectoryPasswordChangeScreen>()->SetUsername(username);
  AdvanceToScreen(ActiveDirectoryPasswordChangeView::kScreenId);
}

void WizardController::ShowLacrosDataMigrationScreen() {
  SetCurrentScreen(GetScreen(LacrosDataMigrationScreenView::kScreenId));
}

void WizardController::ShowLacrosDataBackwardMigrationScreen() {
  SetCurrentScreen(GetScreen(LacrosDataBackwardMigrationScreenView::kScreenId));
}

void WizardController::ShowGuestTosScreen() {
  DCHECK(features::IsOobeConsolidatedConsentEnabled());
  SetCurrentScreen(GetScreen(GuestTosScreenView::kScreenId));
}

void WizardController::ShowArcVmDataMigrationScreen() {
  SetCurrentScreen(GetScreen(ArcVmDataMigrationScreenView::kScreenId));
}

void WizardController::ShowCryptohomeRecoveryScreen(
    std::unique_ptr<UserContext> user_context) {
  DCHECK(features::IsCryptohomeRecoveryEnabled());
  wizard_context_->user_context = std::move(user_context);
  SetCurrentScreen(GetScreen(CryptohomeRecoveryScreenView::kScreenId));
}

void WizardController::OnActiveDirectoryPasswordChangeScreenExit() {
  OnScreenExit(ActiveDirectoryPasswordChangeView::kScreenId,
               kDefaultExitReason);
  ShowLoginScreen();
}

void WizardController::OnUserCreationScreenExit(
    UserCreationScreen::Result result) {
  OnScreenExit(UserCreationView::kScreenId,
               UserCreationScreen::GetResultString(result));
  switch (result) {
    case UserCreationScreen::Result::SIGNIN:
    case UserCreationScreen::Result::SKIPPED:
      AdvanceToSigninScreen();
      break;
    case UserCreationScreen::Result::CHILD_SIGNIN:
      GetScreen<GaiaScreen>()->LoadOnlineForChildSignin();
      AdvanceToScreen(GaiaView::kScreenId);
      break;
    case UserCreationScreen::Result::CHILD_ACCOUNT_CREATE:
      GetScreen<GaiaScreen>()->LoadOnlineForChildSignup();
      AdvanceToScreen(GaiaView::kScreenId);
      break;
    case UserCreationScreen::Result::ENTERPRISE_ENROLL:
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

void WizardController::OnGaiaScreenExit(GaiaScreen::Result result) {
  OnScreenExit(GaiaView::kScreenId, GaiaScreen::GetResultString(result));
  switch (result) {
    case GaiaScreen::Result::BACK:
    case GaiaScreen::Result::CANCEL: {
      if (result == GaiaScreen::Result::BACK &&
          wizard_context_->is_user_creation_enabled) {
        // `Result::BACK` is only triggered when pressing back button. It goes
        // back to UserCreationScreen if screen is enabled; otherwise, it
        // behaves the same as `Result::CANCEL` which is triggered by pressing
        // ESC key.
        AdvanceToScreen(UserCreationView::kScreenId);
        break;
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
        GetScreen<GaiaScreen>()->LoadOnline(EmptyAccountId());
      }
      break;
    }
    case GaiaScreen::Result::ENTERPRISE_ENROLL:
      ShowEnrollmentScreenIfEligible();
      break;
    case GaiaScreen::Result::START_CONSUMER_KIOSK:
      LoginDisplayHost::default_host()->AttemptShowEnableConsumerKioskScreen();
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
          SignInFatalErrorScreen::Error::SCRAPED_PASSWORD_VERIFICATION_FAILURE,
          base::Value::Dict());
  }
}

void WizardController::OnPasswordChangeLegacyScreenExit(
    GaiaPasswordChangedScreenLegacy::Result result) {
  OnScreenExit(GaiaPasswordChangedView::kScreenId,
               GaiaPasswordChangedScreenLegacy::GetResultString(result));
  switch (result) {
    case GaiaPasswordChangedScreenLegacy::Result::CANCEL:
      LoginDisplayHost::default_host()->CancelPasswordChangedFlow();
      break;
    case GaiaPasswordChangedScreenLegacy::Result::RESYNC:
      LoginDisplayHost::default_host()->ResyncUserData();
      break;
    case GaiaPasswordChangedScreenLegacy::Result::MIGRATE:
      NOTREACHED();
  }
}

void WizardController::OnPasswordChangeScreenExit(
    GaiaPasswordChangedScreen::Result result) {
  OnScreenExit(GaiaPasswordChangedView::kScreenId,
               GaiaPasswordChangedScreen::GetResultString(result));
  switch (result) {
    case GaiaPasswordChangedScreen::Result::CANCEL:
      LoginDisplayHost::default_host()->CancelPasswordChangedFlow();
      break;
    case GaiaPasswordChangedScreen::Result::CONTINUE_LOGIN:
      ash::LoginDisplayHost::default_host()
          ->GetExistingUserController()
          ->LoginAuthenticated(std::move(wizard_context_->user_context));
      break;
    case GaiaPasswordChangedScreen::Result::RECREATE_USER: {
      std::unique_ptr<UserContext> context =
          std::move(wizard_context_->user_context);
      ash::LoginDisplayHost::default_host()->CompleteLogin(*context);
      break;
    }
    case GaiaPasswordChangedScreen::Result::CRYPTOHOME_ERROR:
      // TODO(b/239420684): Send an error to the UI.
      NOTREACHED();
      LoginDisplayHost::default_host()->CancelPasswordChangedFlow();
  }
}

void WizardController::OnActiveDirectoryLoginScreenExit() {
  OnScreenExit(ActiveDirectoryLoginView::kScreenId, kDefaultExitReason);
  LoginDisplayHost::default_host()->HideOobeDialog();
}

void WizardController::OnEduCoexistenceLoginScreenExit(
    EduCoexistenceLoginScreen::Result result) {
  OnScreenExit(EduCoexistenceLoginScreen::kScreenId,
               EduCoexistenceLoginScreen::GetResultString(result));
  // TODO(crbug.com/1248063): Handle the case when the feature flag is disabled
  // after being enabled during OOBE.
  if (features::IsOobeConsolidatedConsentEnabled()) {
    ShowConsolidatedConsentScreen();
  } else {
    ShowSyncConsentScreen();
  }
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

void WizardController::OnCryptohomeRecoverySetupScreenExit(
    CryptohomeRecoverySetupScreen::Result result) {
  OnScreenExit(CryptohomeRecoverySetupScreenView::kScreenId,
               CryptohomeRecoverySetupScreen::GetResultString(result));
  ShowFingerprintSetupScreen();
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
      GetScreen<GaiaScreen>()->LoadOnline(EmptyAccountId());
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
    case OsTrialScreen::Result::BACK:
      // The OS Trial screen is only shown when OS Installation is started from
      // the welcome screen, so if the back button was clicked we go back to
      // the welcome screen.
      ShowWelcomeScreen();
      break;
    case OsTrialScreen::Result::NEXT_TRY:
      ShowNetworkScreen();
      break;
    case OsTrialScreen::Result::NEXT_INSTALL:
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

  ShowAuthenticationSetupScreen();
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
          UserContext(user_manager::USER_TYPE_GUEST,
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

  // Stop CHOOBE after exiting the last optional screen.
  if (features::IsOobeChoobeEnabled()) {
    GetChoobeFlowController()->Stop(
        *ProfileManager::GetActiveUserProfile()->GetPrefs());
  }

  ShowMarketingOptInScreen();
}

void WizardController::OnCryptohomeRecoveryScreenExit(
    CryptohomeRecoveryScreen::Result result) {
  OnScreenExit(CryptohomeRecoveryScreenView::kScreenId,
               CryptohomeRecoveryScreen::GetResultString(result));
  switch (result) {
    case CryptohomeRecoveryScreen::Result::kSucceeded:
      ash::LoginDisplayHost::default_host()
          ->GetExistingUserController()
          ->LoginAuthenticated(std::move(wizard_context_->user_context));
      break;
    case CryptohomeRecoveryScreen::Result::kGaiaLogin:
    case CryptohomeRecoveryScreen::Result::kRetry:
      // TODO(b/257073746): We probably want to differentiate between retry with
      // or without login.
      GetScreen<GaiaScreen>()->LoadOnline(
          wizard_context_->user_context->GetAccountId());
      AdvanceToScreen(GaiaView::kScreenId);
      break;
    case CryptohomeRecoveryScreen::Result::kManualRecovery:
    case CryptohomeRecoveryScreen::Result::kNoRecoveryFactor:
      ShowGaiaPasswordChangedScreen(std::move(wizard_context_->user_context));
      break;
  }
}

void WizardController::OnChoobeScreenExit(ChoobeScreen::Result result) {
  OnScreenExit(ChoobeScreenView::kScreenId,
               ChoobeScreen::GetResultString(result));

  switch (result) {
    case ChoobeScreen::Result::SELECTED:
    case ChoobeScreen::Result::NOT_APPLICABLE:
      ShowThemeSelectionScreen();
      break;
    case ChoobeScreen::Result::SKIPPED:
      ShowMarketingOptInScreen();
      break;
  }
}

void WizardController::OnTouchpadScreenExit(
    TouchpadScrollScreen::Result result) {
  OnScreenExit(TouchpadScrollScreenView::kScreenId,
               TouchpadScrollScreen::GetResultString(result));
  switch (result) {
    case TouchpadScrollScreen::Result::kNotApplicable:
    case TouchpadScrollScreen::Result::kNext:
      ShowMarketingOptInScreen();
      break;
  }
}

void WizardController::SkipToLoginForTesting() {
  VLOG(1) << "WizardController::SkipToLoginForTesting()";
  if (current_screen_ && current_screen_->screen_id() == GaiaView::kScreenId) {
    return;
  }
  wizard_context_->skip_to_login_for_tests = true;

  PerformPostNetworkScreenActions();
  OnDeviceDisabledChecked(false /* device_disabled */);
}

void WizardController::OnScreenExit(OobeScreenId screen,
                                    const std::string& exit_reason) {
  VLOG(1) << "Wizard screen " << screen
          << " exited with reason: " << exit_reason;
  // Do not perform checks and record stats for the skipped screen.
  if (exit_reason == BaseScreen::kNotApplicable) {
    return;
  }
  DCHECK(current_screen_->screen_id() == screen);

  RecordUMAHistogramForOOBEStepCompletionTime(
      screen, exit_reason, base::TimeTicks::Now() - screen_show_times_[screen]);
}

void WizardController::AdvanceToSigninScreen() {
  if (g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->GetDeviceMode() == policy::DEVICE_MODE_ENTERPRISE_AD) {
    AdvanceToScreen(ActiveDirectoryLoginView::kScreenId);
  } else {
    // Reset Gaia.
    GetScreen<GaiaScreen>()->LoadOnline(EmptyAccountId());
    AdvanceToScreen(GaiaView::kScreenId);
  }
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
    case WelcomeScreen::Result::SETUP_DEMO:
      StartDemoModeSetup();
      return;
    case WelcomeScreen::Result::ENABLE_DEBUGGING:
      ShowEnableDebuggingScreen();
      return;
    case WelcomeScreen::Result::NEXT_OS_INSTALL:
      ShowOsTrialScreen();
      return;
    case WelcomeScreen::Result::NEXT:
      ShowNetworkScreen();
      return;
    case WelcomeScreen::Result::QUICK_START:
      ShowQuickStartScreen();
      return;
  }
}

void WizardController::OnQuickStartScreenExit(QuickStartScreen::Result result) {
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
  }
}

void WizardController::OnUpdateScreenExit(UpdateScreen::Result result) {
  OnScreenExit(UpdateView::kScreenId, UpdateScreen::GetResultString(result));

  switch (result) {
    case UpdateScreen::Result::UPDATE_NOT_REQUIRED:
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
  if (features::IsOobeConsolidatedConsentEnabled() && demo_setup_controller_) {
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
      PerformOOBECompletedActions();
      DCHECK(!prescribed_enrollment_config_.is_forced());
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
      retry_auto_enrollment_check_ = true;
      ShowAutoEnrollmentCheckScreen();
      break;
  }
}

void WizardController::OnEnrollmentDone() {
  PerformOOBECompletedActions();

  // Restart to make the login page pick up the policy changes resulting from
  // enrollment recovery.  (Not pretty, but this codepath is rarely exercised.)
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

  if (KioskAppManager::Get()->IsAutoLaunchEnabled()) {
    AutoLaunchKioskApp(KioskAppType::kChromeApp);
  } else if (WebKioskAppManager::Get()->GetAutoLaunchAccountId().is_valid()) {
    AutoLaunchKioskApp(KioskAppType::kWebApp);
  } else if (ArcKioskAppManager::Get()->GetAutoLaunchAccountId().is_valid()) {
    AutoLaunchKioskApp(KioskAppType::kArcApp);
  } else if (g_browser_process->platform_part()
                 ->browser_policy_connector_ash()
                 ->IsDeviceEnterpriseManaged()) {
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

void WizardController::OnKioskEnableScreenExit() {
  OnScreenExit(KioskEnableScreenView::kScreenId, kDefaultExitReason);

  ShowLoginScreen();
}

void WizardController::OnKioskAutolaunchScreenExit(
    KioskAutolaunchScreen::Result result) {
  OnScreenExit(KioskAutolaunchScreenView::kScreenId,
               KioskAutolaunchScreen::GetResultString(result));

  switch (result) {
    case KioskAutolaunchScreen::Result::COMPLETED:
      DCHECK(KioskAppManager::Get()->IsAutoLaunchEnabled());
      AutoLaunchKioskApp(KioskAppType::kChromeApp);
      break;
    case KioskAutolaunchScreen::Result::CANCELED:
      ShowLoginScreen();
      break;
  }
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
    case DemoSetupScreen::Result::COMPLETED:
      PerformOOBECompletedActions();
      SwitchWebUItoMojo();
      break;
    case DemoSetupScreen::Result::CANCELED:
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
  AdvanceToScreen(TermsOfServiceScreenView::kScreenId);
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

  ShowAuthenticationSetupScreen();
}

void WizardController::OnFingerprintSetupScreenExit(
    FingerprintSetupScreen::Result result) {
  OnScreenExit(FingerprintSetupScreenView::kScreenId,
               FingerprintSetupScreen::GetResultString(result));

  ShowPinSetupScreen();
}

void WizardController::OnPinSetupScreenExit(PinSetupScreen::Result result) {
  OnScreenExit(PinSetupScreenView::kScreenId,
               PinSetupScreen::GetResultString(result));

  ShowArcTermsOfServiceScreen();
}

void WizardController::OnArcTermsOfServiceScreenExit(
    ArcTermsOfServiceScreen::Result result) {
  OnScreenExit(ArcTermsOfServiceScreenView::kScreenId,
               ArcTermsOfServiceScreen::GetResultString(result));

  switch (result) {
    case ArcTermsOfServiceScreen::Result::ACCEPTED:
    case ArcTermsOfServiceScreen::Result::
        NOT_APPLICABLE_CONSOLIDATED_CONSENT_ARC_ENABLED:
      DCHECK(!demo_setup_controller_);
      ShowRecommendAppsScreen();
      break;
    case ArcTermsOfServiceScreen::Result::NOT_APPLICABLE:
      ShowAssistantOptInFlowScreen();
      break;
    case ArcTermsOfServiceScreen::Result::ACCEPTED_DEMO_ONLINE:
    case ArcTermsOfServiceScreen::Result::NOT_APPLICABLE_DEMO_ONLINE:
      DCHECK(demo_setup_controller_);
      InitiateOOBEUpdate();
      break;
    case ArcTermsOfServiceScreen::Result::BACK:
      DCHECK(demo_setup_controller_);
      DCHECK(StartupUtils::IsEulaAccepted());
      ShowDemoModePreferencesScreen();
      break;
  }
}

void WizardController::OnRecommendAppsScreenExit(
    RecommendAppsScreen::Result result) {
  OnScreenExit(RecommendAppsScreenView::kScreenId,
               RecommendAppsScreen::GetResultString(result));

  switch (result) {
    case RecommendAppsScreen::Result::SELECTED:
      ShowAppDownloadingScreen();
      break;
    case RecommendAppsScreen::Result::SKIPPED:
    case RecommendAppsScreen::Result::NOT_APPLICABLE:
    case RecommendAppsScreen::Result::LOAD_ERROR:
      ShowAssistantOptInFlowScreen();
      break;
  }
}

void WizardController::OnAppDownloadingScreenExit() {
  OnScreenExit(AppDownloadingScreenView::kScreenId, kDefaultExitReason);

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

  ShowGestureNavigationScreen();
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

void WizardController::OnChangedMetricsReportingState(bool enabled) {
  StatsReportingController::Get()->SetEnabled(
      ProfileManager::GetActiveUserProfile(), enabled);
}

void WizardController::OnDeviceModificationCanceled() {
  BaseScreen* previous_screen = nullptr;
  if (base::Contains(previous_screens_, current_screen_)) {
    previous_screen = previous_screens_[current_screen_];
  }

  current_screen_->Hide();
  current_screen_ = nullptr;

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
  current_screen_->Hide();
  current_screen_ = nullptr;
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

  if (features::IsOobeChoobeEnabled()) {
    // Additional cleanup of the pref kChoobeSelectedScreens in case it was not
    // already cleared.
    ProfileManager::GetActiveUserProfile()->GetPrefs()->ClearPref(
        prefs::kChoobeSelectedScreens);
  }

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
    StartEnrollmentScreen(wizard_context_->enrollment_triggered_early);
  } else {
    PerformOOBECompletedActions();
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

  DelayNetworkCall(base::Milliseconds(kDefaultNetworkRetryDelayMS),
                   base::BindOnce(&WizardController::StartTimezoneResolve,
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

  auto& testing_factory = GetSharedURLLoaderFactoryForTesting();
  geolocation_provider_ = std::make_unique<SimpleGeolocationProvider>(
      testing_factory ? testing_factory
                      : g_browser_process->shared_url_loader_factory(),
      SimpleGeolocationProvider::DefaultGeolocationProviderURL());
  geolocation_provider_->RequestGeolocation(
      base::Seconds(kResolveTimeZoneTimeoutSeconds),
      false /* send_wifi_geolocation_data */,
      false /* send_cellular_geolocation_data */,
      base::BindOnce(&WizardController::OnLocationResolved,
                     weak_factory_.GetWeakPtr()));
}

void WizardController::PerformPostNetworkScreenActions() {
  StartNetworkTimezoneResolve();
  DelayNetworkCall(base::Milliseconds(kDefaultNetworkRetryDelayMS),
                   ServicesCustomizationDocument::GetInstance()
                       ->EnsureCustomizationAppliedClosure());

  GetAutoEnrollmentController()->Start();
}

void WizardController::PerformOOBECompletedActions() {
  // Avoid marking OOBE as completed multiple times if going from login screen
  // to enrollment screen (and back).
  if (oobe_marked_completed_) {
    return;
  }

  StartupUtils::MarkOobeCompleted();
  oobe_marked_completed_ = true;
}

void WizardController::SetCurrentScreen(BaseScreen* new_current) {
  VLOG(1) << "SetCurrentScreen: "
          << (new_current ? new_current->screen_id().name : "null");

  if (new_current && new_current->MaybeSkip(*wizard_context_)) {
    RecordUMAHistogramForOOBEStepShownStatus(new_current->screen_id(),
                                             ScreenShownStatus::kSkipped);
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

  if (current_screen_) {
    current_screen_->Hide();
  }

  current_screen_ = new_current;

  if (!current_screen_) {
    NotifyScreenChanged();
    return;
  }

  // Record show time for UMA.
  screen_show_times_[new_current->screen_id()] = base::TimeTicks::Now();

  // First remember how far have we reached so that we can resume if needed.
  if (!demo_setup_controller_) {
    if (is_out_of_box_ && IsResumableOobeScreen(current_screen_->screen_id())) {
      StartupUtils::SaveOobePendingScreen(current_screen_->screen_id().name);
    } else if (IsResumablePostLoginScreen(current_screen_->screen_id()) &&
               !wizard_context_->is_cloud_ready_update_flow &&
               wizard_context_->screen_after_managed_tos !=
                   ash::OOBE_SCREEN_UNKNOWN) {
      // If screen_after_managed_tos == SCREEN_UNKNOWN means that the onboarding
      // has already been finished by the user and we don't need to save the
      // state here.
      user_manager::KnownUser(GetLocalState())
          .SetPendingOnboardingScreen(
              user_manager::UserManager::Get()->GetActiveUser()->GetAccountId(),
              current_screen_->screen_id().name);
    }
  }

  UpdateStatusAreaVisibilityForScreen(current_screen_->screen_id());
  RecordUMAHistogramForOOBEStepShownStatus(current_screen_->screen_id(),
                                           ScreenShownStatus::kShown);
  current_screen_->Show(wizard_context_);
  NotifyScreenChanged();
}

void WizardController::UpdateStatusAreaVisibilityForScreen(
    OobeScreenId screen_id) {
  if (screen_id == WelcomeView::kScreenId) {
    // Hide the status area initially; it only appears after OOBE first animates
    // in. Keep it visible if the user goes back to the existing welcome screen.
    GetLoginDisplayHost()->SetStatusAreaVisible(
        screen_manager_->HasScreen(WelcomeView::kScreenId));
  } else {
    GetLoginDisplayHost()->SetStatusAreaVisible(
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
  } else if (policy::EnrollmentRequisitionManager::IsMeetDevice()) {
    VLOG(1)
        << "Using default Device Requisition value for CFM build configuration"
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
  } else if (screen_id == KioskEnableScreenView::kScreenId) {
    ShowKioskEnableScreen();
  } else if (screen_id == KioskAutolaunchScreenView::kScreenId) {
    ShowKioskAutolaunchScreen();
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
  } else if (screen_id == ArcTermsOfServiceScreenView::kScreenId) {
    ShowArcTermsOfServiceScreen();
  } else if (screen_id == SyncConsentScreenView::kScreenId) {
    ShowSyncConsentScreen();
  } else if (screen_id == RecommendAppsScreenView::kScreenId) {
    ShowRecommendAppsScreen();
  } else if (screen_id == AppDownloadingScreenView::kScreenId) {
    ShowAppDownloadingScreen();
  } else if (screen_id == WrongHWIDScreenView::kScreenId) {
    ShowWrongHWIDScreen();
  } else if (screen_id == AutoEnrollmentCheckScreenView::kScreenId) {
    ShowAutoEnrollmentCheckScreen();
  } else if (screen_id == AppLaunchSplashScreenView::kScreenId) {
    AutoLaunchKioskApp(KioskAppType::kChromeApp);
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
  } else if (screen_id == LacrosDataMigrationScreenView::kScreenId) {
    ShowLacrosDataMigrationScreen();
  } else if (screen_id == LacrosDataBackwardMigrationScreenView::kScreenId) {
    ShowLacrosDataBackwardMigrationScreen();
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
  } else if (screen_id == TpmErrorView::kScreenId ||
             screen_id == GaiaPasswordChangedView::kScreenId ||
             screen_id == ActiveDirectoryPasswordChangeView::kScreenId ||
             screen_id == FamilyLinkNoticeView::kScreenId ||
             screen_id == GaiaView::kScreenId ||
             screen_id == UserCreationView::kScreenId ||
             screen_id == ActiveDirectoryLoginView::kScreenId ||
             screen_id == SignInFatalErrorView::kScreenId ||
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
             screen_id == LocalStateErrorScreenView::kScreenId) {
    SetCurrentScreen(GetScreen(screen_id));
  } else {
    NOTREACHED();
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

void WizardController::SimulateDemoModeSetupForTesting(
    absl::optional<DemoSession::DemoModeConfig> demo_config) {
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

void WizardController::AutoLaunchKioskApp(KioskAppType app_type) {
  KioskAppId kiosk_app_id;
  switch (app_type) {
    case KioskAppType::kChromeApp: {
      KioskAppManagerBase::App app_data;
      std::string app_id = KioskAppManager::Get()->GetAutoLaunchApp();
      CHECK(KioskAppManager::Get()->GetApp(app_id, &app_data));
      kiosk_app_id = KioskAppId::ForChromeApp(app_id);
      break;
    }
    case KioskAppType::kWebApp: {
      const AccountId account_id =
          WebKioskAppManager::Get()->GetAutoLaunchAccountId();
      kiosk_app_id = KioskAppId::ForWebApp(account_id);
      break;
    }
    case KioskAppType::kArcApp:
      const AccountId account_id =
          ArcKioskAppManager::Get()->GetAutoLaunchAccountId();
      kiosk_app_id = KioskAppId::ForArcApp(account_id);
      break;
  }

  // Wait for the `CrosSettings` to become either trusted or permanently
  // untrusted.
  const CrosSettingsProvider::TrustedStatus status =
      CrosSettings::Get()->PrepareTrustedValues(
          base::BindOnce(&WizardController::AutoLaunchKioskApp,
                         weak_factory_.GetWeakPtr(), app_type));
  if (status == CrosSettingsProvider::TEMPORARILY_UNTRUSTED) {
    return;
  }

  if (status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
    // If the `cros_settings_` are permanently untrusted, show an error message
    // and refuse to auto-launch the kiosk app.
    AdvanceToScreen(LocalStateErrorScreenView::kScreenId);
    return;
  }

  if (system::DeviceDisablingManager::IsDeviceDisabledDuringNormalOperation()) {
    // If the device is disabled, bail out. A device disabled screen will be
    // shown by the DeviceDisablingManager.
    return;
  }

  constexpr bool auto_launch = true;
  GetLoginDisplayHost()->StartKiosk(kiosk_app_id, auto_launch);
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
bool WizardController::IsZeroTouchHandsOffOobeFlow() {
  return policy::DeviceCloudPolicyManagerAsh::GetZeroTouchEnrollmentMode() ==
         policy::ZeroTouchEnrollmentMode::HANDS_OFF;
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

  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (connector->IsDeviceEnterpriseManaged()) {
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
    auto& testing_factory = GetSharedURLLoaderFactoryForTesting();
    timezone_provider_ = std::make_unique<TimeZoneProvider>(
        testing_factory ? testing_factory
                        : g_browser_process->shared_url_loader_factory(),
        DefaultTimezoneProviderURL());
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

void WizardController::StartEnrollmentScreen(bool force_interactive) {
  VLOG(1) << "Showing enrollment screen."
          << " Forcing interactive enrollment: " << force_interactive << ".";

  // Determine the effective enrollment configuration. If there is a valid
  // prescribed configuration, use that. If not, figure out which variant of
  // manual enrollment is taking place.
  // If OOBE Configuration exits, it might also affect enrollment configuration.
  policy::EnrollmentConfig effective_config = prescribed_enrollment_config_;
  if (!effective_config.should_enroll() ||
      (force_interactive && !effective_config.should_enroll_interactively())) {
    effective_config.mode =
        prescribed_enrollment_config_.management_domain.empty()
            ? policy::EnrollmentConfig::MODE_MANUAL
            : policy::EnrollmentConfig::MODE_MANUAL_REENROLLMENT;
  }

  EnrollmentScreen* screen = EnrollmentScreen::Get(screen_manager());
  screen->SetEnrollmentConfig(effective_config);
  UpdateStatusAreaVisibilityForScreen(EnrollmentScreenView::kScreenId);
  SetCurrentScreen(screen);
}

void WizardController::ShowEnrollmentScreenIfEligible() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  const bool enterprise_managed = connector->IsDeviceEnterpriseManaged();
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
  auto* old_current_screen = current_screen_;
  SetCurrentScreen(previous_screens_[current_screen_]);
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
        std::make_unique<policy::AutoEnrollmentController>();
  }
  return auto_enrollment_controller_.get();
}

ChoobeFlowController* WizardController::GetChoobeFlowController() {
  if (!choobe_flow_controller_) {
    choobe_flow_controller_ = std::make_unique<ChoobeFlowController>();
  }
  return choobe_flow_controller_.get();
}

void WizardController::MaybeTakeTPMOwnership() {
  if (wizard_context_->is_branded_build || switches::IsTpmDynamic()) {
    return;
  }

  DCHECK(features::IsOobeConsolidatedConsentEnabled());
  chromeos::TpmManagerClient::Get()->TakeOwnership(
      ::tpm_manager::TakeOwnershipRequest(), base::DoNothing());
}

}  // namespace ash
