// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wizard_controller.h"

#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>

#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/login/configuration_keys.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_check_screen.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/hwid_checker.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/login/screens/app_downloading_screen.h"
#include "chrome/browser/chromeos/login/screens/arc_terms_of_service_screen.h"
#include "chrome/browser/chromeos/login/screens/assistant_optin_flow_screen.h"
#include "chrome/browser/chromeos/login/screens/demo_preferences_screen.h"
#include "chrome/browser/chromeos/login/screens/demo_setup_screen.h"
#include "chrome/browser/chromeos/login/screens/device_disabled_screen.h"
#include "chrome/browser/chromeos/login/screens/discover_screen.h"
#include "chrome/browser/chromeos/login/screens/enable_adb_sideloading_screen.h"
#include "chrome/browser/chromeos/login/screens/enable_debugging_screen.h"
#include "chrome/browser/chromeos/login/screens/encryption_migration_screen.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/screens/eula_screen.h"
#include "chrome/browser/chromeos/login/screens/fingerprint_setup_screen.h"
#include "chrome/browser/chromeos/login/screens/hid_detection_screen.h"
#include "chrome/browser/chromeos/login/screens/kiosk_autolaunch_screen.h"
#include "chrome/browser/chromeos/login/screens/kiosk_enable_screen.h"
#include "chrome/browser/chromeos/login/screens/marketing_opt_in_screen.h"
#include "chrome/browser/chromeos/login/screens/multidevice_setup_screen.h"
#include "chrome/browser/chromeos/login/screens/network_error.h"
#include "chrome/browser/chromeos/login/screens/network_screen.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps_screen.h"
#include "chrome/browser/chromeos/login/screens/reset_screen.h"
#include "chrome/browser/chromeos/login/screens/supervision_transition_screen.h"
#include "chrome/browser/chromeos/login/screens/sync_consent_screen.h"
#include "chrome/browser/chromeos/login/screens/update_required_screen.h"
#include "chrome/browser/chromeos/login/screens/update_screen.h"
#include "chrome/browser/chromeos/login/screens/welcome_screen.h"
#include "chrome/browser/chromeos/login/screens/wrong_hwid_screen.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/chromeos/net/delay_network_call.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/stats_reporting_controller.h"
#include "chrome/browser/chromeos/system/device_disabling_manager.h"
#include "chrome/browser/chromeos/system/timezone_resolver_manager.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/webui/chromeos/login/app_downloading_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/arc_kiosk_splash_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/arc_terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/assistant_optin_flow_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/auto_enrollment_check_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_preferences_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_setup_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/device_disabled_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/discover_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/encryption_migration_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/enrollment_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/eula_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/fingerprint_setup_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/hid_detection_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/kiosk_autolaunch_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/kiosk_enable_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/marketing_opt_in_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/multidevice_setup_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/recommend_apps_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/reset_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/supervision_transition_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/sync_consent_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/update_required_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/wrong_hwid_screen_handler.h"
#include "chrome/browser/ui/webui/help/help_utils_chromeos.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/assistant/buildflags.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/constants/chromeos_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/constants/devicetype.h"
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
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/crash/content/app/breakpad_linux.h"
#include "components/crash/content/app/crashpad.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/service_manager_connection.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/accelerators/accelerator.h"

using content::BrowserThread;

namespace {

bool g_using_zero_delays = false;

// Total timezone resolving process timeout.
const unsigned int kResolveTimeZoneTimeoutSeconds = 60;

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
    chromeos::ArcTermsOfServiceScreenView::kScreenId,
    chromeos::AutoEnrollmentCheckScreenView::kScreenId,
    chromeos::RecommendAppsScreenView::kScreenId,
    chromeos::AppDownloadingScreenView::kScreenId,
    chromeos::DiscoverScreenView::kScreenId,
    chromeos::MarketingOptInScreenView::kScreenId,
    chromeos::MultiDeviceSetupScreenView::kScreenId,
};

const chromeos::StaticOobeScreenId kScreensWithHiddenStatusArea[] = {
    chromeos::ArcKioskSplashScreenView::kScreenId,
    chromeos::EnableAdbSideloadingScreenView::kScreenId,
    chromeos::EnableDebuggingScreenView::kScreenId,
    chromeos::KioskAutolaunchScreenView::kScreenId,
    chromeos::KioskEnableScreenView::kScreenId,
    chromeos::ResetView::kScreenId,
    chromeos::SupervisionTransitionScreenView::kScreenId,
    chromeos::WrongHWIDScreenView::kScreenId,
};

// The HID detection screen is only allowed for form factors without built-in
// inputs: Chromebases, Chromebits, and Chromeboxes (crbug.com/965765).
bool CanShowHIDDetectionScreen() {
  switch (chromeos::GetDeviceType()) {
    case chromeos::DeviceType::kChromebase:
    case chromeos::DeviceType::kChromebit:
    case chromeos::DeviceType::kChromebox:
      return !base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableHIDDetectionOnOOBE);
    default:
      return false;
  }
}

bool IsResumableScreen(chromeos::OobeScreenId screen) {
  for (const auto& resumable_screen : kResumableScreens) {
    if (screen == resumable_screen)
      return true;
  }
  return false;
}

bool ShouldHideStatusArea(chromeos::OobeScreenId screen) {
  for (const auto& s : kScreensWithHiddenStatusArea) {
    if (screen == s)
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

void RecordUMAHistogramForOOBEStepCompletionTime(chromeos::OobeScreenId screen,
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
  // can not be used here, because |histogram_name| is calculated dynamically
  // and changes from call to call.
  base::HistogramBase* histogram = base::Histogram::FactoryTimeGet(
      histogram_name, base::TimeDelta::FromMilliseconds(10),
      base::TimeDelta::FromMinutes(3), 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddTime(step_time);
}

bool IsRemoraRequisition() {
  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetDeviceCloudPolicyManager();
  return policy_manager && policy_manager->IsRemoraRequisition();
}

// Return false if the logged in user is a managed or child account. Otherwise,
// return true if the feature flag for recommend app screen is on.
bool ShouldShowRecommendAppsScreen() {
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  DCHECK(user_manager->IsUserLoggedIn());
  bool is_managed_account = ProfileManager::GetActiveUserProfile()
                                ->GetProfilePolicyConnector()
                                ->IsManaged();
  bool is_child_account = user_manager->IsLoggedInAsChildUser();
  return !is_managed_account && !is_child_account;
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
      network_state_helper_(std::make_unique<login::NetworkStateHelper>()) {
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  if (accessibility_manager) {
    // accessibility_manager could be null in Tests.
    accessibility_subscription_ = accessibility_manager->RegisterCallback(
        base::Bind(&WizardController::OnAccessibilityStatusChanged,
                   weak_factory_.GetWeakPtr()));
  }
}

WizardController::~WizardController() {
  screen_manager_.reset();
}

void WizardController::Init(OobeScreenId first_screen) {
  screen_manager_->Init(CreateScreens());

  VLOG(1) << "Starting OOBE wizard with screen: " << first_screen;
  first_screen_ = first_screen;

  bool oobe_complete = StartupUtils::IsOobeCompleted();
  if (!oobe_complete)
    UpdateOobeConfiguration();
  if (!oobe_complete || first_screen == OobeScreen::SCREEN_SPECIAL_OOBE)
    is_out_of_box_ = true;

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
  if (!connector->IsEnterpriseManaged()) {
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
  if (CrosSettings::IsInitialized()) {
    guest_mode_policy_subscription_ = CrosSettings::Get()->AddSettingsObserver(
        kAccountsPrefAllowGuest,
        base::BindRepeating(&WizardController::OnGuestModePolicyUpdated,
                            weak_factory_.GetWeakPtr()));
  }

  // Use the saved screen preference from Local State.
  const std::string screen_pref =
      GetLocalState()->GetString(prefs::kOobeScreenPending);
  if (is_out_of_box_ && !screen_pref.empty() &&
      (first_screen == OobeScreen::SCREEN_UNKNOWN ||
       first_screen == OobeScreen::SCREEN_TEST_NO_WINDOW)) {
    first_screen_ = OobeScreenId(screen_pref);
  }

  AdvanceToScreen(first_screen_);
  if (!IsMachineHWIDCorrect() && !StartupUtils::IsDeviceRegistered() &&
      first_screen_ == OobeScreen::SCREEN_UNKNOWN)
    ShowWrongHWIDScreen();

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kOobeSkipToLogin)) {
    SkipToLoginForTesting(LoginScreenContext());
  }
}

ErrorScreen* WizardController::GetErrorScreen() {
  return GetOobeUI()->GetErrorScreen();
}

bool WizardController::HasScreen(OobeScreenId screen) {
  return screen_manager_->HasScreen(screen);
}

BaseScreen* WizardController::GetScreen(OobeScreenId screen) {
  if (screen == ErrorScreenView::kScreenId)
    return GetErrorScreen();
  return screen_manager_->GetScreen(screen);
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
  append(std::make_unique<chromeos::DemoPreferencesScreen>(
      oobe_ui->GetView<DemoPreferencesScreenHandler>(),
      base::BindRepeating(&WizardController::OnDemoPreferencesScreenExit,
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
      oobe_ui->GetErrorScreen()));
  append(std::make_unique<AssistantOptInFlowScreen>(
      oobe_ui->GetView<AssistantOptInFlowScreenHandler>(),
      base::BindRepeating(&WizardController::OnAssistantOptInFlowScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<MultiDeviceSetupScreen>(
      oobe_ui->GetView<MultiDeviceSetupScreenHandler>(),
      base::BindRepeating(&WizardController::OnMultiDeviceSetupScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<DiscoverScreen>(
      oobe_ui->GetView<DiscoverScreenHandler>(),
      base::BindRepeating(&WizardController::OnDiscoverScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<FingerprintSetupScreen>(
      oobe_ui->GetView<FingerprintSetupScreenHandler>(),
      base::BindRepeating(&WizardController::OnFingerprintSetupScreenExit,
                          weak_factory_.GetWeakPtr())));
  append(std::make_unique<MarketingOptInScreen>(
      oobe_ui->GetView<MarketingOptInScreenHandler>(),
      base::BindRepeating(&WizardController::OnMarketingOptInScreenExit,
                          weak_factory_.GetWeakPtr())));

  return result;
}

void WizardController::ShowWelcomeScreen() {
  SetCurrentScreen(GetScreen(WelcomeView::kScreenId));
}

void WizardController::ShowNetworkScreen() {
  SetCurrentScreen(GetScreen(NetworkScreenView::kScreenId));
}

void WizardController::ShowLoginScreen(const LoginScreenContext& context) {
  // This may be triggered by multiply asynchronous events from the JS side.
  if (login_screen_started_)
    return;

  if (!time_eula_accepted_.is_null()) {
    base::TimeDelta delta = base::Time::Now() - time_eula_accepted_;
    UMA_HISTOGRAM_MEDIUM_TIMES("OOBE.EULAToSignInTime", delta);
  }
  VLOG(1) << "Showing login screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_SPECIAL_LOGIN);
  GetLoginDisplayHost()->StartSignInScreen(context);
  login_screen_started_ = true;
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
  // Only show the Terms of Service when logging into a public account and Terms
  // of Service have been specified through policy. In all other cases, advance
  // to the post-ToS part immediately.
  if (!user_manager::UserManager::Get()->IsLoggedInAsPublicAccount() ||
      !ProfileManager::GetActiveUserProfile()->GetPrefs()->IsManagedPreference(
          prefs::kTermsOfServiceURL)) {
    OnTermsOfServiceAccepted();
    return;
  }

  SetCurrentScreen(GetScreen(TermsOfServiceScreenView::kScreenId));
}

void WizardController::ShowSyncConsentScreen() {
  if (is_branded_build_)
    SetCurrentScreen(GetScreen(SyncConsentScreenView::kScreenId));
  else
    OnSyncConsentFinished();
}

void WizardController::ShowFingerprintSetupScreen() {
  SetCurrentScreen(GetScreen(FingerprintSetupScreenView::kScreenId));
}

void WizardController::ShowMarketingOptInScreen() {
  SetCurrentScreen(GetScreen(MarketingOptInScreenView::kScreenId));
}

void WizardController::ShowArcTermsOfServiceScreen() {
  if (arc::IsArcTermsOfServiceOobeNegotiationNeeded()) {
    SetCurrentScreen(GetScreen(ArcTermsOfServiceScreenView::kScreenId));
    ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
        arc::prefs::kArcTermsShownInOobe, true);
  } else {
    ShowAssistantOptInFlowScreen();
  }
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
  AutoEnrollmentCheckScreen* screen =
      AutoEnrollmentCheckScreen::Get(screen_manager());
  if (retry_auto_enrollment_check_)
    screen->ClearState();
  screen->set_auto_enrollment_controller(GetAutoEnrollmentController());
  SetCurrentScreen(screen);
}

void WizardController::ShowArcKioskSplashScreen() {
  SetCurrentScreen(GetScreen(ArcKioskSplashScreenView::kScreenId));
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
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
  UpdateStatusAreaVisibilityForScreen(AssistantOptInFlowScreenView::kScreenId);
  SetCurrentScreen(GetScreen(AssistantOptInFlowScreenView::kScreenId));
#else
  ShowMultiDeviceSetupScreen();
#endif
}

void WizardController::ShowMultiDeviceSetupScreen() {
  SetCurrentScreen(GetScreen(MultiDeviceSetupScreenView::kScreenId));
}

void WizardController::ShowDiscoverScreen() {
  SetCurrentScreen(GetScreen(DiscoverScreenView::kScreenId));
}

void WizardController::SkipToLoginForTesting(
    const LoginScreenContext& context) {
  VLOG(1) << "SkipToLoginForTesting.";
  StartupUtils::MarkEulaAccepted();

  // Enable metrics and crash collection, and verify that they're enabled.
  ChangeMetricsReportingStateWithReply(
      true,
      base::BindRepeating(&WizardController::OnChangedMetricsReportingState,
                          weak_factory_.GetWeakPtr()));
  if (!StatsReportingController::Get()->IsEnabled()) {
    LOG(ERROR) << "StatsReportingController reports collection is NOT enabled";
  }
  if (!crash_reporter::GetUploadsEnabled()) {
    LOG(ERROR) << "crash_reporter reports that crash uploads NOT enabled";
  }

  PerformPostEulaActions();
  OnDeviceDisabledChecked(false /* device_disabled */);
}

void WizardController::SkipToUpdateForTesting() {
  VLOG(1) << "SkipToUpdateForTesting.";
  StartupUtils::MarkEulaAccepted();
  PerformPostEulaActions();
  InitiateOOBEUpdate();
}

void WizardController::SkipUpdateEnrollAfterEula() {
  skip_update_enroll_after_eula_ = true;
}

void WizardController::OnScreenExit(OobeScreenId screen, int exit_code) {
  DCHECK(current_screen_->screen_id() == screen);

  VLOG(1) << "Wizard screen " << screen << " exited with code: " << exit_code;

  RecordUMAHistogramForOOBEStepCompletionTime(
      screen, base::Time::Now() - screen_show_times_[screen]);
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, ExitHandlers:
void WizardController::OnWrongHWIDScreenExit() {
  OnScreenExit(WrongHWIDScreenView::kScreenId, 0 /* exit_code */);

  if (previous_screen_) {
    SetCurrentScreen(previous_screen_);
  } else {
    ShowLoginScreen(LoginScreenContext());
  }
}

void WizardController::OnHidDetectionScreenExit() {
  OnScreenExit(HIDDetectionView::kScreenId, 0 /* exit_code */);

  // Check for tests configuration.
  if (!StartupUtils::IsOobeCompleted())
    ShowWelcomeScreen();
}

void WizardController::OnWelcomeScreenExit() {
  OnScreenExit(WelcomeView::kScreenId, 0 /* exit_code */);

  ShowNetworkScreen();
}

void WizardController::OnNetworkScreenExit(NetworkScreen::Result result) {
  OnScreenExit(NetworkScreenView::kScreenId, static_cast<int>(result));

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
  OnScreenExit(EulaView::kScreenId, static_cast<int>(result));

  switch (result) {
    case EulaScreen::Result::ACCEPTED_WITH_USAGE_STATS_REPORTING:
      OnEulaAccepted(true /*usage_statistics_reporting_enabled*/);
      break;
    case EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING:
      OnEulaAccepted(false /*usage_statistics_reporting_enabled*/);
      break;
    case EulaScreen::Result::BACK:
      ShowNetworkScreen();
      break;
  }
}

void WizardController::OnEulaAccepted(bool usage_statistics_reporting_enabled) {
  time_eula_accepted_ = base::Time::Now();
  StartupUtils::MarkEulaAccepted();
  ChangeMetricsReportingStateWithReply(
      usage_statistics_reporting_enabled,
      base::BindRepeating(&WizardController::OnChangedMetricsReportingState,
                          weak_factory_.GetWeakPtr()));
  PerformPostEulaActions();

  if (arc::IsArcTermsOfServiceOobeNegotiationNeeded()) {
    ShowArcTermsOfServiceScreen();
    return;
  } else if (demo_setup_controller_) {
    ShowDemoModeSetupScreen();
  }

  if (skip_update_enroll_after_eula_) {
    ShowAutoEnrollmentCheckScreen();
  } else {
    InitiateOOBEUpdate();
  }
}

void WizardController::OnUpdateScreenExit(UpdateScreen::Result result) {
  OnScreenExit(UpdateView::kScreenId, static_cast<int>(result));

  switch (result) {
    case UpdateScreen::Result::UPDATE_NOT_REQUIRED:
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
  OnScreenExit(AutoEnrollmentCheckScreenView::kScreenId, 0 /* exit_code */);

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
  OnScreenExit(EnrollmentScreenView::kScreenId, static_cast<int>(result));

  switch (result) {
    case EnrollmentScreen::Result::COMPLETED:
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

  // Restart to make the login page pick up the policy changes resulting from
  // enrollment recovery.  (Not pretty, but this codepath is rarely exercised.)
  if (prescribed_enrollment_config_.mode ==
          policy::EnrollmentConfig::MODE_RECOVERY ||
      prescribed_enrollment_config_.mode ==
          policy::EnrollmentConfig::MODE_ENROLLED_ROLLBACK) {
    LOG(WARNING) << "Restart Chrome to pick up the policy changes";
    chrome::AttemptRestart();
    return;
  }

  // We need a log to understand when the device finished enrollment.
  VLOG(1) << "Enrollment done";

  if (KioskAppManager::Get()->IsAutoLaunchEnabled())
    AutoLaunchKioskApp();
  else
    ShowLoginScreen(LoginScreenContext());
}

void WizardController::OnEnableAdbSideloadingScreenExit() {
  OnScreenExit(EnableAdbSideloadingScreenView::kScreenId, 0 /* exit_code */);

  OnDeviceModificationCanceled();
}

void WizardController::OnEnableDebuggingScreenExit() {
  OnScreenExit(EnableDebuggingScreenView::kScreenId, 0 /* exit_code */);

  OnDeviceModificationCanceled();
}

void WizardController::OnKioskEnableScreenExit() {
  OnScreenExit(KioskEnableScreenView::kScreenId, 0 /* exit_code */);

  ShowLoginScreen(LoginScreenContext());
}

void WizardController::OnKioskAutolaunchScreenExit(
    KioskAutolaunchScreen::Result result) {
  OnScreenExit(KioskAutolaunchScreenView::kScreenId, 0 /* exit_code */);

  switch (result) {
    case KioskAutolaunchScreen::Result::COMPLETED:
      DCHECK(KioskAppManager::Get()->IsAutoLaunchEnabled());
      AutoLaunchKioskApp();
      break;
    case KioskAutolaunchScreen::Result::CANCELED:
      ShowLoginScreen(LoginScreenContext());
      break;
  }
}

void WizardController::OnDemoPreferencesScreenExit(
    DemoPreferencesScreen::Result result) {
  OnScreenExit(DemoPreferencesScreenView::kScreenId, static_cast<int>(result));

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
  OnScreenExit(DemoSetupScreenView::kScreenId, static_cast<int>(result));

  DCHECK(demo_setup_controller_);
  demo_setup_controller_.reset();

  switch (result) {
    case DemoSetupScreen::Result::COMPLETED:
      PerformOOBECompletedActions();
      ShowLoginScreen(LoginScreenContext());
      break;
    case DemoSetupScreen::Result::CANCELED:
      ShowWelcomeScreen();
      break;
  }
}

void WizardController::OnTermsOfServiceScreenExit(
    TermsOfServiceScreen::Result result) {
  OnScreenExit(TermsOfServiceScreenView::kScreenId, static_cast<int>(result));

  switch (result) {
    case TermsOfServiceScreen::Result::ACCEPTED:
      OnTermsOfServiceAccepted();
      break;
    case TermsOfServiceScreen::Result::DECLINED:
      // End the session and return to the login screen.
      SessionManagerClient::Get()->StopSession();
      break;
  }
}

void WizardController::OnTermsOfServiceAccepted() {
  ShowSyncConsentScreen();
}

void WizardController::OnSyncConsentScreenExit() {
  OnScreenExit(SyncConsentScreenView::kScreenId, 0 /* exit_code */);
  OnSyncConsentFinished();
}

void WizardController::OnSyncConsentFinished() {
  ShowFingerprintSetupScreen();
}

void WizardController::OnFingerprintSetupScreenExit() {
  OnScreenExit(FingerprintSetupScreenView::kScreenId, 0 /* exit_code */);

  ShowDiscoverScreen();
}

void WizardController::OnDiscoverScreenExit() {
  OnScreenExit(DiscoverScreenView::kScreenId, 0 /* exit_code */);
  ShowMarketingOptInScreen();
}

void WizardController::OnMarketingOptInScreenExit() {
  OnScreenExit(MarketingOptInScreenView::kScreenId, 0 /* exit_code */);
  ShowArcTermsOfServiceScreen();
}

void WizardController::OnArcTermsOfServiceScreenExit(
    ArcTermsOfServiceScreen::Result result) {
  OnScreenExit(ArcTermsOfServiceScreenView::kScreenId,
               static_cast<int>(result));

  switch (result) {
    case ArcTermsOfServiceScreen::Result::ACCEPTED:
      OnArcTermsOfServiceAccepted();
      break;
    case ArcTermsOfServiceScreen::Result::SKIPPED:
      OnArcTermsOfServiceSkipped();
      break;
    case ArcTermsOfServiceScreen::Result::BACK:
      DCHECK(demo_setup_controller_);
      DCHECK(StartupUtils::IsEulaAccepted());
      ShowNetworkScreen();
      break;
  }
}

void WizardController::OnArcTermsOfServiceSkipped() {
  // If the user finished with the PlayStore Terms of Service, advance to the
  // assistant opt-in flow screen.
  ShowAssistantOptInFlowScreen();
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

  // If the recommend app screen should be shown, show it after the user
  // accepted the Arc TOS. Otherwise, advance to the assistant opt-in flow
  // screen.
  if (ShouldShowRecommendAppsScreen()) {
    ShowRecommendAppsScreen();
    return;
  }

  ShowAssistantOptInFlowScreen();
}

void WizardController::OnRecommendAppsScreenExit(
    RecommendAppsScreen::Result result) {
  OnScreenExit(RecommendAppsScreenView::kScreenId, static_cast<int>(result));

  switch (result) {
    case RecommendAppsScreen::Result::SELECTED:
      ShowAppDownloadingScreen();
      break;
    case RecommendAppsScreen::Result::SKIPPED:
      ShowAssistantOptInFlowScreen();
      break;
  }
}

void WizardController::OnAppDownloadingScreenExit() {
  OnScreenExit(AppDownloadingScreenView::kScreenId, 0 /* exit_code */);

  ShowAssistantOptInFlowScreen();
}

void WizardController::OnAssistantOptInFlowScreenExit() {
  OnScreenExit(AssistantOptInFlowScreenView::kScreenId, 0 /* exit_code */);

  ShowMultiDeviceSetupScreen();
}

void WizardController::OnMultiDeviceSetupScreenExit() {
  OnScreenExit(MultiDeviceSetupScreenView::kScreenId, 0 /* exit_code */);

  OnOobeFlowFinished();
}

void WizardController::OnResetScreenExit() {
  OnScreenExit(ResetView::kScreenId, 0 /* exit_code */);
  OnDeviceModificationCanceled();
}

void WizardController::OnChangedMetricsReportingState(bool enabled) {
  StatsReportingController::Get()->SetEnabled(
      ProfileManager::GetActiveUserProfile(), enabled);
  if (crash_reporter::IsCrashpadEnabled()) {
    crash_reporter::SetUploadConsent(enabled);
    return;
  }

  if (!enabled)
    return;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  base::PostTask(FROM_HERE, {base::ThreadPool(), base::MayBlock()},
                 base::BindOnce(&breakpad::InitCrashReporter, std::string()));
#endif
}

void WizardController::OnDeviceModificationCanceled() {
  if (previous_screen_) {
    SetCurrentScreen(previous_screen_);
  } else {
    if (current_screen_)
      current_screen_->Hide();

    ShowLoginScreen(LoginScreenContext());
  }
}

void WizardController::OnSupervisionTransitionScreenExit() {
  OnScreenExit(SupervisionTransitionScreenView::kScreenId, 0 /* exit_code */);

  OnOobeFlowFinished();
}

void WizardController::OnOobeFlowFinished() {
  if (!time_oobe_started_.is_null()) {
    base::TimeDelta delta = base::Time::Now() - time_oobe_started_;
    UMA_HISTOGRAM_CUSTOM_TIMES("OOBE.BootToSignInCompleted", delta,
                               base::TimeDelta::FromMilliseconds(10),
                               base::TimeDelta::FromMinutes(30), 100);
    time_oobe_started_ = base::Time();
  }

  // Launch browser and delete login host controller.
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&UserSessionManager::DoBrowserLaunch,
                     base::Unretained(UserSessionManager::GetInstance()),
                     ProfileManager::GetActiveUserProfile(),
                     GetLoginDisplayHost()));
}

void WizardController::OnDeviceDisabledChecked(bool device_disabled) {
  prescribed_enrollment_config_ = g_browser_process->platform_part()
                                      ->browser_policy_connector_chromeos()
                                      ->GetPrescribedEnrollmentConfig();

  bool configuration_forced_enrollment = false;
  auto* start_enrollment_value = oobe_configuration_.FindKeyOfType(
      configuration::kWizardAutoEnroll, base::Value::Type::BOOLEAN);
  if (start_enrollment_value)
    configuration_forced_enrollment = start_enrollment_value->GetBool();

  if (device_disabled) {
    demo_setup_controller_.reset();
    ShowDeviceDisabledScreen();
  } else if (demo_setup_controller_) {
    ShowDemoModeSetupScreen();
  } else if (skip_update_enroll_after_eula_ ||
             prescribed_enrollment_config_.should_enroll() ||
             configuration_forced_enrollment) {
    VLOG(1) << "StartEnrollment from OnDeviceDisabledChecked(device_disabled="
            << device_disabled << ") "
            << "skip_update_enroll_after_eula_="
            << skip_update_enroll_after_eula_
            << ", prescribed_enrollment_config_.should_enroll()="
            << prescribed_enrollment_config_.should_enroll()
            << ", configuration_forced_enrollment="
            << configuration_forced_enrollment;
    StartEnrollmentScreen(skip_update_enroll_after_eula_);
  } else {
    PerformOOBECompletedActions();
    ShowLoginScreen(LoginScreenContext());
  }
}

void WizardController::InitiateOOBEUpdate() {
  if (IsRemoraRequisition()) {
    VLOG(1) << "Skip OOBE Update for remora.";
    OnUpdateCompleted();
    return;
  }

  const auto* skip_screen_key = oobe_configuration_.FindKeyOfType(
      configuration::kUpdateSkipUpdate, base::Value::Type::BOOLEAN);
  const bool skip_screen = skip_screen_key && skip_screen_key->GetBool();

  if (skip_screen) {
    VLOG(1) << "Skip OOBE Update because of configuration.";
    OnUpdateCompleted();
    return;
  }

  // If this is a Cellular First device, instruct UpdateEngine to allow
  // updates over cellular data connections.
  if (chromeos::switches::IsCellularFirstDevice()) {
    DBusThreadManager::Get()
        ->GetUpdateEngineClient()
        ->SetUpdateOverCellularPermission(
            true, base::Bind(&WizardController::StartOOBEUpdate,
                             weak_factory_.GetWeakPtr()));
  } else {
    StartOOBEUpdate();
  }
}

void WizardController::StartOOBEUpdate() {
  SetCurrentScreen(GetScreen(UpdateView::kScreenId));
}

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
      base::Bind(&WizardController::OnLocationResolved,
                 weak_factory_.GetWeakPtr()));
}

void WizardController::PerformPostEulaActions() {
  DelayNetworkCall(
      base::TimeDelta::FromMilliseconds(kDefaultNetworkRetryDelayMS),
      base::Bind(&WizardController::StartTimezoneResolve,
                 weak_factory_.GetWeakPtr()));
  DelayNetworkCall(
      base::TimeDelta::FromMilliseconds(kDefaultNetworkRetryDelayMS),
      ServicesCustomizationDocument::GetInstance()
          ->EnsureCustomizationAppliedClosure());

  // Now that EULA has been accepted (for official builds), enable portal check.
  // ChromiumOS builds would go though this code path too.
  NetworkHandler::Get()->network_state_handler()->SetCheckPortalList(
      NetworkStateHandler::kDefaultCheckPortalList);
  GetAutoEnrollmentController()->Start();
  GetLoginDisplayHost()->PrewarmAuthentication();
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
      GetLocalState()->GetInteger(prefs::kTimesHIDDialogShown));
  GetLocalState()->ClearPref(prefs::kTimesHIDDialogShown);
  StartupUtils::MarkOobeCompleted();
  oobe_marked_completed_ = true;
}

void WizardController::SetCurrentScreen(BaseScreen* new_current) {
  VLOG(1) << "SetCurrentScreen: " << new_current->screen_id();
  if (current_screen_ == new_current || new_current == nullptr ||
      GetOobeUI() == nullptr) {
    return;
  }

  if (current_screen_) {
    current_screen_->Hide();
    current_screen_->SetConfiguration(nullptr);
  }

  // Record show time for UMA.
  screen_show_times_[new_current->screen_id()] = base::Time::Now();

  previous_screen_ = current_screen_;
  current_screen_ = new_current;

  // First remember how far have we reached so that we can resume if needed.
  if (is_out_of_box_ && !demo_setup_controller_ &&
      IsResumableScreen(current_screen_->screen_id())) {
    StartupUtils::SaveOobePendingScreen(current_screen_->screen_id().name);
  }

  UpdateStatusAreaVisibilityForScreen(current_screen_->screen_id());
  current_screen_->SetConfiguration(&oobe_configuration_);
  current_screen_->Show();
}

void WizardController::UpdateStatusAreaVisibilityForScreen(
    OobeScreenId screen) {
  if (screen == WelcomeView::kScreenId) {
    // Hide the status area initially; it only appears after OOBE first animates
    // in. Keep it visible if the user goes back to the existing welcome screen.
    GetLoginDisplayHost()->SetStatusAreaVisible(
        screen_manager_->HasScreen(WelcomeView::kScreenId));
  } else {
    GetLoginDisplayHost()->SetStatusAreaVisible(!ShouldHideStatusArea(screen));
  }
}

void WizardController::OnHIDScreenNecessityCheck(bool screen_needed) {
  if (!GetOobeUI())
    return;

  // Check for tests configuration.
  if (StartupUtils::IsEulaAccepted() || StartupUtils::IsOobeCompleted())
    return;

  const auto* skip_screen_key = oobe_configuration_.FindKeyOfType(
      configuration::kSkipHIDDetection, base::Value::Type::BOOLEAN);
  const bool skip_screen = skip_screen_key && skip_screen_key->GetBool();

  if (screen_needed && !skip_screen)
    ShowHIDDetectionScreen();
  else
    ShowWelcomeScreen();
}

void WizardController::UpdateOobeConfiguration() {
  oobe_configuration_ = base::Value(base::Value::Type::DICTIONARY);
  chromeos::configuration::FilterConfiguration(
      OobeConfiguration::Get()->GetConfiguration(),
      chromeos::configuration::ConfigurationHandlerSide::HANDLER_CPP,
      oobe_configuration_);
  auto* requisition_value = oobe_configuration_.FindKeyOfType(
      configuration::kDeviceRequisition, base::Value::Type::STRING);
  if (requisition_value) {
    auto* policy_manager = g_browser_process->platform_part()
                               ->browser_policy_connector_chromeos()
                               ->GetDeviceCloudPolicyManager();
    if (policy_manager) {
      VLOG(1) << "Using Device Requisition from configuration"
              << requisition_value->GetString();
      policy_manager->SetDeviceRequisition(requisition_value->GetString());
    }
  }
}

void WizardController::AdvanceToScreen(OobeScreenId screen) {
  if (screen == WelcomeView::kScreenId) {
    ShowWelcomeScreen();
  } else if (screen == NetworkScreenView::kScreenId) {
    ShowNetworkScreen();
  } else if (screen == OobeScreen::SCREEN_SPECIAL_LOGIN) {
    ShowLoginScreen(LoginScreenContext());
  } else if (screen == UpdateView::kScreenId) {
    InitiateOOBEUpdate();
  } else if (screen == EulaView::kScreenId) {
    ShowEulaScreen();
  } else if (screen == ResetView::kScreenId) {
    ShowResetScreen();
  } else if (screen == KioskEnableScreenView::kScreenId) {
    ShowKioskEnableScreen();
  } else if (screen == KioskAutolaunchScreenView::kScreenId) {
    ShowKioskAutolaunchScreen();
  } else if (screen == EnableAdbSideloadingScreenView::kScreenId) {
    ShowEnableAdbSideloadingScreen();
  } else if (screen == EnableDebuggingScreenView::kScreenId) {
    ShowEnableDebuggingScreen();
  } else if (screen == EnrollmentScreenView::kScreenId) {
    ShowEnrollmentScreen();
  } else if (screen == DemoSetupScreenView::kScreenId) {
    ShowDemoModeSetupScreen();
  } else if (screen == DemoPreferencesScreenView::kScreenId) {
    ShowDemoModePreferencesScreen();
  } else if (screen == TermsOfServiceScreenView::kScreenId) {
    ShowTermsOfServiceScreen();
  } else if (screen == SyncConsentScreenView::kScreenId) {
    ShowSyncConsentScreen();
  } else if (screen == ArcTermsOfServiceScreenView::kScreenId) {
    ShowArcTermsOfServiceScreen();
  } else if (screen == RecommendAppsScreenView::kScreenId) {
    ShowRecommendAppsScreen();
  } else if (screen == AppDownloadingScreenView::kScreenId) {
    ShowAppDownloadingScreen();
  } else if (screen == WrongHWIDScreenView::kScreenId) {
    ShowWrongHWIDScreen();
  } else if (screen == AutoEnrollmentCheckScreenView::kScreenId) {
    ShowAutoEnrollmentCheckScreen();
  } else if (screen == AppLaunchSplashScreenView::kScreenId) {
    AutoLaunchKioskApp();
  } else if (screen == ArcKioskSplashScreenView::kScreenId) {
    ShowArcKioskSplashScreen();
  } else if (screen == HIDDetectionView::kScreenId) {
    ShowHIDDetectionScreen();
  } else if (screen == DeviceDisabledScreenView::kScreenId) {
    ShowDeviceDisabledScreen();
  } else if (screen == EncryptionMigrationScreenView::kScreenId) {
    ShowEncryptionMigrationScreen();
  } else if (screen == UpdateRequiredView::kScreenId) {
    ShowUpdateRequiredScreen();
  } else if (screen == AssistantOptInFlowScreenView::kScreenId) {
    ShowAssistantOptInFlowScreen();
  } else if (screen == MultiDeviceSetupScreenView::kScreenId) {
    ShowMultiDeviceSetupScreen();
  } else if (screen == DiscoverScreenView::kScreenId) {
    ShowDiscoverScreen();
  } else if (screen == FingerprintSetupScreenView::kScreenId) {
    ShowFingerprintSetupScreen();
  } else if (screen == MarketingOptInScreenView::kScreenId) {
    ShowMarketingOptInScreen();
  } else if (screen == SupervisionTransitionScreenView::kScreenId) {
    ShowSupervisionTransitionScreen();
  } else if (screen != OobeScreen::SCREEN_TEST_NO_WINDOW) {
    if (is_out_of_box_) {
      time_oobe_started_ = base::Time::Now();
      if (CanShowHIDDetectionScreen()) {
        hid_screen_ = GetScreen(HIDDetectionView::kScreenId);
        base::Callback<void(bool)> on_check =
            base::Bind(&WizardController::OnHIDScreenNecessityCheck,
                       weak_factory_.GetWeakPtr());
        GetOobeUI()
            ->GetView<HIDDetectionScreenHandler>()
            ->CheckIsScreenRequired(on_check);
      } else {
        ShowWelcomeScreen();
      }
    } else {
      ShowLoginScreen(LoginScreenContext());
    }
  }
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

void WizardController::ShowErrorScreen() {
  SetCurrentScreen(GetScreen(ErrorScreenView::kScreenId));
}

void WizardController::OnAccessibilityStatusChanged(
    const AccessibilityStatusEventDetails& details) {
  enum AccessibilityNotificationType type = details.notification_type;
  if (type == ACCESSIBILITY_MANAGER_SHUTDOWN) {
    accessibility_subscription_.reset();
    return;
  } else if (type != ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK || !details.enabled) {
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

void WizardController::OnGuestModePolicyUpdated() {
  ash::LoginScreen::Get()->SetAllowLoginAsGuest(
      user_manager::UserManager::Get()->IsGuestSessionAllowed());
}

void WizardController::AutoLaunchKioskApp() {
  KioskAppManager::App app_data;
  std::string app_id = KioskAppManager::Get()->GetAutoLaunchApp();
  CHECK(KioskAppManager::Get()->GetApp(app_id, &app_data));

  // Wait for the |CrosSettings| to become either trusted or permanently
  // untrusted.
  const CrosSettingsProvider::TrustedStatus status =
      CrosSettings::Get()->PrepareTrustedValues(base::Bind(
          &WizardController::AutoLaunchKioskApp, weak_factory_.GetWeakPtr()));
  if (status == CrosSettingsProvider::TEMPORARILY_UNTRUSTED)
    return;

  if (status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
    // If the |cros_settings_| are permanently untrusted, show an error message
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

  const bool diagnostic_mode = false;
  const bool auto_launch = true;
  GetLoginDisplayHost()->StartAppLaunch(app_id, diagnostic_mode, auto_launch);
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
      current_screen_id == SyncConsentScreenView::kScreenId ||
      current_screen_id == FingerprintSetupScreenView::kScreenId ||
      current_screen_id == ArcTermsOfServiceScreenView::kScreenId ||
      current_screen_id == DiscoverScreenView::kScreenId ||
      current_screen_id == MarketingOptInScreenView::kScreenId) {
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
WizardController::ForceBrandedBuildForTesting() {
  return std::make_unique<base::AutoReset<bool>>(&is_branded_build_, true);
}

// static
bool WizardController::UsingHandsOffEnrollment() {
  return policy::DeviceCloudPolicyManagerChromeOS::
             GetZeroTouchEnrollmentMode() ==
         policy::ZeroTouchEnrollmentMode::HANDS_OFF;
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
  base::ScopedClosureRunner inform_test(on_timezone_resolved_for_testing_);
  on_timezone_resolved_for_testing_.Reset();

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
      base::Bind(&WizardController::OnTimezoneResolved,
                 weak_factory_.GetWeakPtr()));
}

bool WizardController::SetOnTimeZoneResolvedForTesting(
    const base::Closure& callback) {
  if (timezone_resolved_)
    return false;

  on_timezone_resolved_for_testing_ = callback;
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
  auto* restore_after_rollback_value = oobe_configuration_.FindKeyOfType(
      configuration::kRestoreAfterRollback, base::Value::Type::BOOLEAN);
  if (restore_after_rollback_value && restore_after_rollback_value->GetBool())
    effective_config.mode = policy::EnrollmentConfig::MODE_ENROLLED_ROLLBACK;

  // If enrollment token is specified via OOBE configuration use corresponding
  // configuration.
  auto* enrollment_token = oobe_configuration_.FindKeyOfType(
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

AutoEnrollmentController* WizardController::GetAutoEnrollmentController() {
  if (!auto_enrollment_controller_)
    auto_enrollment_controller_ = std::make_unique<AutoEnrollmentController>();
  return auto_enrollment_controller_.get();
}

}  // namespace chromeos
