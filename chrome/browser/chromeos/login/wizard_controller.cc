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
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/voice_interaction/arc_voice_interaction_framework_service.h"
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
#include "chrome/browser/chromeos/login/screens/enable_debugging_screen.h"
#include "chrome/browser/chromeos/login/screens/encryption_migration_screen.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/screens/eula_screen.h"
#include "chrome/browser/chromeos/login/screens/fingerprint_setup_screen.h"
#include "chrome/browser/chromeos/login/screens/hid_detection_view.h"
#include "chrome/browser/chromeos/login/screens/kiosk_autolaunch_screen.h"
#include "chrome/browser/chromeos/login/screens/kiosk_enable_screen.h"
#include "chrome/browser/chromeos/login/screens/marketing_opt_in_screen.h"
#include "chrome/browser/chromeos/login/screens/multidevice_setup_screen.h"
#include "chrome/browser/chromeos/login/screens/network_error.h"
#include "chrome/browser/chromeos/login/screens/network_screen.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps_screen.h"
#include "chrome/browser/chromeos/login/screens/reset_screen.h"
#include "chrome/browser/chromeos/login/screens/sync_consent_screen.h"
#include "chrome/browser/chromeos/login/screens/terms_of_service_screen.h"
#include "chrome/browser/chromeos/login/screens/update_required_screen.h"
#include "chrome/browser/chromeos/login/screens/update_screen.h"
#include "chrome/browser/chromeos/login/screens/user_image_screen.h"
#include "chrome/browser/chromeos/login/screens/voice_interaction_value_prop_screen.h"
#include "chrome/browser/chromeos/login/screens/wait_for_container_ready_screen.h"
#include "chrome/browser/chromeos/login/screens/welcome_view.h"
#include "chrome/browser/chromeos/login/screens/wrong_hwid_screen.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/chromeos/net/delay_network_call.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system/device_disabling_manager.h"
#include "chrome/browser/chromeos/system/timezone_resolver_manager.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/help/help_utils_chromeos.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/chromeos_constants.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
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
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_util.h"
#include "components/crash/content/app/breakpad_linux.h"
#include "components/pairing/bluetooth_controller_pairing_controller.h"
#include "components/pairing/bluetooth_host_pairing_controller.h"
#include "components/pairing/shark_connection_listener.h"
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
// Interval in ms which is used for smooth screen showing.
static int g_show_delay_ms = 400;

// Total timezone resolving process timeout.
const unsigned int kResolveTimeZoneTimeoutSeconds = 60;

// Stores the list of all screens that should be shown when resuming OOBE.
const chromeos::OobeScreen kResumableScreens[] = {
    chromeos::OobeScreen::SCREEN_OOBE_WELCOME,
    chromeos::OobeScreen::SCREEN_OOBE_NETWORK,
    chromeos::OobeScreen::SCREEN_OOBE_UPDATE,
    chromeos::OobeScreen::SCREEN_OOBE_EULA,
    chromeos::OobeScreen::SCREEN_OOBE_ENROLLMENT,
    chromeos::OobeScreen::SCREEN_TERMS_OF_SERVICE,
    chromeos::OobeScreen::SCREEN_SYNC_CONSENT,
    chromeos::OobeScreen::SCREEN_FINGERPRINT_SETUP,
    chromeos::OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE,
    chromeos::OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK,
    chromeos::OobeScreen::SCREEN_RECOMMEND_APPS,
    chromeos::OobeScreen::SCREEN_APP_DOWNLOADING,
    chromeos::OobeScreen::SCREEN_DISCOVER,
    chromeos::OobeScreen::SCREEN_MARKETING_OPT_IN,
    chromeos::OobeScreen::SCREEN_MULTIDEVICE_SETUP,
};

// Checks if device is in tablet mode, and that HID-detection screen is not
// disabled by flag.
bool CanShowHIDDetectionScreen() {
  return !TabletModeClient::Get()->tablet_mode_enabled() &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             chromeos::switches::kDisableHIDDetectionOnOOBE) &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             ash::switches::kAshEnableTabletMode);
}

bool IsResumableScreen(chromeos::OobeScreen screen) {
  for (const auto& resumable_screen : kResumableScreens) {
    if (screen == resumable_screen)
      return true;
  }
  return false;
}

struct Entry {
  chromeos::OobeScreen screen;
  const char* uma_name;
};

// Some screens had multiple different names in the past (they have since been
// unified). We need to always use the same name for UMA stats, though.
constexpr const Entry kLegacyUmaOobeScreenNames[] = {
    {chromeos::OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE, "arc_tos"},
    {chromeos::OobeScreen::SCREEN_OOBE_ENROLLMENT, "enroll"},
    {chromeos::OobeScreen::SCREEN_OOBE_WELCOME, "network"},
    {chromeos::OobeScreen::SCREEN_CREATE_SUPERVISED_USER_FLOW_DEPRECATED,
     "supervised-user-creation-flow"},
    {chromeos::OobeScreen::SCREEN_TERMS_OF_SERVICE, "tos"},
    {chromeos::OobeScreen::SCREEN_USER_IMAGE_PICKER, "image"}};

void RecordUMAHistogramForOOBEStepCompletionTime(chromeos::OobeScreen screen,
                                                 base::TimeDelta step_time) {
  // Fetch screen name; make sure to use initial UMA name if the name has
  // changed.
  std::string screen_name = chromeos::GetOobeScreenName(screen);
  for (const auto& entry : kLegacyUmaOobeScreenNames) {
    if (entry.screen == screen) {
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

bool IsSharkRequisition() {
  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetDeviceCloudPolicyManager();
  return policy_manager && policy_manager->IsSharkRequisition();
}

// Checks if a controller device ("Master") is detected during the bootstrapping
// or shark/remora setup process.
bool IsControllerDetected() {
  return g_browser_process->local_state()->GetBoolean(
      prefs::kOobeControllerDetected);
}

void SetControllerDetectedPref(bool value) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kOobeControllerDetected, value);
  prefs->CommitPendingWrite();
}

// Checks if the device is a "slave" device in the bootstrapping process.
bool IsBootstrappingSlave() {
  return g_browser_process->local_state()->GetBoolean(
      prefs::kIsBootstrappingSlave);
}

// Checks if the device is a "Master" device in the bootstrapping process.
bool IsBootstrappingMaster() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kOobeBootstrappingMaster);
}

bool NetworkAllowUpdate(const chromeos::NetworkState* network) {
  if (!network || !network->IsConnectedState())
    return false;
  if (network->type() == shill::kTypeBluetooth ||
      (network->type() == shill::kTypeCellular &&
       !help_utils_chromeos::IsUpdateOverCellularAllowed(
           false /* interactive */))) {
    return false;
  }
  return true;
}

// Return false if the logged in user is a managed or child account. Otherwise,
// return true if the feature flag for recommend app screen is on.
bool ShouldShowRecommendAppsScreen() {
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  DCHECK(user_manager->IsUserLoggedIn());
  bool is_managed_account =
      policy::ProfilePolicyConnectorFactory::IsProfileManaged(
          ProfileManager::GetActiveUserProfile());
  bool is_child_account = user_manager->IsLoggedInAsChildUser();
  return !is_managed_account && !is_child_account &&
         base::FeatureList::IsEnabled(features::kOobeRecommendAppsScreen);
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
  static scoped_refptr<network::SharedURLLoaderFactory> loader;
  return loader;
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
WizardController* WizardController::default_controller() {
  auto* host = chromeos::LoginDisplayHost::default_host();
  return host ? host->GetWizardController() : nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, public:

PrefService* WizardController::local_state_for_testing_ = nullptr;

WizardController::WizardController()
    : screen_manager_(std::make_unique<ScreenManager>()),
      network_state_helper_(std::make_unique<login::NetworkStateHelper>()),
      oobe_configuration_(base::Value(base::Value::Type::DICTIONARY)),
      weak_factory_(this) {
  // In session OOBE was initiated from voice interaction keyboard shortcuts.
  is_in_session_oobe_ =
      session_manager::SessionManager::Get()->IsSessionStarted();
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
  // |remora_controller| has to be reset after |screen_manager_| is reset.
  remora_controller_.reset();
  if (shark_connection_listener_.get()) {
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
        FROM_HERE, shark_connection_listener_.release());
  }
}

void WizardController::Init(OobeScreen first_screen) {
  VLOG(1) << "Starting OOBE wizard with screen: "
          << GetOobeScreenName(first_screen);
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

  // If the device is a Master device in bootstrapping process (mostly for demo
  // and test purpose), start the enrollment OOBE flow.
  if (IsBootstrappingMaster())
    connector->GetDeviceCloudPolicyManager()->SetDeviceEnrollmentAutoStart();

  // Use the saved screen preference from Local State.
  const std::string screen_pref =
      GetLocalState()->GetString(prefs::kOobeScreenPending);
  if (is_out_of_box_ && !screen_pref.empty() && !IsRemoraPairingOobe() &&
      !IsControllerDetected() &&
      (first_screen == OobeScreen::SCREEN_UNKNOWN ||
       first_screen == OobeScreen::SCREEN_TEST_NO_WINDOW)) {
    first_screen_ = GetOobeScreenFromName(screen_pref);
  }
  // We need to reset the kOobeControllerDetected pref to allow the user to have
  // the choice to setup the device manually. The pref will be set properly if
  // an eligible controller is detected later.
  SetControllerDetectedPref(false);

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

BaseScreen* WizardController::GetScreen(OobeScreen screen) {
  if (screen == OobeScreen::SCREEN_ERROR_MESSAGE)
    return GetErrorScreen();
  return screen_manager_->GetScreen(screen);
}

std::unique_ptr<BaseScreen> WizardController::CreateScreen(OobeScreen screen) {
  OobeUI* oobe_ui = GetOobeUI();

  if (screen == OobeScreen::SCREEN_OOBE_WELCOME) {
    return std::make_unique<WelcomeScreen>(this, this,
                                           oobe_ui->GetWelcomeView());
  } else if (screen == OobeScreen::SCREEN_OOBE_NETWORK) {
    return std::make_unique<NetworkScreen>(this,
                                           oobe_ui->GetNetworkScreenView());
  } else if (screen == OobeScreen::SCREEN_OOBE_UPDATE) {
    return std::make_unique<UpdateScreen>(this, oobe_ui->GetUpdateView(),
                                          remora_controller_.get());
  } else if (screen == OobeScreen::SCREEN_USER_IMAGE_PICKER) {
    return std::make_unique<UserImageScreen>(this, oobe_ui->GetUserImageView());
  } else if (screen == OobeScreen::SCREEN_OOBE_EULA) {
    return std::make_unique<EulaScreen>(this, this, oobe_ui->GetEulaView());
  } else if (screen == OobeScreen::SCREEN_OOBE_ENROLLMENT) {
    return std::make_unique<EnrollmentScreen>(
        this, oobe_ui->GetEnrollmentScreenView());
  } else if (screen == OobeScreen::SCREEN_OOBE_RESET) {
    return std::make_unique<chromeos::ResetScreen>(this,
                                                   oobe_ui->GetResetView());
  } else if (screen == OobeScreen::SCREEN_OOBE_DEMO_SETUP) {
    return std::make_unique<chromeos::DemoSetupScreen>(
        this, oobe_ui->GetDemoSetupScreenView());
  } else if (screen == OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES) {
    return std::make_unique<chromeos::DemoPreferencesScreen>(
        this, oobe_ui->GetDemoPreferencesScreenView());
  } else if (screen == OobeScreen::SCREEN_OOBE_ENABLE_DEBUGGING) {
    return std::make_unique<EnableDebuggingScreen>(
        this, oobe_ui->GetEnableDebuggingScreenView());
  } else if (screen == OobeScreen::SCREEN_KIOSK_ENABLE) {
    return std::make_unique<KioskEnableScreen>(
        this, oobe_ui->GetKioskEnableScreenView());
  } else if (screen == OobeScreen::SCREEN_KIOSK_AUTOLAUNCH) {
    return std::make_unique<KioskAutolaunchScreen>(
        this, oobe_ui->GetKioskAutolaunchScreenView());
  } else if (screen == OobeScreen::SCREEN_TERMS_OF_SERVICE) {
    return std::make_unique<TermsOfServiceScreen>(
        this, oobe_ui->GetTermsOfServiceScreenView());
  } else if (screen == OobeScreen::SCREEN_SYNC_CONSENT) {
    return std::make_unique<SyncConsentScreen>(
        this, oobe_ui->GetSyncConsentScreenView());
  } else if (screen == OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE) {
    return std::make_unique<ArcTermsOfServiceScreen>(
        this, oobe_ui->GetArcTermsOfServiceScreenView());
  } else if (screen == OobeScreen::SCREEN_RECOMMEND_APPS) {
    return std::make_unique<RecommendAppsScreen>(
        this, oobe_ui->GetRecommendAppsScreenView());
  } else if (screen == OobeScreen::SCREEN_APP_DOWNLOADING) {
    return std::make_unique<AppDownloadingScreen>(
        this, oobe_ui->GetAppDownloadingScreenView());
  } else if (screen == OobeScreen::SCREEN_WRONG_HWID) {
    return std::make_unique<WrongHWIDScreen>(this,
                                             oobe_ui->GetWrongHWIDScreenView());
  } else if (screen == OobeScreen::SCREEN_OOBE_HID_DETECTION) {
    return std::make_unique<chromeos::HIDDetectionScreen>(
        this, oobe_ui->GetHIDDetectionView());
  } else if (screen == OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK) {
    return std::make_unique<AutoEnrollmentCheckScreen>(
        this, oobe_ui->GetAutoEnrollmentCheckScreenView());
  } else if (screen == OobeScreen::SCREEN_OOBE_CONTROLLER_PAIRING) {
    if (!shark_controller_) {
      shark_controller_ = std::make_unique<
          pairing_chromeos::BluetoothControllerPairingController>();
    }
    return std::make_unique<ControllerPairingScreen>(
        this, this, oobe_ui->GetControllerPairingScreenView(),
        shark_controller_.get());
  } else if (screen == OobeScreen::SCREEN_OOBE_HOST_PAIRING) {
    if (!remora_controller_) {
      DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
      DCHECK(content::ServiceManagerConnection::GetForProcess());
      service_manager::Connector* connector =
          content::ServiceManagerConnection::GetForProcess()->GetConnector();
      remora_controller_ =
          std::make_unique<pairing_chromeos::BluetoothHostPairingController>(
              connector);
      remora_controller_->StartPairing();
    }
    return std::make_unique<HostPairingScreen>(
        this, this, oobe_ui->GetHostPairingScreenView(),
        remora_controller_.get());
  } else if (screen == OobeScreen::SCREEN_DEVICE_DISABLED) {
    return std::make_unique<DeviceDisabledScreen>(
        this, oobe_ui->GetDeviceDisabledScreenView());
  } else if (screen == OobeScreen::SCREEN_ENCRYPTION_MIGRATION) {
    return std::make_unique<EncryptionMigrationScreen>(
        this, oobe_ui->GetEncryptionMigrationScreenView());
  } else if (screen == OobeScreen::SCREEN_VOICE_INTERACTION_VALUE_PROP) {
    return std::make_unique<VoiceInteractionValuePropScreen>(
        this, oobe_ui->GetVoiceInteractionValuePropScreenView());
  } else if (screen == OobeScreen::SCREEN_WAIT_FOR_CONTAINER_READY) {
    return std::make_unique<WaitForContainerReadyScreen>(
        this, oobe_ui->GetWaitForContainerReadyScreenView());
  } else if (screen == OobeScreen::SCREEN_UPDATE_REQUIRED) {
    return std::make_unique<UpdateRequiredScreen>(
        this, oobe_ui->GetUpdateRequiredScreenView());
  } else if (screen == OobeScreen::SCREEN_ASSISTANT_OPTIN_FLOW) {
    return std::make_unique<AssistantOptInFlowScreen>(
        this, oobe_ui->GetAssistantOptInFlowScreenView());
  } else if (screen == OobeScreen::SCREEN_MULTIDEVICE_SETUP) {
    return std::make_unique<MultiDeviceSetupScreen>(
        this, oobe_ui->GetMultiDeviceSetupScreenView());
  } else if (screen == OobeScreen::SCREEN_DISCOVER) {
    return std::make_unique<DiscoverScreen>(this,
                                            oobe_ui->GetDiscoverScreenView());
  } else if (screen == OobeScreen::SCREEN_FINGERPRINT_SETUP) {
    return std::make_unique<FingerprintSetupScreen>(
        this, oobe_ui->GetFingerprintSetupScreenView());
  } else if (screen == OobeScreen::SCREEN_MARKETING_OPT_IN) {
    return std::make_unique<MarketingOptInScreen>(
        this, oobe_ui->GetMarketingOptInScreenView());
  }
  return nullptr;
}

void WizardController::SetCurrentScreenForTesting(BaseScreen* screen) {
  current_screen_ = screen;
}

void WizardController::SetSharedURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> factory) {
  auto& testing_factory = GetSharedURLLoaderFactoryForTesting();
  testing_factory = std::move(factory);
}

void WizardController::ShowWelcomeScreen() {
  VLOG(1) << "Showing welcome screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_OOBE_WELCOME));

  // There are two possible screens where we listen to the incoming Bluetooth
  // connection request: the first one is the HID detection screen, which will
  // show up when there is no sufficient input devices. In this case, we just
  // keep the logic as it is today: always put the Bluetooth is discoverable
  // mode. The other place is the Network screen (here), which will show up when
  // there are input devices detected. In this case, we disable the Bluetooth by
  // default until the user explicitly enable it by pressing a key combo (Ctrl+
  // Alt+Shift+S).
  if (IsBootstrappingSlave())
    MaybeStartListeningForSharkConnection();
}

void WizardController::ShowNetworkScreen() {
  VLOG(1) << "Showing network screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_OOBE_NETWORK));
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
  smooth_show_timer_.Stop();
  login_screen_started_ = true;
}

void WizardController::ShowPreviousScreen() {
  DCHECK(previous_screen_);
  SetCurrentScreen(previous_screen_);
}

void WizardController::ShowUserImageScreen() {
  VLOG(1) << "Showing user image screen.";
  // Status area has been already shown at sign in screen so it
  // doesn't make sense to hide it here and then show again at user session as
  // this produces undesired UX transitions.
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_USER_IMAGE_PICKER);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_USER_IMAGE_PICKER));
}

void WizardController::ShowEulaScreen() {
  VLOG(1) << "Showing EULA screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_OOBE_EULA);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_OOBE_EULA));
}

void WizardController::ShowEnrollmentScreen() {
  // Update the enrollment configuration and start the screen.
  prescribed_enrollment_config_ = g_browser_process->platform_part()
                                      ->browser_policy_connector_chromeos()
                                      ->GetPrescribedEnrollmentConfig();
  StartEnrollmentScreen(false);
}

void WizardController::ShowDemoModePreferencesScreen() {
  VLOG(1) << "Showing demo mode preferences screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES));
}

void WizardController::ShowDemoModeSetupScreen() {
  VLOG(1) << "Showing demo mode setup screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_OOBE_DEMO_SETUP);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_OOBE_DEMO_SETUP));
}

void WizardController::ShowResetScreen() {
  VLOG(1) << "Showing reset screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_OOBE_RESET);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_OOBE_RESET));
}

void WizardController::ShowKioskEnableScreen() {
  VLOG(1) << "Showing kiosk enable screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_KIOSK_ENABLE);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_KIOSK_ENABLE));
}

void WizardController::ShowKioskAutolaunchScreen() {
  VLOG(1) << "Showing kiosk autolaunch screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_KIOSK_AUTOLAUNCH);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_KIOSK_AUTOLAUNCH));
}

void WizardController::ShowEnableDebuggingScreen() {
  VLOG(1) << "Showing enable developer features screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_OOBE_ENABLE_DEBUGGING);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_OOBE_ENABLE_DEBUGGING));
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

  VLOG(1) << "Showing Terms of Service screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_TERMS_OF_SERVICE);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_TERMS_OF_SERVICE));
}

void WizardController::ShowSyncConsentScreen() {
#if defined(GOOGLE_CHROME_BUILD)
  VLOG(1) << "Showing Sync Consent screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_SYNC_CONSENT);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_SYNC_CONSENT));
#else
  OnSyncConsentFinished();
#endif
}

void WizardController::ShowFingerprintSetupScreen() {
  VLOG(1) << "Showing Fingerprint Setup screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_FINGERPRINT_SETUP);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_FINGERPRINT_SETUP));
}

void WizardController::ShowMarketingOptInScreen() {
  VLOG(1) << "Showing Marketing Opt-In screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_MARKETING_OPT_IN);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_MARKETING_OPT_IN));
}

void WizardController::ShowArcTermsOfServiceScreen() {
  if (arc::IsArcTermsOfServiceOobeNegotiationNeeded()) {
    VLOG(1) << "Showing ARC Terms of Service screen.";
    UpdateStatusAreaVisibilityForScreen(
        OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE);
    SetCurrentScreen(GetScreen(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE));
    // Assistant Wizard also uses wizard for ARC opt-in, unlike other scenarios
    // which use ArcSupport for now, because we're interested in only OOBE flow.
    // Note that this part also needs to be updated on b/65861628.
    // TODO(khmel): add unit test once we have support for OobeUI.
    if (!GetLoginDisplayHost()->IsVoiceInteractionOobe()) {
      ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
          arc::prefs::kArcTermsShownInOobe, true);
    }
  } else {
    ShowAssistantOptInFlowScreen();
  }
}

void WizardController::ShowRecommendAppsScreen() {
  VLOG(1) << "Showing Recommend Apps screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_RECOMMEND_APPS);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_RECOMMEND_APPS));
}

void WizardController::ShowAppDownloadingScreen() {
  VLOG(1) << "Showing App Downloading screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_APP_DOWNLOADING);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_APP_DOWNLOADING));
}

void WizardController::ShowWrongHWIDScreen() {
  VLOG(1) << "Showing wrong HWID screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_WRONG_HWID);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_WRONG_HWID));
}

void WizardController::ShowAutoEnrollmentCheckScreen() {
  VLOG(1) << "Showing Auto-enrollment check screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  AutoEnrollmentCheckScreen* screen =
      AutoEnrollmentCheckScreen::Get(screen_manager());
  if (retry_auto_enrollment_check_)
    screen->ClearState();
  screen->set_auto_enrollment_controller(GetAutoEnrollmentController());
  SetCurrentScreen(screen);
}

void WizardController::ShowArcKioskSplashScreen() {
  VLOG(1) << "Showing ARC kiosk splash screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_ARC_KIOSK_SPLASH);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_ARC_KIOSK_SPLASH));
}

void WizardController::ShowHIDDetectionScreen() {
  VLOG(1) << "Showing HID discovery screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_OOBE_HID_DETECTION);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_OOBE_HID_DETECTION));
  // In HID detection screen, puts the Bluetooth in discoverable mode and waits
  // for the incoming Bluetooth connection request. See the comments in
  // WizardController::ShowWelcomeScreen() for more details.
  MaybeStartListeningForSharkConnection();
}

void WizardController::ShowControllerPairingScreen() {
  VLOG(1) << "Showing controller pairing screen.";
  UpdateStatusAreaVisibilityForScreen(
      OobeScreen::SCREEN_OOBE_CONTROLLER_PAIRING);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_OOBE_CONTROLLER_PAIRING));
}

void WizardController::ShowHostPairingScreen() {
  VLOG(1) << "Showing host pairing screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_OOBE_HOST_PAIRING);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_OOBE_HOST_PAIRING));
}

void WizardController::ShowDeviceDisabledScreen() {
  VLOG(1) << "Showing device disabled screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_DEVICE_DISABLED);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_DEVICE_DISABLED));
}

void WizardController::ShowEncryptionMigrationScreen() {
  VLOG(1) << "Showing encryption migration screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_ENCRYPTION_MIGRATION);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_ENCRYPTION_MIGRATION));
}

void WizardController::ShowVoiceInteractionValuePropScreen() {
  if (ShouldShowVoiceInteractionValueProp()) {
    VLOG(1) << "Showing voice interaction value prop screen.";
    UpdateStatusAreaVisibilityForScreen(
        OobeScreen::SCREEN_VOICE_INTERACTION_VALUE_PROP);
    SetCurrentScreen(
        GetScreen(OobeScreen::SCREEN_VOICE_INTERACTION_VALUE_PROP));
  } else {
    OnOobeFlowFinished();
  }
}

void WizardController::ShowWaitForContainerReadyScreen() {
  DCHECK(is_in_session_oobe_);
  // At this point we could make sure the value prop flow has been accepted.
  // Set the value prop pref as accepted in framework service.
  auto* service =
      arc::ArcVoiceInteractionFrameworkService::GetForBrowserContext(
          ProfileManager::GetActiveUserProfile());
  if (service)
    service->SetVoiceInteractionSetupCompleted();

  UpdateStatusAreaVisibilityForScreen(
      OobeScreen::SCREEN_WAIT_FOR_CONTAINER_READY);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_WAIT_FOR_CONTAINER_READY));
}

void WizardController::ShowUpdateRequiredScreen() {
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_UPDATE_REQUIRED));
}

void WizardController::ShowAssistantOptInFlowScreen() {
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_ASSISTANT_OPTIN_FLOW);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_ASSISTANT_OPTIN_FLOW));
}

void WizardController::ShowMultiDeviceSetupScreen() {
  VLOG(1) << "Showing MultiDevice setup screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_MULTIDEVICE_SETUP);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_MULTIDEVICE_SETUP));
}

void WizardController::ShowDiscoverScreen() {
  VLOG(1) << "Showing Discover screen.";
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_DISCOVER);
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_DISCOVER));
}

void WizardController::SkipToLoginForTesting(
    const LoginScreenContext& context) {
  VLOG(1) << "SkipToLoginForTesting.";
  StartupUtils::MarkEulaAccepted();
  PerformPostEulaActions();
  OnDeviceDisabledChecked(false /* device_disabled */);
}

void WizardController::SkipToUpdateForTesting() {
  VLOG(1) << "SkipToUpdateForTesting.";
  StartupUtils::MarkEulaAccepted();
  PerformPostEulaActions();
  InitiateOOBEUpdate();
}

pairing_chromeos::SharkConnectionListener*
WizardController::GetSharkConnectionListenerForTesting() {
  return shark_connection_listener_.get();
}

void WizardController::SkipUpdateEnrollAfterEula() {
  skip_update_enroll_after_eula_ = true;
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, ExitHandlers:
void WizardController::OnHIDDetectionCompleted() {
  // Check for tests configuration.
  if (!StartupUtils::IsOobeCompleted())
    ShowWelcomeScreen();
}

void WizardController::OnWelcomeContinued() {
  ShowNetworkScreen();
}

void WizardController::OnNetworkBack() {
  if (demo_setup_controller_) {
    ShowDemoModePreferencesScreen();
  } else {
    ShowWelcomeScreen();
  }
}

void WizardController::OnNetworkConnected() {
  if (demo_setup_controller_) {
    demo_setup_controller_->set_demo_config(
        DemoSession::DemoModeConfig::kOnline);
  }

  if (is_official_build_) {
    if (!StartupUtils::IsEulaAccepted()) {
      ShowEulaScreen();
    } else if (arc::IsArcTermsOfServiceOobeNegotiationNeeded()) {
      ShowArcTermsOfServiceScreen();
    } else {
      // Possible cases:
      // 1. EULA was accepted, forced shutdown/reboot during update.
      // 2. EULA was accepted, planned reboot after update.
      // Make sure that device is up to date.
      InitiateOOBEUpdate();
    }
  } else {
    InitiateOOBEUpdate();
  }
}

void WizardController::OnOfflineDemoModeSetup() {
  DCHECK(demo_setup_controller_);
  demo_setup_controller_->set_demo_config(
      DemoSession::DemoModeConfig::kOffline);

  if (is_official_build_) {
    if (!StartupUtils::IsEulaAccepted()) {
      ShowEulaScreen();
    } else if (arc::IsArcTermsOfServiceOobeNegotiationNeeded()) {
      ShowArcTermsOfServiceScreen();
    } else {
      ShowDemoModeSetupScreen();
    }
  } else {
    // TODO(agawronska): Maybe check if device is connected to the network and
    // attempt system update. It is possible to initiate offline demo setup on
    // the device that is connected, although it is probably not common.
    ShowDemoModeSetupScreen();
  }
}

void WizardController::OnConnectionFailed() {
  // TODO(dpolukhin): show error message after login screen is displayed.
  ShowLoginScreen(LoginScreenContext());
}

void WizardController::OnUpdateCompleted() {
  if (IsSharkRequisition() || IsBootstrappingMaster()) {
    ShowControllerPairingScreen();
  } else if (IsControllerDetected()) {
    ShowHostPairingScreen();
  } else {
    ShowAutoEnrollmentCheckScreen();
  }
}

void WizardController::OnUpdateOverCellularRejected() {
  ShowNetworkScreen();
}

void WizardController::OnEulaAccepted() {
  time_eula_accepted_ = base::Time::Now();
  StartupUtils::MarkEulaAccepted();
  ChangeMetricsReportingStateWithReply(
      usage_statistics_reporting_,
      base::Bind(&WizardController::OnChangedMetricsReportingState,
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

void WizardController::OnEulaBack() {
  ShowNetworkScreen();
}

void WizardController::OnChangedMetricsReportingState(bool enabled) {
  CrosSettings::Get()->SetBoolean(kStatsReportingPref, enabled);
  if (!enabled)
    return;
#if defined(GOOGLE_CHROME_BUILD)
  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock()},
      base::Bind(&breakpad::InitCrashReporter, std::string()));
#endif
}

void WizardController::OnUpdateErrorCheckingForUpdate() {
  // TODO(nkostylev): Update should be required during OOBE.
  // We do not want to block users from being able to proceed to the login
  // screen if there is any error checking for an update.
  // They could use "browse without sign-in" feature to set up the network to be
  // able to perform the update later.
  OnUpdateCompleted();
}

void WizardController::OnUpdateErrorUpdating(bool is_critical_update) {
  // If there was an error while getting or applying the update, return to
  // network selection screen if the OOBE isn't complete and the update is
  // deemed critical. Otherwise, similar to OnUpdateErrorCheckingForUpdate(), we
  // do not want to block users from being able to proceed to the login screen.
  if (is_out_of_box_ && is_critical_update)
    ShowNetworkScreen();
  else
    OnUpdateCompleted();
}

void WizardController::EnableUserImageScreenReturnToPreviousHack() {
  user_image_screen_return_to_previous_hack_ = true;
}

void WizardController::OnUserImageSelected() {
  if (user_image_screen_return_to_previous_hack_) {
    user_image_screen_return_to_previous_hack_ = false;
    DCHECK(previous_screen_);
    if (previous_screen_) {
      SetCurrentScreen(previous_screen_);
      return;
    }
  }
  OnOobeFlowFinished();
}

void WizardController::OnEnrollmentDone() {
  PerformOOBECompletedActions();

  // Restart to make the login page pick up the policy changes resulting from
  // enrollment recovery.  (Not pretty, but this codepath is rarely exercised.)
  if (prescribed_enrollment_config_.mode ==
      policy::EnrollmentConfig::MODE_RECOVERY) {
    chrome::AttemptRestart();
  }

  // TODO(mnissler): Unify the logic for auto-login for Public Sessions and
  // Kiosk Apps and make this code cover both cases: http://crbug.com/234694.
  if (KioskAppManager::Get()->IsAutoLaunchEnabled())
    AutoLaunchKioskApp();
  else
    ShowLoginScreen(LoginScreenContext());
}

void WizardController::OnDeviceModificationCanceled() {
  if (previous_screen_) {
    SetCurrentScreen(previous_screen_);
  } else {
    ShowLoginScreen(LoginScreenContext());
  }
}

void WizardController::OnKioskAutolaunchCanceled() {
  ShowLoginScreen(LoginScreenContext());
}

void WizardController::OnKioskAutolaunchConfirmed() {
  DCHECK(KioskAppManager::Get()->IsAutoLaunchEnabled());
  AutoLaunchKioskApp();
}

void WizardController::OnKioskEnableCompleted() {
  ShowLoginScreen(LoginScreenContext());
}

void WizardController::OnWrongHWIDWarningSkipped() {
  if (previous_screen_)
    SetCurrentScreen(previous_screen_);
  else
    ShowLoginScreen(LoginScreenContext());
}

void WizardController::OnTermsOfServiceDeclined() {
  // If the user declines the Terms of Service, end the session and return to
  // the login screen.
  DBusThreadManager::Get()->GetSessionManagerClient()->StopSession();
}

void WizardController::OnTermsOfServiceAccepted() {
  ShowSyncConsentScreen();
}

void WizardController::OnSyncConsentFinished() {
  if (chromeos::quick_unlock::IsFingerprintEnabled(
          ProfileManager::GetActiveUserProfile())) {
    ShowFingerprintSetupScreen();
  } else {
    ShowDiscoverScreen();
  }
}

void WizardController::OnDiscoverScreenFinished() {
  ShowMarketingOptInScreen();
}

void WizardController::OnMarketingOptInFinished() {
  ShowArcTermsOfServiceScreen();
}

void WizardController::OnFingerprintSetupFinished() {
  ShowDiscoverScreen();
}

void WizardController::OnArcTermsOfServiceSkipped() {
  DCHECK(!arc::IsArcTermsOfServiceOobeNegotiationNeeded());

  if (is_in_session_oobe_) {
    OnOobeFlowFinished();
    return;
  }
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

  if (is_in_session_oobe_) {
    ShowWaitForContainerReadyScreen();
    return;
  }

  // If the recommend app screen should be shown, show it after the user
  // finished with the PlayStore Terms of Service. Otherwise, advance to the
  // assistant opt-in flow screen.
  if (ShouldShowRecommendAppsScreen()) {
    ShowRecommendAppsScreen();
  } else {
    ShowAssistantOptInFlowScreen();
  }
}

void WizardController::OnArcTermsOfServiceBack() {
  DCHECK(demo_setup_controller_);
  DCHECK(StartupUtils::IsEulaAccepted());
  ShowNetworkScreen();
}

void WizardController::OnRecommendAppsSkipped() {
  ShowAssistantOptInFlowScreen();
}

void WizardController::OnRecommendAppsSelected() {
  ShowAppDownloadingScreen();
}

void WizardController::OnAppDownloadingFinished() {
  ShowAssistantOptInFlowScreen();
}

void WizardController::OnVoiceInteractionValuePropSkipped() {
  OnOobeFlowFinished();
}

void WizardController::OnVoiceInteractionValuePropAccepted() {
  if (is_in_session_oobe_ && arc::IsArcTermsOfServiceOobeNegotiationNeeded()) {
    ShowArcTermsOfServiceScreen();
    return;
  }
  ShowWaitForContainerReadyScreen();
}

void WizardController::OnWaitForContainerReadyFinished() {
  OnOobeFlowFinished();
  StartVoiceInteractionSetupWizard();
}

void WizardController::OnAssistantOptInFlowFinished() {
  ShowMultiDeviceSetupScreen();
}

void WizardController::OnMultiDeviceSetupFinished() {
  ShowUserImageScreen();
}

void WizardController::OnControllerPairingFinished() {
  ShowAutoEnrollmentCheckScreen();
}

void WizardController::OnAutoEnrollmentCheckCompleted() {
  // Check whether the device is disabled. OnDeviceDisabledChecked() will be
  // invoked when the result of this check is known. Until then, the current
  // screen will remain visible and will continue showing a spinner.
  g_browser_process->platform_part()
      ->device_disabling_manager()
      ->CheckWhetherDeviceDisabledDuringOOBE(
          base::Bind(&WizardController::OnDeviceDisabledChecked,
                     weak_factory_.GetWeakPtr()));
}

void WizardController::OnDemoSetupFinished() {
  DCHECK(demo_setup_controller_);
  demo_setup_controller_.reset();
  PerformOOBECompletedActions();
  ShowLoginScreen(LoginScreenContext());
}

void WizardController::OnDemoSetupCanceled() {
  DCHECK(demo_setup_controller_);
  demo_setup_controller_.reset();
  ShowWelcomeScreen();
}

void WizardController::OnDemoPreferencesContinued() {
  DCHECK(demo_setup_controller_);
  ShowNetworkScreen();
}

void WizardController::OnDemoPreferencesCanceled() {
  DCHECK(demo_setup_controller_);
  demo_setup_controller_.reset();
  ShowWelcomeScreen();
}

void WizardController::OnOobeFlowFinished() {
  if (is_in_session_oobe_) {
    GetLoginDisplayHost()->SetStatusAreaVisible(true);
    GetLoginDisplayHost()->Finalize(base::OnceClosure());
    return;
  }

  if (!time_oobe_started_.is_null()) {
    base::TimeDelta delta = base::Time::Now() - time_oobe_started_;
    UMA_HISTOGRAM_CUSTOM_TIMES("OOBE.BootToSignInCompleted", delta,
                               base::TimeDelta::FromMilliseconds(10),
                               base::TimeDelta::FromMinutes(30), 100);
    time_oobe_started_ = base::Time();
  }

  // Launch browser and delete login host controller.
  base::PostTaskWithTraits(
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
  VLOG(1) << "StartOOBEUpdate";
  SetCurrentScreenSmooth(GetScreen(OobeScreen::SCREEN_OOBE_UPDATE), true);
  UpdateScreen::Get(screen_manager())->StartNetworkCheck();
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

  if (shark_connection_listener_.get())
    shark_connection_listener_->ResetController();
}

void WizardController::SetCurrentScreen(BaseScreen* new_current) {
  SetCurrentScreenSmooth(new_current, false);
}

void WizardController::ShowCurrentScreen() {
  // ShowCurrentScreen may get called by smooth_show_timer_ even after
  // flow has been switched to sign in screen (ExistingUserController).
  if (!GetOobeUI())
    return;

  // First remember how far have we reached so that we can resume if needed.
  if (is_out_of_box_ && !demo_setup_controller_ &&
      IsResumableScreen(current_screen_->screen_id())) {
    StartupUtils::SaveOobePendingScreen(
        GetOobeScreenName(current_screen_->screen_id()));
  }

  smooth_show_timer_.Stop();

  UpdateStatusAreaVisibilityForScreen(current_screen_->screen_id());
  current_screen_->SetConfiguration(&oobe_configuration_, false /*notify */);
  current_screen_->Show();
}

void WizardController::SetCurrentScreenSmooth(BaseScreen* new_current,
                                              bool use_smoothing) {
  VLOG(1) << "SetCurrentScreenSmooth: "
          << GetOobeScreenName(new_current->screen_id());
  if (current_screen_ == new_current || new_current == nullptr ||
      GetOobeUI() == nullptr) {
    return;
  }

  smooth_show_timer_.Stop();

  if (current_screen_) {
    current_screen_->Hide();
    current_screen_->SetConfiguration(nullptr, false /*notify */);
  }

  const OobeScreen screen = new_current->screen_id();
  if (IsOOBEStepToTrack(screen))
    screen_show_times_[GetOobeScreenName(screen)] = base::Time::Now();

  previous_screen_ = current_screen_;
  current_screen_ = new_current;

  if (use_smoothing) {
    smooth_show_timer_.Start(FROM_HERE,
                             base::TimeDelta::FromMilliseconds(g_show_delay_ms),
                             this, &WizardController::ShowCurrentScreen);
  } else {
    ShowCurrentScreen();
  }
}

void WizardController::UpdateStatusAreaVisibilityForScreen(OobeScreen screen) {
  if (screen == OobeScreen::SCREEN_OOBE_WELCOME) {
    // Hide the status area initially; it only appears after OOBE first animates
    // in. Keep it visible if the user goes back to the existing welcome screen.
    GetLoginDisplayHost()->SetStatusAreaVisible(
        screen_manager_->HasScreen(OobeScreen::SCREEN_OOBE_WELCOME));
  } else if (screen == OobeScreen::SCREEN_OOBE_RESET ||
             screen == OobeScreen::SCREEN_KIOSK_ENABLE ||
             screen == OobeScreen::SCREEN_KIOSK_AUTOLAUNCH ||
             screen == OobeScreen::SCREEN_OOBE_ENABLE_DEBUGGING ||
             screen == OobeScreen::SCREEN_WRONG_HWID ||
             screen == OobeScreen::SCREEN_ARC_KIOSK_SPLASH ||
             screen == OobeScreen::SCREEN_OOBE_CONTROLLER_PAIRING ||
             screen == OobeScreen::SCREEN_OOBE_HOST_PAIRING) {
    GetLoginDisplayHost()->SetStatusAreaVisible(false);
  } else {
    GetLoginDisplayHost()->SetStatusAreaVisible(true);
  }
}

void WizardController::OnHIDScreenNecessityCheck(bool screen_needed) {
  if (!GetOobeUI())
    return;

  if (screen_needed) {
    ShowHIDDetectionScreen();
  } else {
    ShowWelcomeScreen();
  }
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

void WizardController::AdvanceToScreen(OobeScreen screen) {
  if (screen == OobeScreen::SCREEN_OOBE_WELCOME) {
    ShowWelcomeScreen();
  } else if (screen == OobeScreen::SCREEN_OOBE_NETWORK) {
    ShowNetworkScreen();
  } else if (screen == OobeScreen::SCREEN_SPECIAL_LOGIN) {
    ShowLoginScreen(LoginScreenContext());
  } else if (screen == OobeScreen::SCREEN_OOBE_UPDATE) {
    InitiateOOBEUpdate();
  } else if (screen == OobeScreen::SCREEN_USER_IMAGE_PICKER) {
    ShowUserImageScreen();
  } else if (screen == OobeScreen::SCREEN_OOBE_EULA) {
    ShowEulaScreen();
  } else if (screen == OobeScreen::SCREEN_OOBE_RESET) {
    ShowResetScreen();
  } else if (screen == OobeScreen::SCREEN_KIOSK_ENABLE) {
    ShowKioskEnableScreen();
  } else if (screen == OobeScreen::SCREEN_KIOSK_AUTOLAUNCH) {
    ShowKioskAutolaunchScreen();
  } else if (screen == OobeScreen::SCREEN_OOBE_ENABLE_DEBUGGING) {
    ShowEnableDebuggingScreen();
  } else if (screen == OobeScreen::SCREEN_OOBE_ENROLLMENT) {
    ShowEnrollmentScreen();
  } else if (screen == OobeScreen::SCREEN_OOBE_DEMO_SETUP) {
    ShowDemoModeSetupScreen();
  } else if (screen == OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES) {
    ShowDemoModePreferencesScreen();
  } else if (screen == OobeScreen::SCREEN_TERMS_OF_SERVICE) {
    ShowTermsOfServiceScreen();
  } else if (screen == OobeScreen::SCREEN_SYNC_CONSENT) {
    ShowSyncConsentScreen();
  } else if (screen == OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE) {
    ShowArcTermsOfServiceScreen();
  } else if (screen == OobeScreen::SCREEN_RECOMMEND_APPS) {
    ShowRecommendAppsScreen();
  } else if (screen == OobeScreen::SCREEN_APP_DOWNLOADING) {
    ShowAppDownloadingScreen();
  } else if (screen == OobeScreen::SCREEN_WRONG_HWID) {
    ShowWrongHWIDScreen();
  } else if (screen == OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK) {
    ShowAutoEnrollmentCheckScreen();
  } else if (screen == OobeScreen::SCREEN_APP_LAUNCH_SPLASH) {
    AutoLaunchKioskApp();
  } else if (screen == OobeScreen::SCREEN_ARC_KIOSK_SPLASH) {
    ShowArcKioskSplashScreen();
  } else if (screen == OobeScreen::SCREEN_OOBE_HID_DETECTION) {
    ShowHIDDetectionScreen();
  } else if (screen == OobeScreen::SCREEN_OOBE_CONTROLLER_PAIRING) {
    ShowControllerPairingScreen();
  } else if (screen == OobeScreen::SCREEN_OOBE_HOST_PAIRING) {
    ShowHostPairingScreen();
  } else if (screen == OobeScreen::SCREEN_DEVICE_DISABLED) {
    ShowDeviceDisabledScreen();
  } else if (screen == OobeScreen::SCREEN_ENCRYPTION_MIGRATION) {
    ShowEncryptionMigrationScreen();
  } else if (screen == OobeScreen::SCREEN_VOICE_INTERACTION_VALUE_PROP) {
    ShowVoiceInteractionValuePropScreen();
  } else if (screen == OobeScreen::SCREEN_WAIT_FOR_CONTAINER_READY) {
    ShowWaitForContainerReadyScreen();
  } else if (screen == OobeScreen::SCREEN_UPDATE_REQUIRED) {
    ShowUpdateRequiredScreen();
  } else if (screen == OobeScreen::SCREEN_ASSISTANT_OPTIN_FLOW) {
    ShowAssistantOptInFlowScreen();
  } else if (screen == OobeScreen::SCREEN_MULTIDEVICE_SETUP) {
    ShowMultiDeviceSetupScreen();
  } else if (screen == OobeScreen::SCREEN_DISCOVER) {
    ShowDiscoverScreen();
  } else if (screen == OobeScreen::SCREEN_FINGERPRINT_SETUP) {
    ShowFingerprintSetupScreen();
  } else if (screen == OobeScreen::SCREEN_MARKETING_OPT_IN) {
    ShowMarketingOptInScreen();
  } else if (screen != OobeScreen::SCREEN_TEST_NO_WINDOW) {
    if (is_out_of_box_) {
      time_oobe_started_ = base::Time::Now();

      if (IsRemoraPairingOobe() || IsControllerDetected()) {
        ShowHostPairingScreen();
      } else if (CanShowHIDDetectionScreen()) {
        hid_screen_ = GetScreen(OobeScreen::SCREEN_OOBE_HID_DETECTION);
        base::Callback<void(bool)> on_check =
            base::Bind(&WizardController::OnHIDScreenNecessityCheck,
                       weak_factory_.GetWeakPtr());
        GetOobeUI()->GetHIDDetectionView()->CheckIsScreenRequired(on_check);
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
  demo_setup_controller_ = std::make_unique<DemoSetupController>();
  if (demo_config.has_value())
    demo_setup_controller_->set_demo_config(*demo_config);
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, BaseScreenDelegate overrides:
void WizardController::OnExit(ScreenExitCode exit_code) {
  VLOG(1) << "Wizard screen exit code: " << ExitCodeToString(exit_code);
  const OobeScreen previous_screen = current_screen_->screen_id();
  if (IsOOBEStepToTrack(previous_screen)) {
    RecordUMAHistogramForOOBEStepCompletionTime(
        previous_screen,
        base::Time::Now() -
            screen_show_times_[GetOobeScreenName(previous_screen)]);
  }
  switch (exit_code) {
    case ScreenExitCode::HID_DETECTION_COMPLETED:
      OnHIDDetectionCompleted();
      break;
    case ScreenExitCode::WELCOME_CONTINUED:
      OnWelcomeContinued();
      break;
    case ScreenExitCode::NETWORK_BACK:
      OnNetworkBack();
      break;
    case ScreenExitCode::NETWORK_CONNECTED:
      OnNetworkConnected();
      break;
    case ScreenExitCode::NETWORK_OFFLINE_DEMO_SETUP:
      OnOfflineDemoModeSetup();
      break;
    case ScreenExitCode::CONNECTION_FAILED:
      OnConnectionFailed();
      break;
    case ScreenExitCode::UPDATE_INSTALLED:
    case ScreenExitCode::UPDATE_NOUPDATE:
      OnUpdateCompleted();
      break;
    case ScreenExitCode::UPDATE_REJECT_OVER_CELLULAR:
      OnUpdateOverCellularRejected();
      return;
    case ScreenExitCode::UPDATE_ERROR_CHECKING_FOR_UPDATE:
      OnUpdateErrorCheckingForUpdate();
      break;
    case ScreenExitCode::UPDATE_ERROR_UPDATING:
      OnUpdateErrorUpdating(false /* is_critical_update */);
      break;
    case ScreenExitCode::UPDATE_ERROR_UPDATING_CRITICAL_UPDATE:
      OnUpdateErrorUpdating(true /* is_critical_update */);
      break;
    case ScreenExitCode::USER_IMAGE_SELECTED:
      OnUserImageSelected();
      break;
    case ScreenExitCode::EULA_ACCEPTED:
      OnEulaAccepted();
      break;
    case ScreenExitCode::EULA_BACK:
      OnEulaBack();
      break;
    case ScreenExitCode::ENABLE_DEBUGGING_CANCELED:
      OnDeviceModificationCanceled();
      break;
    case ScreenExitCode::ENABLE_DEBUGGING_FINISHED:
      OnDeviceModificationCanceled();
      break;
    case ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED:
      OnAutoEnrollmentCheckCompleted();
      break;
    case ScreenExitCode::ENTERPRISE_ENROLLMENT_COMPLETED:
      OnEnrollmentDone();
      break;
    case ScreenExitCode::ENTERPRISE_ENROLLMENT_BACK:
      retry_auto_enrollment_check_ = true;
      ShowAutoEnrollmentCheckScreen();
      break;
    case ScreenExitCode::RESET_CANCELED:
      OnDeviceModificationCanceled();
      break;
    case ScreenExitCode::KIOSK_AUTOLAUNCH_CANCELED:
      OnKioskAutolaunchCanceled();
      break;
    case ScreenExitCode::KIOSK_AUTOLAUNCH_CONFIRMED:
      OnKioskAutolaunchConfirmed();
      break;
    case ScreenExitCode::KIOSK_ENABLE_COMPLETED:
      OnKioskEnableCompleted();
      break;
    case ScreenExitCode::TERMS_OF_SERVICE_DECLINED:
      OnTermsOfServiceDeclined();
      break;
    case ScreenExitCode::TERMS_OF_SERVICE_ACCEPTED:
      OnTermsOfServiceAccepted();
      break;
    case ScreenExitCode::ARC_TERMS_OF_SERVICE_SKIPPED:
      OnArcTermsOfServiceSkipped();
      break;
    case ScreenExitCode::ARC_TERMS_OF_SERVICE_ACCEPTED:
      OnArcTermsOfServiceAccepted();
      break;
    case ScreenExitCode::ARC_TERMS_OF_SERVICE_BACK:
      OnArcTermsOfServiceBack();
      break;
    case ScreenExitCode::WRONG_HWID_WARNING_SKIPPED:
      OnWrongHWIDWarningSkipped();
      break;
    case ScreenExitCode::CONTROLLER_PAIRING_FINISHED:
      OnControllerPairingFinished();
      break;
    case ScreenExitCode::VOICE_INTERACTION_VALUE_PROP_SKIPPED:
      OnVoiceInteractionValuePropSkipped();
      break;
    case ScreenExitCode::VOICE_INTERACTION_VALUE_PROP_ACCEPTED:
      OnVoiceInteractionValuePropAccepted();
      break;
    case ScreenExitCode::WAIT_FOR_CONTAINER_READY_FINISHED:
      OnWaitForContainerReadyFinished();
      break;
    case ScreenExitCode::WAIT_FOR_CONTAINER_READY_ERROR:
      OnOobeFlowFinished();
      break;
    case ScreenExitCode::SYNC_CONSENT_FINISHED:
      OnSyncConsentFinished();
      break;
    case ScreenExitCode::RECOMMEND_APPS_SKIPPED:
      OnRecommendAppsSkipped();
      break;
    case ScreenExitCode::RECOMMEND_APPS_SELECTED:
      OnRecommendAppsSelected();
      break;
    case ScreenExitCode::APP_DOWNLOADING_FINISHED:
      OnAppDownloadingFinished();
      break;
    case ScreenExitCode::DEMO_MODE_SETUP_FINISHED:
      OnDemoSetupFinished();
      break;
    case ScreenExitCode::DEMO_MODE_SETUP_CANCELED:
      OnDemoSetupCanceled();
      break;
    case ScreenExitCode::DEMO_MODE_PREFERENCES_CONTINUED:
      OnDemoPreferencesContinued();
      break;
    case ScreenExitCode::DEMO_MODE_PREFERENCES_CANCELED:
      OnDemoPreferencesCanceled();
      break;
    case ScreenExitCode::DISCOVER_FINISHED:
      OnDiscoverScreenFinished();
      break;
    case ScreenExitCode::FINGERPRINT_SETUP_FINISHED:
      OnFingerprintSetupFinished();
      break;
    case ScreenExitCode::MARKETING_OPT_IN_FINISHED:
      OnMarketingOptInFinished();
      break;
    case ScreenExitCode::ASSISTANT_OPTIN_FLOW_FINISHED:
      OnAssistantOptInFlowFinished();
      break;
    case ScreenExitCode::MULTIDEVICE_SETUP_FINISHED:
      OnMultiDeviceSetupFinished();
      break;
    default:
      NOTREACHED();
  }
}

void WizardController::ShowErrorScreen() {
  VLOG(1) << "Showing error screen.";
  SetCurrentScreen(GetScreen(OobeScreen::SCREEN_ERROR_MESSAGE));
}

void WizardController::HideErrorScreen(BaseScreen* parent_screen) {
  DCHECK(parent_screen);
  VLOG(1) << "Hiding error screen.";
  SetCurrentScreen(parent_screen);
}

void WizardController::SetUsageStatisticsReporting(bool val) {
  usage_statistics_reporting_ = val;
}

bool WizardController::GetUsageStatisticsReporting() const {
  return usage_statistics_reporting_;
}

void WizardController::SetHostNetwork() {
  if (!shark_controller_)
    return;
  std::string onc_spec;
  network_state_helper_->GetConnectedWifiNetwork(&onc_spec);
  if (!onc_spec.empty())
    shark_controller_->SetHostNetwork(onc_spec);
}

void WizardController::SetHostConfiguration() {
  if (!shark_controller_)
    return;
  WelcomeScreen* welcome_screen = WelcomeScreen::Get(screen_manager());
  shark_controller_->SetHostConfiguration(
      true,  // Eula must be accepted before we get this far.
      welcome_screen->GetApplicationLocale(), welcome_screen->GetTimezone(),
      GetUsageStatisticsReporting(), welcome_screen->GetInputMethod());
}

void WizardController::ConfigureHostRequested(
    bool accepted_eula,
    const std::string& lang,
    const std::string& timezone,
    bool send_reports,
    const std::string& keyboard_layout) {
  VLOG(1) << "ConfigureHost locale=" << lang << ", timezone=" << timezone
          << ", keyboard_layout=" << keyboard_layout;
  if (accepted_eula)  // Always true.
    StartupUtils::MarkEulaAccepted();
  SetUsageStatisticsReporting(send_reports);

  WelcomeScreen* welcome_screen = WelcomeScreen::Get(screen_manager());
  welcome_screen->SetApplicationLocaleAndInputMethod(lang, keyboard_layout);
  welcome_screen->SetTimezone(timezone);

  // Don't block the OOBE update and the following enrollment process if there
  // is available and valid network already.
  const chromeos::NetworkState* network_state = chromeos::NetworkHandler::Get()
                                                    ->network_state_handler()
                                                    ->DefaultNetwork();
  if (NetworkAllowUpdate(network_state))
    InitiateOOBEUpdate();
}

void WizardController::AddNetworkRequested(const std::string& onc_spec) {
  remora_controller_->OnNetworkConnectivityChanged(
      pairing_chromeos::HostPairingController::CONNECTIVITY_CONNECTING);

  const chromeos::NetworkState* network_state = chromeos::NetworkHandler::Get()
                                                    ->network_state_handler()
                                                    ->DefaultNetwork();
  if (NetworkAllowUpdate(network_state)) {
    network_state_helper_->CreateAndConnectNetworkFromOnc(
        onc_spec, base::DoNothing(), network_handler::ErrorCallback());
  } else {
    network_state_helper_->CreateAndConnectNetworkFromOnc(
        onc_spec,
        base::Bind(&WizardController::OnSetHostNetworkSuccessful,
                   weak_factory_.GetWeakPtr()),
        base::Bind(&WizardController::OnSetHostNetworkFailed,
                   weak_factory_.GetWeakPtr()));
  }
}

void WizardController::RebootHostRequested() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart(
      power_manager::REQUEST_RESTART_OTHER, "login wizard reboot host");
}

void WizardController::OnEnableDebuggingScreenRequested() {
  if (!login_screen_started())
    AdvanceToScreen(OobeScreen::SCREEN_OOBE_ENABLE_DEBUGGING);
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
  g_show_delay_ms = 0;
}

// static
bool WizardController::IsZeroDelayEnabled() {
  return g_show_delay_ms == 0;
}

// static
bool WizardController::IsOOBEStepToTrack(OobeScreen screen_id) {
  return (screen_id == OobeScreen::SCREEN_OOBE_HID_DETECTION ||
          screen_id == OobeScreen::SCREEN_OOBE_WELCOME ||
          screen_id == OobeScreen::SCREEN_OOBE_UPDATE ||
          screen_id == OobeScreen::SCREEN_USER_IMAGE_PICKER ||
          screen_id == OobeScreen::SCREEN_OOBE_EULA ||
          screen_id == OobeScreen::SCREEN_SPECIAL_LOGIN ||
          screen_id == OobeScreen::SCREEN_WRONG_HWID);
}

// static
void WizardController::SkipPostLoginScreensForTesting() {
  skip_post_login_screens_ = true;
  if (!default_controller() || !default_controller()->current_screen())
    return;

  const OobeScreen current_screen_id =
      default_controller()->current_screen()->screen_id();
  if (current_screen_id == OobeScreen::SCREEN_TERMS_OF_SERVICE ||
      current_screen_id == OobeScreen::SCREEN_SYNC_CONSENT ||
      current_screen_id == OobeScreen::SCREEN_FINGERPRINT_SETUP ||
      current_screen_id == OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE ||
      current_screen_id == OobeScreen::SCREEN_USER_IMAGE_PICKER ||
      current_screen_id == OobeScreen::SCREEN_DISCOVER ||
      current_screen_id == OobeScreen::SCREEN_MARKETING_OPT_IN) {
    default_controller()->OnOobeFlowFinished();
  } else {
    LOG(WARNING) << "SkipPostLoginScreensForTesting(): Ignore screen "
                 << static_cast<int>(current_screen_id);
  }
}

// static
void WizardController::SkipEnrollmentPromptsForTesting() {
  skip_enrollment_prompts_ = true;
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

bool WizardController::IsRemoraPairingOobe() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kHostPairingOobe);
}

bool WizardController::ShouldShowVoiceInteractionValueProp() const {
  // If the OOBE flow was initiated from voice interaction shortcut, we will
  // show Arc terms later.
  if (!is_in_session_oobe_ && !arc::IsArcPlayStoreEnabledForProfile(
                                  ProfileManager::GetActiveUserProfile())) {
    VLOG(1) << "Skip Voice Interaction Value Prop screen because Arc Terms is "
            << "skipped.";
    return false;
  }
  if (!chromeos::switches::IsVoiceInteractionEnabled()) {
    VLOG(1) << "Skip Voice Interaction Value Prop screen because voice "
            << "interaction service is disabled.";
    return false;
  }
  return true;
}

void WizardController::StartVoiceInteractionSetupWizard() {
  auto* service =
      arc::ArcVoiceInteractionFrameworkService::GetForBrowserContext(
          ProfileManager::GetActiveUserProfile());
  if (service)
    service->StartVoiceInteractionSetupWizard();
}

void WizardController::MaybeStartListeningForSharkConnection() {
  // We shouldn't be here if we are running pairing OOBE already.
  if (IsControllerDetected())
    return;

  if (!shark_connection_listener_) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(content::ServiceManagerConnection::GetForProcess());
    service_manager::Connector* connector =
        content::ServiceManagerConnection::GetForProcess()->GetConnector();
    shark_connection_listener_ =
        std::make_unique<pairing_chromeos::SharkConnectionListener>(
            connector, base::Bind(&WizardController::OnSharkConnected,
                                  weak_factory_.GetWeakPtr()));
  }
}

void WizardController::OnSharkConnected(
    std::unique_ptr<pairing_chromeos::HostPairingController>
        remora_controller) {
  VLOG(1) << "OnSharkConnected";
  remora_controller_ = std::move(remora_controller);
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
      FROM_HERE, shark_connection_listener_.release());
  SetControllerDetectedPref(true);
  ShowHostPairingScreen();
}

void WizardController::OnSetHostNetworkSuccessful() {
  remora_controller_->OnNetworkConnectivityChanged(
      pairing_chromeos::HostPairingController::CONNECTIVITY_CONNECTED);
  InitiateOOBEUpdate();
}

void WizardController::OnSetHostNetworkFailed(
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  std::string error_message;
  JSONStringValueSerializer serializer(&error_message);
  serializer.Serialize(*error_data);
  error_message = error_name + ": " + error_message;

  remora_controller_->SetErrorCodeAndMessage(
      static_cast<int>(
          pairing_chromeos::HostPairingController::ErrorCode::NETWORK_ERROR),
      error_message);

  remora_controller_->OnNetworkConnectivityChanged(
      pairing_chromeos::HostPairingController::CONNECTIVITY_NONE);
}

void WizardController::StartEnrollmentScreen(bool force_interactive) {
  VLOG(1) << "Showing enrollment screen."
          << " Forcing interactive enrollment: " << force_interactive << ".";

  // Determine the effective enrollment configuration. If there is a valid
  // prescribed configuration, use that. If not, figure out which variant of
  // manual enrollment is taking place.
  policy::EnrollmentConfig effective_config = prescribed_enrollment_config_;
  if (!effective_config.should_enroll() ||
      (force_interactive && !effective_config.should_enroll_interactively())) {
    effective_config.mode =
        prescribed_enrollment_config_.management_domain.empty()
            ? policy::EnrollmentConfig::MODE_MANUAL
            : policy::EnrollmentConfig::MODE_MANUAL_REENROLLMENT;
  }

  EnrollmentScreen* screen = EnrollmentScreen::Get(screen_manager());
  screen->SetParameters(effective_config, shark_controller_.get());
  UpdateStatusAreaVisibilityForScreen(OobeScreen::SCREEN_OOBE_ENROLLMENT);
  SetCurrentScreen(screen);
}

AutoEnrollmentController* WizardController::GetAutoEnrollmentController() {
  if (!auto_enrollment_controller_)
    auto_enrollment_controller_ = std::make_unique<AutoEnrollmentController>();
  return auto_enrollment_controller_.get();
}

}  // namespace chromeos
