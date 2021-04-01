// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#include "ash/components/audio/cras_audio_handler.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/devicetype.h"
#include "ash/public/cpp/ash_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/enrollment/auto_enrollment_check_screen.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/hwid_checker.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/screens/active_directory_login_screen.h"
#include "chrome/browser/ash/login/screens/active_directory_password_change_screen.h"
#include "chrome/browser/ash/login/screens/app_downloading_screen.h"
#include "chrome/browser/ash/login/screens/arc_terms_of_service_screen.h"
#include "chrome/browser/ash/login/screens/assistant_optin_flow_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/demo_preferences_screen.h"
#include "chrome/browser/ash/login/screens/demo_setup_screen.h"
#include "chrome/browser/ash/login/screens/device_disabled_screen.h"
#include "chrome/browser/ash/login/screens/edu_coexistence_login_screen.h"
#include "chrome/browser/ash/login/screens/enable_adb_sideloading_screen.h"
#include "chrome/browser/ash/login/screens/enable_debugging_screen.h"
#include "chrome/browser/ash/login/screens/encryption_migration_screen.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/screens/eula_screen.h"
#include "chrome/browser/ash/login/screens/family_link_notice_screen.h"
#include "chrome/browser/ash/login/screens/fingerprint_setup_screen.h"
#include "chrome/browser/ash/login/screens/gaia_password_changed_screen.h"
#include "chrome/browser/ash/login/screens/gaia_screen.h"
#include "chrome/browser/ash/login/screens/gesture_navigation_screen.h"
#include "chrome/browser/ash/login/screens/hid_detection_screen.h"
#include "chrome/browser/ash/login/screens/kiosk_autolaunch_screen.h"
#include "chrome/browser/ash/login/screens/kiosk_enable_screen.h"
#include "chrome/browser/ash/login/screens/locale_switch_screen.h"
#include "chrome/browser/ash/login/screens/marketing_opt_in_screen.h"
#include "chrome/browser/ash/login/screens/multidevice_setup_screen.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ash/login/screens/network_screen.h"
#include "chrome/browser/ash/login/screens/offline_login_screen.h"
#include "chrome/browser/ash/login/screens/packaged_license_screen.h"
#include "chrome/browser/ash/login/screens/pin_setup_screen.h"
#include "chrome/browser/ash/login/screens/recommend_apps_screen.h"
#include "chrome/browser/ash/login/screens/reset_screen.h"
#include "chrome/browser/ash/login/screens/signin_fatal_error_screen.h"
#include "chrome/browser/ash/login/screens/supervision_transition_screen.h"
#include "chrome/browser/ash/login/screens/sync_consent_screen.h"
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
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/ash/system/device_disabling_manager.h"
#include "chrome/browser/ash/system/timezone_resolver_manager.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/chromeos/net/delay_network_call.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/enrollment_requisition_manager.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/webui/chromeos/login/active_directory_login_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/active_directory_password_change_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/app_downloading_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/arc_terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/assistant_optin_flow_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/auto_enrollment_check_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_preferences_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_setup_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/device_disabled_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/encryption_migration_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/enrollment_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/eula_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/family_link_notice_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/fingerprint_setup_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_password_changed_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gesture_navigation_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/hid_detection_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/kiosk_autolaunch_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/kiosk_enable_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/locale_switch_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/marketing_opt_in_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/multidevice_setup_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/offline_login_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/packaged_license_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/parental_handoff_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/pin_setup_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/recommend_apps_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/reset_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_fatal_error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/supervision_transition_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/sync_consent_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/tpm_error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/update_required_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/wrong_hwid_screen_handler.h"
#include "chrome/browser/ui/webui/help/help_utils_chromeos.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/geolocation/simple_geolocation_provider.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "chromeos/settings/timezone_settings.h"
#include "chromeos/timezone/timezone_provider.h"
#include "chromeos/timezone/timezone_request.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/crash/core/app/breakpad_linux.h"
#include "components/crash/core/app/crashpad.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_types.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/accelerators/accelerator.h"

using content::BrowserThread;

namespace {

bool g_using_zero_delays = false;

// Total timezone resolving process timeout.
const unsigned int kResolveTimeZoneTimeoutSeconds = 60;

constexpr const char kDefaultExitReason[] = "Next";
constexpr const char kResetScreenExitReason[] = "Cancel";

// TODO(https://crbug.com/1161535) Remove after stepping stone is set after M87.
constexpr char kLegacyUpdateScreenName[] = "update";

// Stores the list of all screens that should be shown when resuming OOBE.
const chromeos::StaticOobeScreenId kResumableScreens[] = {
    chromeos::WelcomeView::kScreenId,
    chromeos::NetworkScreenView::kScreenId,
    chromeos::UpdateView::kScreenId,
    chromeos::EulaView::kScreenId,
    chromeos::EnrollmentScreenView::kScreenId,
    chromeos::TermsOfServiceScreenView::kScreenId,
    chromeos::SyncConsentScreenView::kScreenId,
    chromeos::FingerprintSetupScreenView::kScreenId,
    chromeos::GestureNavigationScreenView::kScreenId,
    chromeos::ArcTermsOfServiceScreenView::kScreenId,
    chromeos::AutoEnrollmentCheckScreenView::kScreenId,
    chromeos::RecommendAppsScreenView::kScreenId,
    chromeos::AppDownloadingScreenView::kScreenId,
    chromeos::PinSetupScreenView::kScreenId,
    chromeos::MarketingOptInScreenView::kScreenId,
    chromeos::MultiDeviceSetupScreenView::kScreenId,
};

const chromeos::StaticOobeScreenId kScreensWithHiddenStatusArea[] = {
    chromeos::EnableAdbSideloadingScreenView::kScreenId,
    chromeos::EnableDebuggingScreenView::kScreenId,
    chromeos::KioskAutolaunchScreenView::kScreenId,
    chromeos::KioskEnableScreenView::kScreenId,
    chromeos::SupervisionTransitionScreenView::kScreenId,
    chromeos::TpmErrorView::kScreenId,
    chromeos::WrongHWIDScreenView::kScreenId,
};

// The HID detection screen is only allowed for form factors without built-in
// inputs: Chromebases, Chromebits, and Chromeboxes (crbug.com/965765).
bool CanShowHIDDetectionScreen() {
  switch (chromeos::GetDeviceType()) {
    case chromeos::DeviceType::kChromebase:
    case chromeos::DeviceType::kChromebit:
    case chromeos::DeviceType::kChromebox:
      return true;
    default:
      return false;
  }
}

bool IsResumableScreen(chromeos::OobeScreenId screen_id) {
  for (const auto& resumable_screen : kResumableScreens) {
    if (screen_id == resumable_screen)
      return true;
  }
  return false;
}

bool ShouldHideStatusArea(chromeos::OobeScreenId screen_id) {
  for (const auto& s : kScreensWithHiddenStatusArea) {
    if (screen_id == s)
      return true;
  }
  return false;
}

struct Entry {
  chromeos::StaticOobeScreenId screen;
  const char* uma_name;
};

// Some screens had multiple different names in the past (they have since been
// unified). We need to always use the same name for UMA stats, though.
constexpr const Entry kLegacyUmaOobeScreenNames[] = {
    {chromeos::ArcTermsOfServiceScreenView::kScreenId, "arc_tos"},
    {chromeos::EnrollmentScreenView::kScreenId, "enroll"},
    {chromeos::WelcomeView::kScreenId, "network"},
    {chromeos::OobeScreen::SCREEN_CREATE_SUPERVISED_USER_FLOW_DEPRECATED,
     "supervised-user-creation-flow"},
    {chromeos::TermsOfServiceScreenView::kScreenId, "tos"}};

void RecordUMAHistogramForOOBEStepShownStatus(
    chromeos::OobeScreenId screen,
    WizardController::ScreenShownStatus status) {
  std::string screen_name = screen.name;
  screen_name[0] = std::toupper(screen_name[0]);
  std::string histogram_name = "OOBE.StepShownStatus." + screen_name;
  base::UmaHistogramEnumeration(histogram_name, status);
}

void RecordUMAHistogramForOOBEStepCompletionTime(chromeos::OobeScreenId screen,
                                                 const std::string& exit_reason,
                                                 base::TimeDelta step_time) {
  // Fetch screen name; make sure to use initial UMA name if the name has
  // changed.
  std::string screen_name = screen.name;
  for (const auto& entry : kLegacyUmaOobeScreenNames) {
    if (entry.screen.AsId() == screen) {
      screen_name = entry.uma_name;
      break;
    }
  }

  screen_name[0] = std::toupper(screen_name[0]);
  std::string histogram_name = "OOBE.StepCompletionTime." + screen_name;

  // Equivalent to using UMA_HISTOGRAM_MEDIUM_TIMES. UMA_HISTOGRAM_MEDIUM_TIMES
  // can not be used here, because `histogram_name` is calculated dynamically
  // and changes from call to call.
  base::HistogramBase* histogram = base::Histogram::FactoryTimeGet(
      histogram_name, base::TimeDelta::FromMilliseconds(10),
      base::TimeDelta::FromMinutes(3), 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddTime(step_time);

  // Use for this Histogram real screen names.
  screen_name = screen.name;
  screen_name[0] = std::toupper(screen_name[0]);
  std::string histogram_name_with_reason =
      "OOBE.StepCompletionTimeByExitReason." + screen_name + "." + exit_reason;
  base::HistogramBase* histogram_with_reason = base::Histogram::FactoryTimeGet(
      histogram_name_with_reason, base::TimeDelta::FromMilliseconds(10),
      base::TimeDelta::FromMinutes(10), 100,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram_with_reason->AddTime(step_time);
}

chromeos::LoginDisplayHost* GetLoginDisplayHost() {
  return chromeos::LoginDisplayHost::default_host();
}

chromeos::OobeUI* GetOobeUI() {
  auto* host = chromeos::LoginDisplayHost::default_host();
  return host ? host->GetOobeUI() : nullptr;
}

scoped_refptr<network::SharedURLLoaderFactory>&
GetSharedURLLoaderFactoryForTesting() {
  static base::NoDestructor<scoped_refptr<network::SharedURLLoaderFactory>>
      loader;
  return *loader;
}

chromeos::OobeScreenId PrefToScreenId(const std::string& pref_value) {
  if (pref_value == kLegacyUpdateScreenName)
    return chromeos::UpdateView::kScreenId;
  return chromeos::OobeScreenId(pref_value);
}

}  // namespace

namespace chromeos {

// static
const int WizardController::kMinAudibleOutputVolumePercent = 10;

// static
bool WizardController::skip_post_login_screens_ = false;

// static
bool WizardController::skip_enrollment_prompts_ = false;

// static
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
bool WizardController::is_branded_build_ = true;
#else
bool WizardController::is_branded_build_ = false;
#endif

// static
WizardController* WizardController::default_controller() {
  auto* host = chromeos::LoginDisplayHost::default_host();
  return host ? host->GetWizardController() : nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, public:

PrefService* WizardController::local_state_for_testing_ = nullptr;

WizardController::WizardController()
    : screen_manager_(std::make_unique<ScreenManager>()),
      wizard_context_(std::make_unique<WizardContext>()),
      network_state_helper_(std::make_unique<login::NetworkStateHelper>()) {
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
  }
}

WizardController::~WizardController() {
  for (ScreenObserver& obs : screen_observers_)
    obs.OnShutdown();
  screen_manager_.reset();
}

void WizardController::Init(OobeScreenId first_screen) {
  DCHECK(!is_initialized());
  is_initialized_ = true;

  prescribed_enrollment_config_ = g_browser_process->platform_part()
                                      ->browser_policy_connector_chromeos()
                                      ->GetPrescribedEnrollmentConfig();

  VLOG(1) << "Starting OOBE wizard with screen: " << first_screen;

  bool oobe_complete = StartupUtils::IsOobeCompleted();
  if (!oobe_complete) {
    UpdateOobeConfiguration();
    is_out_of_box_ = true;
  }

  // This is a hacky way to check for local state corruption, because
  // it depends on the fact that the local state is loaded
  // synchronously and at the first demand. IsEnterpriseManaged()
  // check is required because currently powerwash is disabled for
  // enterprise-enrolled devices.
  //
  // TODO (ygorshenin@): implement handling of the local state
  // corruption in the case of asynchronious loading.
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  const bool is_enterprise_managed = connector->IsEnterpriseManaged();
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
  if (!device_is_owned && CanShowHIDDetectionScreen() &&
      first_screen == OobeScreen::SCREEN_UNKNOWN) {
    GetScreen<HIDDetectionScreen>()->CheckIsScreenRequired(
        base::BindOnce(&WizardController::OnHIDScreenNecessityCheck,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  AdvanceToScreenAfterHIDDetection(first_screen);
}

void WizardController::AdvanceToScreenAfterHIDDetection(
    OobeScreenId first_screen) {
  OobeScreenId actual_first_screen = first_screen;
  if (actual_first_screen == OobeScreen::SCREEN_UNKNOWN) {
    if (!is_out_of_box_) {
      DeviceSettingsService::Get()->GetOwnershipStatusAsync(
          base::BindOnce(&WizardController::OnOwnershipStatusCheckDone,
                         weak_factory_.GetWeakPtr()));
      return;
    }

    // Use the saved screen preference from Local State.
    const std::string screen_pref =
        GetLocalState()->GetString(prefs::kOobeScreenPending);
    if (!screen_pref.empty() && HasScreen(PrefToScreenId(screen_pref)))
      actual_first_screen = PrefToScreenId(screen_pref);
    else
      actual_first_screen = WelcomeView::kScreenId;
  }

  first_screen_for_testing_ = actual_first_screen;
  AdvanceToScreen(actual_first_screen);

  if (!IsMachineHWIDCorrect() && !StartupUtils::IsDeviceRegistered() &&
      first_screen == OobeScreen::SCREEN_UNKNOWN) {
    ShowWrongHWIDScreen();
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kOobeSkipToLogin)) {
    SkipToLoginForTesting();
  }
}

ErrorScreen* WizardController::GetErrorScreen() {
  return GetOobeUI()->GetErrorScreen();
}

bool WizardController::HasScreen(OobeScreenId screen_id) {
  return screen_manager_->HasScreen(screen_id);
}

BaseScreen* WizardController::GetScreen(OobeScreenId screen_id) {
  if (screen_id == ErrorScreenView::kScreenId)
    return GetErrorScreen();
  return screen_manager_->GetScreen(screen_id);
}

void WizardController::SetCurrentScreenForTesting(BaseScreen* screen) {
  current_screen_ = screen;
}

void WizardController::SetSharedURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> factory) {
  auto& testing_factory = GetSharedURLLoaderFactoryForTesting();
  testing_factory = std::move(factory);
}

std::vector<std::unique_ptr<BaseScreen>> WizardController::CreateScreens() {
  OobeUI* oobe_ui = GetOobeUI();

  std::vector<std::unique_ptr<BaseScreen>> result;

  auto append = [&](std::unique_ptr<BaseScreen> screen) {
    result.emplace_back(std::move(screen));
  };

  if (oobe_ui->display_type() == OobeUI::kOobeDisplay) {
    append(std::make_unique<WelcomeScreen>(
        oobe_ui->GetView<WelcomeScreenHandler>(),
        base::BindRepeating(&WizardController::OnWelcomeScreenExit,
                            weak_factory_.GetWeakPtr())));

    append(std::make_unique<chromeos::DemoPreferencesScreen>(
        oobe_ui->GetView<DemoPreferencesScreenHandler>(),
        base::BindRepeating(&WizardController::OnDemoPreferencesScreenExit,
                            weak_factory_.GetWeakPtr())));
  }

  append(std::make_unique<NetworkScreen>(
      oobe_ui->GetView<NetworkScreenHandler>(),
      base::BindRepeating(&WizardController::OnNetworkScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<UpdateScreen>(
      oobe_ui->GetView<UpdateScreenHandler>(), oobe_ui->GetErrorScreen(),
      base::BindRepeating(&WizardController::OnUpdateScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<EulaScreen>(
      oobe_ui->GetView<EulaScreenHandler>(),
      base::BindRepeating(&WizardController::OnEulaScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<EnrollmentScreen>(
      oobe_ui->GetView<EnrollmentScreenHandler>(),
      base::BindRepeating(&WizardController::OnEnrollmentScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<chromeos::ResetScreen>(
      oobe_ui->GetView<ResetScreenHandler>(), oobe_ui->GetErrorScreen(),
      base::BindRepeating(&WizardController::OnResetScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<chromeos::DemoSetupScreen>(
      oobe_ui->GetView<DemoSetupScreenHandler>(),
      base::BindRepeating(&WizardController::OnDemoSetupScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<EnableAdbSideloadingScreen>(
      oobe_ui->GetView<EnableAdbSideloadingScreenHandler>(),
      base::BindRepeating(&WizardController::OnEnableAdbSideloadingScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<EnableDebuggingScreen>(
      oobe_ui->GetView<EnableDebuggingScreenHandler>(),
      base::BindRepeating(&WizardController::OnEnableDebuggingScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<KioskEnableScreen>(
      oobe_ui->GetView<KioskEnableScreenHandler>(),
      base::BindRepeating(&WizardController::OnKioskEnableScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<KioskAutolaunchScreen>(
      oobe_ui->GetView<KioskAutolaunchScreenHandler>(),
      base::BindRepeating(&WizardController::OnKioskAutolaunchScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<LocaleSwitchScreen>(
      oobe_ui->GetView<LocaleSwitchScreenHandler>(),
      base::BindRepeating(&WizardController::OnLocaleSwitchScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<TermsOfServiceScreen>(
      oobe_ui->GetView<TermsOfServiceScreenHandler>(),
      base::BindRepeating(&WizardController::OnTermsOfServiceScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<SyncConsentScreen>(
      oobe_ui->GetView<SyncConsentScreenHandler>(),
      base::BindRepeating(&WizardController::OnSyncConsentScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<ArcTermsOfServiceScreen>(
      oobe_ui->GetView<ArcTermsOfServiceScreenHandler>(),
      base::BindRepeating(&WizardController::OnArcTermsOfServiceScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<RecommendAppsScreen>(
      oobe_ui->GetView<RecommendAppsScreenHandler>(),
      base::BindRepeating(&WizardController::OnRecommendAppsScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<AppDownloadingScreen>(
      oobe_ui->GetView<AppDownloadingScreenHandler>(),
      base::BindRepeating(&WizardController::OnAppDownloadingScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<WrongHWIDScreen>(
      oobe_ui->GetView<WrongHWIDScreenHandler>(),
      base::BindRepeating(&WizardController::OnWrongHWIDScreenExit,
                          weak_factory_.GetWeakPtr())));

  if (CanShowHIDDetectionScreen()) {
    append(std::make_unique<chromeos::HIDDetectionScreen>(
        oobe_ui->GetView<HIDDetectionScreenHandler>(),
        base::BindRepeating(&WizardController::OnHidDetectionScreenExit,
                            weak_factory_.GetWeakPtr())));
  }

  append(std::make_unique<AutoEnrollmentCheckScreen>(
      oobe_ui->GetView<AutoEnrollmentCheckScreenHandler>(),
      oobe_ui->GetErrorScreen(),
      base::BindRepeating(&WizardController::OnAutoEnrollmentCheckScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<DeviceDisabledScreen>(
      oobe_ui->GetView<DeviceDisabledScreenHandler>()));
  append(std::make_unique<EncryptionMigrationScreen>(
      oobe_ui->GetView<EncryptionMigrationScreenHandler>()));
  append(std::make_unique<SupervisionTransitionScreen>(
      oobe_ui->GetView<SupervisionTransitionScreenHandler>(),
      base::BindRepeating(&WizardController::OnSupervisionTransitionScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<UpdateRequiredScreen>(
      oobe_ui->GetView<UpdateRequiredScreenHandler>(),
      oobe_ui->GetErrorScreen(),
      base::BindRepeating(&WizardController::OnUpdateRequiredScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<AssistantOptInFlowScreen>(
      oobe_ui->GetView<AssistantOptInFlowScreenHandler>(),
      base::BindRepeating(&WizardController::OnAssistantOptInFlowScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<MultiDeviceSetupScreen>(
      oobe_ui->GetView<MultiDeviceSetupScreenHandler>(),
      base::BindRepeating(&WizardController::OnMultiDeviceSetupScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<PinSetupScreen>(
      oobe_ui->GetView<PinSetupScreenHandler>(),
      base::BindRepeating(&WizardController::OnPinSetupScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<FingerprintSetupScreen>(
      oobe_ui->GetView<FingerprintSetupScreenHandler>(),
      base::BindRepeating(&WizardController::OnFingerprintSetupScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<GestureNavigationScreen>(
      oobe_ui->GetView<GestureNavigationScreenHandler>(),
      base::BindRepeating(&WizardController::OnGestureNavigationScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<MarketingOptInScreen>(
      oobe_ui->GetView<MarketingOptInScreenHandler>(),
      base::BindRepeating(&WizardController::OnMarketingOptInScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<PackagedLicenseScreen>(
      oobe_ui->GetView<PackagedLicenseScreenHandler>(),
      base::BindRepeating(&WizardController::OnPackagedLicenseScreenExit,
                          weak_factory_.GetWeakPtr())));
  auto gaia_screen = std::make_unique<GaiaScreen>(base::BindRepeating(
      &WizardController::OnGaiaScreenExit, weak_factory_.GetWeakPtr()));
  gaia_screen->SetView(oobe_ui->GetView<GaiaScreenHandler>());
  append(std::move(gaia_screen));
  append(std::make_unique<OfflineLoginScreen>(
      oobe_ui->GetView<OfflineLoginScreenHandler>(),
      base::BindRepeating(&WizardController::OnOfflineLoginScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<TpmErrorScreen>(
      oobe_ui->GetView<TpmErrorScreenHandler>()));

  auto gaia_password_change_screen =
      std::make_unique<GaiaPasswordChangedScreen>(
          base::BindRepeating(&WizardController::OnPasswordChangeScreenExit,
                              weak_factory_.GetWeakPtr()),
          oobe_ui->GetView<GaiaPasswordChangedScreenHandler>());
  append(std::move(gaia_password_change_screen));

  append(std::make_unique<ActiveDirectoryPasswordChangeScreen>(
      oobe_ui->GetView<ActiveDirectoryPasswordChangeScreenHandler>(),
      base::BindRepeating(
          &WizardController::OnActiveDirectoryPasswordChangeScreenExit,
          weak_factory_.GetWeakPtr())));

  append(std::make_unique<FamilyLinkNoticeScreen>(
      oobe_ui->GetView<FamilyLinkNoticeScreenHandler>(),
      base::BindRepeating(&WizardController::OnFamilyLinkNoticeScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<UserCreationScreen>(
      oobe_ui->GetView<UserCreationScreenHandler>(), oobe_ui->GetErrorScreen(),
      base::BindRepeating(&WizardController::OnUserCreationScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<ActiveDirectoryLoginScreen>(
      oobe_ui->GetView<ActiveDirectoryLoginScreenHandler>(),
      oobe_ui->GetErrorScreen(),
      base::BindRepeating(&WizardController::OnActiveDirectoryLoginScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<EduCoexistenceLoginScreen>(
      base::BindRepeating(&WizardController::OnEduCoexistenceLoginScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<SignInFatalErrorScreen>(
      oobe_ui->GetView<SignInFatalErrorScreenHandler>(),
      base::BindRepeating(&WizardController::OnSignInFatalErrorScreenExit,
                          weak_factory_.GetWeakPtr())));

  append(std::make_unique<ParentalHandoffScreen>(
      oobe_ui->GetView<ParentalHandoffScreenHandler>(),
      base::BindRepeating(&WizardController::OnParentalHandoffScreenExit,
                          weak_factory_.GetWeakPtr())));

  return result;
}

void WizardController::ShowWelcomeScreen() {
  SetCurrentScreen(GetScreen(WelcomeView::kScreenId));
}

void WizardController::ShowNetworkScreen() {
  SetCurrentScreen(GetScreen(NetworkScreenView::kScreenId));
}

void WizardController::OnOwnershipStatusCheckDone(
    DeviceSettingsService::OwnershipStatus status) {
  if (status == DeviceSettingsService::OWNERSHIP_NONE)
    ShowPackagedLicenseScreen();
  else
    ShowLoginScreen();
}

void WizardController::ShowSignInFatalErrorScreen(
    SignInFatalErrorScreen::Error error,
    const base::Value* params) {
  GetScreen<SignInFatalErrorScreen>()->SetErrorState(error, params);
  AdvanceToScreen(SignInFatalErrorView::kScreenId);
}

void WizardController::OnSignInFatalErrorScreenExit() {
  OnScreenExit(SignInFatalErrorView::kScreenId, kDefaultExitReason);
  AdvanceToScreen(GaiaView::kScreenId);
}

void WizardController::ShowLoginScreen() {
  // This may be triggered by multiply asynchronous events from the JS side.
  if (login_screen_started_)
    return;

  if (!time_eula_accepted_.is_null()) {
    base::TimeDelta delta = base::TimeTicks::Now() - time_eula_accepted_;
    UMA_HISTOGRAM_MEDIUM_TIMES("OOBE.EULAToSignInTime", delta);
  }
  VLOG(1) << "Showing login screen.";
  UpdateStatusAreaVisibilityForScreen(GaiaView::kScreenId);
  GetLoginDisplayHost()->StartSignInScreen();
  login_screen_started_ = true;
}

void WizardController::ShowGaiaPasswordChangedScreen(
    const AccountId& account_id,
    bool has_error) {
  GaiaPasswordChangedScreen* screen = GetScreen<GaiaPasswordChangedScreen>();
  screen->Configure(account_id, has_error);
  if (current_screen_ != screen) {
    SetCurrentScreen(screen);
  } else {
    screen->Show(wizard_context_.get());
  }
}

void WizardController::ShowEulaScreen() {
  SetCurrentScreen(GetScreen(EulaView::kScreenId));
}

void WizardController::ShowEnrollmentScreen() {
  // Update the enrollment configuration and start the screen.
  prescribed_enrollment_config_ = g_browser_process->platform_part()
                                      ->browser_policy_connector_chromeos()
                                      ->GetPrescribedEnrollmentConfig();
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
  if (retry_auto_enrollment_check_)
    screen->ClearState();
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

void WizardController::ShowSupervisionTransitionScreen() {
  SetCurrentScreen(GetScreen(SupervisionTransitionScreenView::kScreenId));
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

void WizardController::ShowActiveDirectoryPasswordChangeScreen(
    const std::string& username) {
  GetScreen<ActiveDirectoryPasswordChangeScreen>()->SetUsername(username);
  AdvanceToScreen(ActiveDirectoryPasswordChangeView::kScreenId);
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
      if (g_browser_process->platform_part()
              ->browser_policy_connector_chromeos()
              ->GetDeviceMode() == policy::DEVICE_MODE_ENTERPRISE_AD) {
        AdvanceToScreen(ActiveDirectoryLoginView::kScreenId);
      } else {
        GetScreen<GaiaScreen>()->LoadOnline(EmptyAccountId());
        AdvanceToScreen(GaiaView::kScreenId);
      }
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
    case UserCreationScreen::Result::CANCEL:
      LoginDisplayHost::default_host()->HideOobeDialog();
      break;
  }
}

void WizardController::OnGaiaScreenExit(GaiaScreen::Result result) {
  OnScreenExit(GaiaView::kScreenId, GaiaScreen::GetResultString(result));
  switch (result) {
    case GaiaScreen::Result::BACK:
    case GaiaScreen::Result::CANCEL:
      if (result == GaiaScreen::Result::BACK &&
          wizard_context_->is_user_creation_enabled) {
        // `Result::BACK` is only triggered when pressing back button. It goes
        // back to UserCreationScreen if screen is enabled; otherwise, it
        // behaves the same as `Result::CANCEL` which is triggered by pressing
        // ESC key.
        AdvanceToScreen(UserCreationView::kScreenId);
        break;
      }
      if (LoginDisplayHost::default_host()->HasUserPods() &&
          !wizard_context_->is_user_creation_enabled) {
        LoginDisplayHost::default_host()->HideOobeDialog();
      } else {
        GetScreen<GaiaScreen>()->LoadOnline(EmptyAccountId());
      }
      break;
    case GaiaScreen::Result::ENTERPRISE_ENROLL:
      ShowEnrollmentScreenIfEligible();
      break;
    case GaiaScreen::Result::START_CONSUMER_KIOSK:
      LoginDisplayHost::default_host()->AttemptShowEnableConsumerKioskScreen();
      break;
  }
}

void WizardController::OnPasswordChangeScreenExit(
    GaiaPasswordChangedScreen::Result result) {
  if (!LoginDisplayHost::default_host())
    return;
  switch (result) {
    case GaiaPasswordChangedScreen::Result::CANCEL:
      LoginDisplayHost::default_host()->CancelPasswordChangedFlow();
      break;
    case GaiaPasswordChangedScreen::Result::RESYNC:
      LoginDisplayHost::default_host()->ResyncUserData();
      break;
    case GaiaPasswordChangedScreen::Result::MIGRATE:
      NOTREACHED();
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
  ShowSyncConsentScreen();
}

void WizardController::OnParentalHandoffScreenExit(
    ParentalHandoffScreen::Result result) {
  OnScreenExit(ParentalHandoffScreenView::kScreenId,
               ParentalHandoffScreen::GetResultString(result));
  ShowMultiDeviceSetupScreen();
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

void WizardController::SkipToLoginForTesting() {
  VLOG(1) << "WizardController::SkipToLoginForTesting()";
  if (current_screen_ && current_screen_->screen_id() == GaiaView::kScreenId)
    return;
  wizard_context_->skip_to_login_for_tests = true;
  StartupUtils::MarkEulaAccepted();

  PerformPostEulaActions();
  OnDeviceDisabledChecked(false /* device_disabled */);
}

void WizardController::SkipToUpdateForTesting() {
  VLOG(1) << "SkipToUpdateForTesting.";
  wizard_context_->skip_to_update_for_tests = true;
  StartupUtils::MarkEulaAccepted();
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableHIDDetectionOnOOBEForTesting)) {
    // We store the flag into local state so it persists restart after the
    // update. Command line switch does not persist the restart during the
    // test.
    StartupUtils::DisableHIDDetectionScreenForTests();
  }
  PerformPostEulaActions();
  InitiateOOBEUpdate();
}

void WizardController::OnScreenExit(OobeScreenId screen,
                                    const std::string& exit_reason) {
  VLOG(1) << "Wizard screen " << screen
          << " exited with reason: " << exit_reason;
  // Do not perform checks and record stats for the skipped screen.
  if (exit_reason == chromeos::BaseScreen::kNotApplicable)
    return;
  DCHECK(current_screen_->screen_id() == screen);

  RecordUMAHistogramForOOBEStepCompletionTime(
      screen, exit_reason, base::TimeTicks::Now() - screen_show_times_[screen]);
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

  if ((result == HIDDetectionScreen::Result::SKIP ||
       result == HIDDetectionScreen::Result::SKIPPED_FOR_TESTS) &&
      current_screen_) {
    return;
  }

  AdvanceToScreenAfterHIDDetection(OobeScreen::SCREEN_UNKNOWN);
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
    case WelcomeScreen::Result::NEXT:
      ShowNetworkScreen();
      return;
  }
}

void WizardController::OnNetworkScreenExit(NetworkScreen::Result result) {
  OnScreenExit(NetworkScreenView::kScreenId,
               NetworkScreen::GetResultString(result));

  if (result == NetworkScreen::Result::BACK) {
    if (demo_setup_controller_) {
      ShowDemoModePreferencesScreen();
    } else {
      ShowWelcomeScreen();
    }
    return;
  }

  // Update the demo setup config for demo setup flow.
  if (demo_setup_controller_) {
    switch (result) {
      case NetworkScreen::Result::CONNECTED:
        demo_setup_controller_->set_demo_config(
            DemoSession::DemoModeConfig::kOnline);
        break;
      case NetworkScreen::Result::OFFLINE_DEMO_SETUP:
        demo_setup_controller_->set_demo_config(
            DemoSession::DemoModeConfig::kOffline);
        break;
      case NetworkScreen::Result::BACK:
        NOTREACHED();
    }
  }

  if (ShowEulaOrArcTosAfterNetworkScreen())
    return;

  switch (result) {
    case NetworkScreen::Result::CONNECTED:
      InitiateOOBEUpdate();
      break;
    case NetworkScreen::Result::OFFLINE_DEMO_SETUP:
      // TODO(agawronska): Maybe check if device is connected to the network
      // and attempt system update. It is possible to initiate offline demo
      // setup on the device that is connected, although it is probably not
      // common.
      ShowDemoModeSetupScreen();
      break;
    case NetworkScreen::Result::BACK:
      NOTREACHED();
  }
}

bool WizardController::ShowEulaOrArcTosAfterNetworkScreen() {
  if (!is_branded_build_)
    return false;

  if (!StartupUtils::IsEulaAccepted()) {
    ShowEulaScreen();
    return true;
  }
  if (arc::IsArcTermsOfServiceOobeNegotiationNeeded()) {
    ShowArcTermsOfServiceScreen();
    return true;
  }

  // This is reachable in case of a reboot during previous OOBE flow after EULA
  // was accepted and ARC terms of service handled - for example due to a forced
  // update (which is the next step, with the exception of offline demo mode
  // setup).
  return false;
}

void WizardController::OnEulaScreenExit(EulaScreen::Result result) {
  OnScreenExit(EulaView::kScreenId, EulaScreen::GetResultString(result));

  switch (result) {
    case EulaScreen::Result::ACCEPTED_WITH_USAGE_STATS_REPORTING:
      OnEulaAccepted(true /*usage_statistics_reporting_enabled*/);
      break;
    case EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING:
    case EulaScreen::Result::NOT_APPLICABLE:
      OnEulaAccepted(false /*usage_statistics_reporting_enabled*/);
      break;
    case EulaScreen::Result::BACK:
      ShowNetworkScreen();
      break;
  }
}

void WizardController::OnEulaAccepted(bool usage_statistics_reporting_enabled) {
  time_eula_accepted_ = base::TimeTicks::Now();
  StartupUtils::MarkEulaAccepted();
  ChangeMetricsReportingStateWithReply(
      usage_statistics_reporting_enabled,
      base::BindOnce(&WizardController::OnChangedMetricsReportingState,
                     weak_factory_.GetWeakPtr()));
  PerformPostEulaActions();

  if (arc::IsArcTermsOfServiceOobeNegotiationNeeded()) {
    ShowArcTermsOfServiceScreen();
    return;
  } else if (demo_setup_controller_) {
    ShowDemoModeSetupScreen();
  }

  InitiateOOBEUpdate();
}

void WizardController::OnUpdateScreenExit(UpdateScreen::Result result) {
  OnScreenExit(UpdateView::kScreenId, UpdateScreen::GetResultString(result));

  switch (result) {
    case UpdateScreen::Result::UPDATE_NOT_REQUIRED:
    case UpdateScreen::Result::UPDATE_SKIPPED:
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
  ShowAutoEnrollmentCheckScreen();
}

void WizardController::OnAutoEnrollmentCheckScreenExit() {
  OnScreenExit(AutoEnrollmentCheckScreenView::kScreenId, kDefaultExitReason);
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
    case EnrollmentScreen::Result::SKIPPED_FOR_TESTS:
      OnEnrollmentDone();
      break;
    case EnrollmentScreen::Result::BACK:
      retry_auto_enrollment_check_ = true;
      ShowAutoEnrollmentCheckScreen();
      break;
  }
}

void WizardController::OnEnrollmentDone() {
  PerformOOBECompletedActions();

  // Fetch the rollback flag from `oobe_configuration_`.
  bool enrollment_mode_rollback = false;
  auto* restore_after_rollback_value =
      wizard_context_->configuration.FindKeyOfType(
          configuration::kRestoreAfterRollback, base::Value::Type::BOOLEAN);
  if (restore_after_rollback_value)
    enrollment_mode_rollback = restore_after_rollback_value->GetBool();

  // Restart to make the login page pick up the policy changes resulting from
  // enrollment recovery.  (Not pretty, but this codepath is rarely exercised.)
  if (prescribed_enrollment_config_.mode ==
          policy::EnrollmentConfig::MODE_RECOVERY ||
      enrollment_mode_rollback) {
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
                 ->browser_policy_connector_chromeos()
                 ->IsEnterpriseManaged()) {
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
      ShowNetworkScreen();
      break;
    case DemoPreferencesScreen::Result::CANCELED:
      demo_setup_controller_.reset();
      ShowWelcomeScreen();
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
      ShowLoginScreen();
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
  AdvanceToScreen(TermsOfServiceScreenView::kScreenId);
}

void WizardController::OnTermsOfServiceScreenExit(
    TermsOfServiceScreen::Result result) {
  OnScreenExit(TermsOfServiceScreenView::kScreenId,
               TermsOfServiceScreen::GetResultString(result));

  switch (result) {
    case TermsOfServiceScreen::Result::ACCEPTED:
    case TermsOfServiceScreen::Result::NOT_APPLICABLE:
      ShowFamilyLinkNoticeScreen();
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
  ShowFingerprintSetupScreen();
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
      OnArcTermsOfServiceAccepted();
      break;
    case ArcTermsOfServiceScreen::Result::NOT_APPLICABLE:
      ShowAssistantOptInFlowScreen();
      break;
    case ArcTermsOfServiceScreen::Result::BACK:
      DCHECK(demo_setup_controller_);
      DCHECK(StartupUtils::IsEulaAccepted());
      ShowNetworkScreen();
      break;
  }
}

void WizardController::OnArcTermsOfServiceAccepted() {
  if (demo_setup_controller_) {
    if (demo_setup_controller_->IsOfflineEnrollment()) {
      ShowDemoModeSetupScreen();
    } else {
      InitiateOOBEUpdate();
    }
    return;
  }
  ShowRecommendAppsScreen();
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
  ShowParentalHandoffScreen();
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

  ShowMarketingOptInScreen();
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
  current_screen_->Hide();
  current_screen_ = nullptr;
  if (previous_screen_) {
    if (IsSigninScreen(previous_screen_->screen_id())) {
      ShowLoginScreen();
    } else {
      SetCurrentScreen(previous_screen_);
    }
    return;
  }
  ShowPackagedLicenseScreen();
}

void WizardController::OnSupervisionTransitionScreenExit() {
  OnScreenExit(SupervisionTransitionScreenView::kScreenId, kDefaultExitReason);

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
      ShowEnrollmentScreen();
      break;
  }
}

void WizardController::OnOobeFlowFinished() {
  SetCurrentScreen(nullptr);

  // Launch browser and delete login host controller.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&UserSessionManager::DoBrowserLaunch,
                                UserSessionManager::GetInstance()->AsWeakPtr(),
                                ProfileManager::GetActiveUserProfile(),
                                GetLoginDisplayHost()));
}

void WizardController::OnDeviceDisabledChecked(bool device_disabled) {
  prescribed_enrollment_config_ = g_browser_process->platform_part()
                                      ->browser_policy_connector_chromeos()
                                      ->GetPrescribedEnrollmentConfig();

  bool configuration_forced_enrollment = false;
  auto* start_enrollment_value = wizard_context_->configuration.FindKeyOfType(
      configuration::kWizardAutoEnroll, base::Value::Type::BOOLEAN);
  if (start_enrollment_value)
    configuration_forced_enrollment = start_enrollment_value->GetBool();

  // Fetch the rollback flag from `configuration`. It is not stored in the
  // `prescribed_enrollment_config_`. To restore after rollback the enrollment
  // screen needs to be started. (crbug.com/1093928)
  auto* restore_after_rollback_value =
      wizard_context_->configuration.FindKeyOfType(
          configuration::kRestoreAfterRollback, base::Value::Type::BOOLEAN);
  if (restore_after_rollback_value)
    configuration_forced_enrollment |= restore_after_rollback_value->GetBool();

  if (device_disabled) {
    demo_setup_controller_.reset();
    ShowDeviceDisabledScreen();
  } else if (demo_setup_controller_) {
    ShowDemoModeSetupScreen();
  } else if (wizard_context_->enrollment_triggered_early ||
             prescribed_enrollment_config_.should_enroll() ||
             configuration_forced_enrollment) {
    VLOG(1) << "StartEnrollment from OnDeviceDisabledChecked("
            << "device_disabled=" << device_disabled << ") "
            << "skip_update_enroll_after_eula_="
            << wizard_context_->enrollment_triggered_early
            << ", prescribed_enrollment_config_.should_enroll()="
            << prescribed_enrollment_config_.should_enroll()
            << ", configuration_forced_enrollment="
            << configuration_forced_enrollment;
    StartEnrollmentScreen(wizard_context_->enrollment_triggered_early);
  } else {
    PerformOOBECompletedActions();
    ShowPackagedLicenseScreen();
  }
}

void WizardController::InitiateOOBEUpdate() {
  // If this is a Cellular First device, instruct UpdateEngine to allow
  // updates over cellular data connections.
  if (chromeos::switches::IsCellularFirstDevice()) {
    DBusThreadManager::Get()
        ->GetUpdateEngineClient()
        ->SetUpdateOverCellularPermission(
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

  DelayNetworkCall(
      base::TimeDelta::FromMilliseconds(kDefaultNetworkRetryDelayMS),
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
      base::TimeDelta::FromSeconds(kResolveTimeZoneTimeoutSeconds),
      false /* send_wifi_geolocation_data */,
      false /* send_cellular_geolocation_data */,
      base::BindOnce(&WizardController::OnLocationResolved,
                     weak_factory_.GetWeakPtr()));
}

void WizardController::PerformPostEulaActions() {
  StartNetworkTimezoneResolve();
  DelayNetworkCall(
      base::TimeDelta::FromMilliseconds(kDefaultNetworkRetryDelayMS),
      ServicesCustomizationDocument::GetInstance()
          ->EnsureCustomizationAppliedClosure());

  // Now that EULA has been accepted (for official builds), enable portal check.
  // ChromiumOS builds would go though this code path too.
  NetworkHandler::Get()->network_state_handler()->SetCheckPortalList(
      NetworkStateHandler::kDefaultCheckPortalList);
  GetAutoEnrollmentController()->Start();
  network_portal_detector::GetInstance()->Enable(true);
}

void WizardController::PerformOOBECompletedActions() {
  // Avoid marking OOBE as completed multiple times if going from login screen
  // to enrollment screen (and back).
  if (oobe_marked_completed_) {
    return;
  }

  UMA_HISTOGRAM_COUNTS_100(
      "HIDDetection.TimesDialogShownPerOOBECompleted",
      GetLocalState()->GetInteger(::prefs::kTimesHIDDialogShown));
  GetLocalState()->ClearPref(::prefs::kTimesHIDDialogShown);
  StartupUtils::MarkOobeCompleted();
  oobe_marked_completed_ = true;
}

void WizardController::SetCurrentScreen(BaseScreen* new_current) {
  VLOG(1) << "SetCurrentScreen: "
          << (new_current ? new_current->screen_id().name : "null");

  if (new_current && new_current->MaybeSkip(wizard_context_.get())) {
    RecordUMAHistogramForOOBEStepShownStatus(new_current->screen_id(),
                                             ScreenShownStatus::kSkipped);
    return;
  }

  if (current_screen_ == new_current || GetOobeUI() == nullptr)
    return;

  if (current_screen_) {
    current_screen_->Hide();
  }

  previous_screen_ = current_screen_;
  current_screen_ = new_current;

  if (!current_screen_) {
    NotifyScreenChanged();
    return;
  }

  // Record show time for UMA.
  screen_show_times_[new_current->screen_id()] = base::TimeTicks::Now();

  // First remember how far have we reached so that we can resume if needed.
  if (is_out_of_box_ && !demo_setup_controller_ &&
      IsResumableScreen(current_screen_->screen_id())) {
    StartupUtils::SaveOobePendingScreen(current_screen_->screen_id().name);
  }

  UpdateStatusAreaVisibilityForScreen(current_screen_->screen_id());
  RecordUMAHistogramForOOBEStepShownStatus(current_screen_->screen_id(),
                                           ScreenShownStatus::kShown);
  current_screen_->Show(wizard_context_.get());
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
  if (!GetOobeUI())
    return;

  // Check for tests configurations.
  if (wizard_context_->skip_to_update_for_tests ||
      wizard_context_->skip_to_login_for_tests) {
    return;
  }

  if (screen_needed)
    ShowHIDDetectionScreen();
  else
    AdvanceToScreenAfterHIDDetection(OobeScreen::SCREEN_UNKNOWN);
}

void WizardController::UpdateOobeConfiguration() {
  wizard_context_->configuration = base::Value(base::Value::Type::DICTIONARY);
  chromeos::configuration::FilterConfiguration(
      OobeConfiguration::Get()->GetConfiguration(),
      chromeos::configuration::ConfigurationHandlerSide::HANDLER_CPP,
      wizard_context_->configuration);
  auto* requisition_value = wizard_context_->configuration.FindKeyOfType(
      configuration::kDeviceRequisition, base::Value::Type::STRING);
  if (requisition_value) {
    VLOG(1) << "Using Device Requisition from configuration"
            << requisition_value->GetString();
    policy::EnrollmentRequisitionManager::SetDeviceRequisition(
        requisition_value->GetString());
  }
}

bool WizardController::CanNavigateTo(OobeScreenId screen_id) {
  if (!current_screen_)
    return true;
  BaseScreen* next_screen = GetScreen(screen_id);
  return next_screen->screen_priority() <= current_screen_->screen_priority();
}

void WizardController::AdvanceToScreen(OobeScreenId screen_id) {
  if (!CanNavigateTo(screen_id)) {
    LOG(WARNING) << "Cannot advance to screen : " << screen_id
                 << " as it's priority is less than the current screen : "
                 << current_screen_->screen_id();
    return;
  }
  login_screen_started_ = false;

  if (screen_id == WelcomeView::kScreenId) {
    ShowWelcomeScreen();
  } else if (screen_id == NetworkScreenView::kScreenId) {
    ShowNetworkScreen();
  } else if (screen_id == PackagedLicenseView::kScreenId) {
    ShowPackagedLicenseScreen();
  } else if (screen_id == UpdateView::kScreenId) {
    InitiateOOBEUpdate();
  } else if (screen_id == EulaView::kScreenId) {
    ShowEulaScreen();
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
  } else if (screen_id == SyncConsentScreenView::kScreenId) {
    ShowSyncConsentScreen();
  } else if (screen_id == ArcTermsOfServiceScreenView::kScreenId) {
    ShowArcTermsOfServiceScreen();
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
  } else if (screen_id == SupervisionTransitionScreenView::kScreenId) {
    ShowSupervisionTransitionScreen();
  } else if (screen_id == TpmErrorView::kScreenId ||
             screen_id == GaiaPasswordChangedView::kScreenId ||
             screen_id == ActiveDirectoryPasswordChangeView::kScreenId ||
             screen_id == FamilyLinkNoticeView::kScreenId ||
             screen_id == GaiaView::kScreenId ||
             screen_id == UserCreationView::kScreenId ||
             screen_id == ActiveDirectoryLoginView::kScreenId ||
             screen_id == SignInFatalErrorView::kScreenId ||
             screen_id == LocaleSwitchView::kScreenId ||
             screen_id == OfflineLoginView::kScreenId) {
    SetCurrentScreen(GetScreen(screen_id));
  } else {
    NOTREACHED();
  }
}

bool WizardController::HandleAccelerator(ash::LoginAcceleratorAction action) {
  if (current_screen_) {
    if (current_screen_->HandleAccelerator(action))
      return true;
  }
  return false;
}

void WizardController::StartDemoModeSetup() {
  demo_setup_controller_ = std::make_unique<DemoSetupController>();
  ShowDemoModePreferencesScreen();
}

void WizardController::SimulateDemoModeSetupForTesting(
    base::Optional<DemoSession::DemoModeConfig> demo_config) {
  if (!demo_setup_controller_)
    demo_setup_controller_ = std::make_unique<DemoSetupController>();
  if (demo_config.has_value())
    demo_setup_controller_->set_demo_config(*demo_config);
}

void WizardController::SetAuthSessionForOnboarding(
    const UserContext& auth_session) {
  wizard_context_->extra_factors_auth_session =
      std::make_unique<UserContext>(auth_session);
}

void WizardController::ShowErrorScreen() {
  SetCurrentScreen(GetScreen(ErrorScreenView::kScreenId));
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
  if (status == CrosSettingsProvider::TEMPORARILY_UNTRUSTED)
    return;

  if (status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
    // If the `cros_settings_` are permanently untrusted, show an error message
    // and refuse to auto-launch the kiosk app.
    GetErrorScreen()->SetUIState(NetworkError::UI_STATE_LOCAL_STATE_ERROR);
    GetLoginDisplayHost()->SetStatusAreaVisible(false);
    ShowErrorScreen();
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

// static
void WizardController::SkipPostLoginScreensForTesting() {
  skip_post_login_screens_ = true;
  if (!default_controller() || !default_controller()->current_screen())
    return;

  const OobeScreenId current_screen_id =
      default_controller()->current_screen()->screen_id();
  if (current_screen_id == TermsOfServiceScreenView::kScreenId ||
      current_screen_id == FamilyLinkNoticeView::kScreenId ||
      current_screen_id == EduCoexistenceLoginScreen::kScreenId ||
      current_screen_id == SyncConsentScreenView::kScreenId ||
      current_screen_id == FingerprintSetupScreenView::kScreenId ||
      current_screen_id == ArcTermsOfServiceScreenView::kScreenId ||
      current_screen_id == PinSetupScreenView::kScreenId ||
      current_screen_id == MarketingOptInScreenView::kScreenId ||
      current_screen_id == ParentalHandoffScreenView::kScreenId) {
    default_controller()->OnOobeFlowFinished();
  } else {
    LOG(WARNING) << "SkipPostLoginScreensForTesting(): Ignore screen "
                 << current_screen_id.name;
  }
}

// static
void WizardController::SkipEnrollmentPromptsForTesting() {
  skip_enrollment_prompts_ = true;
}

// static
std::unique_ptr<base::AutoReset<bool>>
WizardController::ForceBrandedBuildForTesting(bool value) {
  return std::make_unique<base::AutoReset<bool>>(&is_branded_build_, value);
}

// static
bool WizardController::UsingHandsOffEnrollment() {
  return policy::DeviceCloudPolicyManagerChromeOS::
             GetZeroTouchEnrollmentMode() ==
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
  GetErrorScreen()->SetUIState(NetworkError::UI_STATE_LOCAL_STATE_ERROR);
  GetLoginDisplayHost()->SetStatusAreaVisible(false);
  ShowErrorScreen();
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
  if (local_state_for_testing_)
    return local_state_for_testing_;
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

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (connector->IsEnterpriseManaged()) {
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
    chromeos::system::SetSystemAndSigninScreenTimezone(timezone->timeZoneId);
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

  const base::TimeDelta timeout =
      base::TimeDelta::FromSeconds(kResolveTimeZoneTimeoutSeconds);
  // Ignore invalid position.
  if (!position.Valid())
    return;

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
  if (timezone_resolved_)
    return false;

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

  // If chrome version is rolled back via policy, the device is actually
  // enrolled but some enrollment-flow steps still need to be taken.
  auto* restore_after_rollback_value =
      wizard_context_->configuration.FindKeyOfType(
          configuration::kRestoreAfterRollback, base::Value::Type::BOOLEAN);
  if (restore_after_rollback_value && restore_after_rollback_value->GetBool())
    effective_config.mode = policy::EnrollmentConfig::MODE_ENROLLED_ROLLBACK;

  // If enrollment token is specified via OOBE configuration use corresponding
  // configuration.
  auto* enrollment_token = wizard_context_->configuration.FindKeyOfType(
      configuration::kEnrollmentToken, base::Value::Type::STRING);
  if (enrollment_token && !enrollment_token->GetString().empty()) {
    effective_config.mode =
        policy::EnrollmentConfig::MODE_ATTESTATION_ENROLLMENT_TOKEN;
    effective_config.auth_mechanism =
        policy::EnrollmentConfig::AUTH_MECHANISM_ATTESTATION;
    effective_config.enrollment_token = enrollment_token->GetString();
  }

  EnrollmentScreen* screen = EnrollmentScreen::Get(screen_manager());
  screen->SetEnrollmentConfig(effective_config);
  UpdateStatusAreaVisibilityForScreen(EnrollmentScreenView::kScreenId);
  SetCurrentScreen(screen);
}

void WizardController::ShowEnrollmentScreenIfEligible() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  const bool enterprise_managed = connector->IsEnterpriseManaged();
  const bool has_users = !user_manager::UserManager::Get()->GetUsers().empty();
  if (!has_users && !enterprise_managed) {
    AdvanceToScreen(EnrollmentScreenView::kScreenId);
  }
}

void WizardController::NotifyScreenChanged() {
  for (ScreenObserver& obs : screen_observers_)
    obs.OnCurrentScreenChanged(current_screen_);
}

AutoEnrollmentController* WizardController::GetAutoEnrollmentController() {
  if (!auto_enrollment_controller_)
    auto_enrollment_controller_ = std::make_unique<AutoEnrollmentController>();
  return auto_enrollment_controller_.get();
}

}  // namespace chromeos
