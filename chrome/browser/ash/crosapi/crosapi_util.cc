// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/crosapi_util.h"

#include <sys/mman.h>

#include "ash/components/settings/cros_settings_provider.h"
#include "ash/components/tpm/install_attributes.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/files/file_util.h"
#include "chrome/browser/apps/app_service/extension_apps_utils.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/browser_version_service_ash.h"
#include "chrome/browser/ash/crosapi/field_trial_service_ash.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/browser/ash/crosapi/native_theme_service_ash.h"
#include "chrome/browser/ash/crosapi/resource_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/components/cdm_factory_daemon/mojom/browser_cdm_factory.mojom.h"
#include "chromeos/components/sensors/mojom/cros_sensor_service.mojom.h"
#include "chromeos/constants/devicetype.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "chromeos/crosapi/mojom/app_window_tracker.mojom.h"
#include "chromeos/crosapi/mojom/arc.mojom.h"
#include "chromeos/crosapi/mojom/authentication.mojom.h"
#include "chromeos/crosapi/mojom/automation.mojom.h"
#include "chromeos/crosapi/mojom/browser_app_instance_registry.mojom.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "chromeos/crosapi/mojom/clipboard.mojom.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/crosapi/mojom/content_protection.mojom.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/desk_template.mojom.h"
#include "chromeos/crosapi/mojom/device_attributes.mojom.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "chromeos/crosapi/mojom/dlp.mojom.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "chromeos/crosapi/mojom/drive_integration_service.mojom.h"
#include "chromeos/crosapi/mojom/feedback.mojom.h"
#include "chromeos/crosapi/mojom/file_manager.mojom.h"
#include "chromeos/crosapi/mojom/force_installed_tracker.mojom.h"
#include "chromeos/crosapi/mojom/geolocation.mojom.h"
#include "chromeos/crosapi/mojom/holding_space_service.mojom.h"
#include "chromeos/crosapi/mojom/identity_manager.mojom.h"
#include "chromeos/crosapi/mojom/image_writer.mojom.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "chromeos/crosapi/mojom/kiosk_session_service.mojom.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/crosapi/mojom/login.mojom.h"
#include "chromeos/crosapi/mojom/login_screen_storage.mojom.h"
#include "chromeos/crosapi/mojom/login_state.mojom.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/metrics_reporting.mojom.h"
#include "chromeos/crosapi/mojom/network_settings_service.mojom.h"
#include "chromeos/crosapi/mojom/networking_attributes.mojom.h"
#include "chromeos/crosapi/mojom/policy_service.mojom.h"
#include "chromeos/crosapi/mojom/power.mojom.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/crosapi/mojom/remoting.mojom.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/crosapi/mojom/sharesheet.mojom.h"
#include "chromeos/crosapi/mojom/structured_metrics_service.mojom.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "chromeos/crosapi/mojom/system_display.mojom.h"
#include "chromeos/crosapi/mojom/task_manager.mojom.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/crosapi/mojom/timezone.mojom.h"
#include "chromeos/crosapi/mojom/tts.mojom.h"
#include "chromeos/crosapi/mojom/url_handler.mojom.h"
#include "chromeos/crosapi/mojom/video_capture.mojom.h"
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#include "chromeos/crosapi/mojom/web_page_info.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/ukm_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "media/capture/mojom/video_capture.mojom.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

using MojoOptionalBool = crosapi::mojom::DeviceSettings::OptionalBool;
using MojoPolicyMap =
    base::flat_map<::policy::PolicyNamespace, std::vector<uint8_t>>;

namespace crosapi {
namespace browser_util {

namespace {

// Capability to support reloading the lacros browser on receiving a
// notification that the browser component was successfully updated.
constexpr char kBrowserManagerReloadBrowserCapability[] = "crbug/1237235";

// Returns the vector containing policy data of the device account. In case of
// an error, returns nullopt.
absl::optional<std::vector<uint8_t>> GetDeviceAccountPolicy(
    EnvironmentProvider* environment_provider) {
  if (!user_manager::UserManager::IsInitialized()) {
    LOG(ERROR) << "User not initialized.";
    return absl::nullopt;
  }
  const auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user) {
    LOG(ERROR) << "No primary user.";
    return absl::nullopt;
  }
  std::string policy_data = environment_provider->GetDeviceAccountPolicy();
  return std::vector<uint8_t>(policy_data.begin(), policy_data.end());
}

// Returns the map containing component policy for each namespace. The values
// represent the serialized policy blob for the namespace.
const absl::optional<MojoPolicyMap> GetDeviceAccountComponentPolicy(
    EnvironmentProvider* environment_provider) {
  const MojoPolicyMap& map =
      environment_provider->GetDeviceAccountComponentPolicy();
  if (map.empty())
    return absl::nullopt;

  return map;
}

// Returns the device specific data needed for Lacros.
mojom::DevicePropertiesPtr GetDeviceProperties() {
  mojom::DevicePropertiesPtr result = mojom::DeviceProperties::New();
  result->device_dm_token = "";

  if (ash::DeviceSettingsService::IsInitialized()) {
    const enterprise_management::PolicyData* policy_data =
        ash::DeviceSettingsService::Get()->policy_data();

    if (policy_data && policy_data->has_request_token())
      result->device_dm_token = policy_data->request_token();

    if (policy_data && !policy_data->device_affiliation_ids().empty()) {
      const auto& ids = policy_data->device_affiliation_ids();
      result->device_affiliation_ids = {ids.begin(), ids.end()};
    }
  }

  return result;
}

struct InterfaceVersionEntry {
  base::Token uuid;
  uint32_t version;
};

template <typename T>
constexpr InterfaceVersionEntry MakeInterfaceVersionEntry() {
  return {T::Uuid_, T::Version_};
}

constexpr InterfaceVersionEntry kInterfaceVersionEntries[] = {
    MakeInterfaceVersionEntry<chromeos::cdm::mojom::BrowserCdmFactory>(),
    MakeInterfaceVersionEntry<chromeos::sensors::mojom::SensorHalClient>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Arc>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Authentication>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Automation>(),
    MakeInterfaceVersionEntry<crosapi::mojom::AccountManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::AppPublisher>(),
    MakeInterfaceVersionEntry<crosapi::mojom::AppServiceProxy>(),
    MakeInterfaceVersionEntry<crosapi::mojom::AppWindowTracker>(),
    MakeInterfaceVersionEntry<crosapi::mojom::BrowserAppInstanceRegistry>(),
    MakeInterfaceVersionEntry<crosapi::mojom::BrowserServiceHost>(),
    MakeInterfaceVersionEntry<crosapi::mojom::BrowserVersionService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::CertDatabase>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Clipboard>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ClipboardHistory>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ContentProtection>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Crosapi>(),
    MakeInterfaceVersionEntry<crosapi::mojom::DeskTemplate>(),
    MakeInterfaceVersionEntry<crosapi::mojom::DeviceAttributes>(),
    MakeInterfaceVersionEntry<crosapi::mojom::DeviceSettingsService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Dlp>(),
    MakeInterfaceVersionEntry<crosapi::mojom::DownloadController>(),
    MakeInterfaceVersionEntry<crosapi::mojom::DriveIntegrationService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Feedback>(),
    MakeInterfaceVersionEntry<crosapi::mojom::FieldTrialService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::FileManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ForceInstalledTracker>(),
    MakeInterfaceVersionEntry<crosapi::mojom::GeolocationService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::HoldingSpaceService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::IdentityManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::IdleService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ImageWriter>(),
    MakeInterfaceVersionEntry<crosapi::mojom::KeystoreService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::KioskSessionService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::LocalPrinter>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Login>(),
    MakeInterfaceVersionEntry<crosapi::mojom::LoginScreenStorage>(),
    MakeInterfaceVersionEntry<crosapi::mojom::LoginState>(),
    MakeInterfaceVersionEntry<
        chromeos::machine_learning::mojom::MachineLearningService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::MessageCenter>(),
    MakeInterfaceVersionEntry<crosapi::mojom::MetricsReporting>(),
    MakeInterfaceVersionEntry<crosapi::mojom::NativeThemeService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::NetworkingAttributes>(),
    MakeInterfaceVersionEntry<crosapi::mojom::NetworkSettingsService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::PolicyService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Power>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Prefs>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Remoting>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ResourceManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ScreenManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::SearchControllerRegistry>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Sharesheet>(),
    MakeInterfaceVersionEntry<crosapi::mojom::StructuredMetricsService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::SnapshotCapturer>(),
    MakeInterfaceVersionEntry<crosapi::mojom::SyncService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::SystemDisplay>(),
    MakeInterfaceVersionEntry<crosapi::mojom::TaskManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::TestController>(),
    MakeInterfaceVersionEntry<crosapi::mojom::TimeZoneService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Tts>(),
    MakeInterfaceVersionEntry<crosapi::mojom::UrlHandler>(),
    MakeInterfaceVersionEntry<crosapi::mojom::VideoCaptureDeviceFactory>(),
    MakeInterfaceVersionEntry<crosapi::mojom::WebAppService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::WebPageInfoFactory>(),
    MakeInterfaceVersionEntry<device::mojom::HidConnection>(),
    MakeInterfaceVersionEntry<device::mojom::HidManager>(),
    MakeInterfaceVersionEntry<
        media::stable::mojom::StableVideoDecoderFactory>(),
    MakeInterfaceVersionEntry<media_session::mojom::MediaControllerManager>(),
    MakeInterfaceVersionEntry<media_session::mojom::AudioFocusManager>(),
    MakeInterfaceVersionEntry<media_session::mojom::AudioFocusManagerDebug>(),
};

constexpr bool HasDuplicatedUuid() {
  // We assume the number of entries are small enough so that simple
  // O(N^2) check works.
  const size_t size = std::size(kInterfaceVersionEntries);
  for (size_t i = 0; i < size; ++i) {
    for (size_t j = i + 1; j < size; ++j) {
      if (kInterfaceVersionEntries[i].uuid == kInterfaceVersionEntries[j].uuid)
        return true;
    }
  }
  return false;
}

static_assert(!HasDuplicatedUuid(),
              "Each Crosapi Mojom interface should have unique UUID.");

crosapi::mojom::BrowserInitParams::DeviceType ConvertDeviceType(
    chromeos::DeviceType device_type) {
  switch (device_type) {
    case chromeos::DeviceType::kChromebook:
      return crosapi::mojom::BrowserInitParams::DeviceType::kChromebook;
    case chromeos::DeviceType::kChromebase:
      return crosapi::mojom::BrowserInitParams::DeviceType::kChromebase;
    case chromeos::DeviceType::kChromebit:
      return crosapi::mojom::BrowserInitParams::DeviceType::kChromebit;
    case chromeos::DeviceType::kChromebox:
      return crosapi::mojom::BrowserInitParams::DeviceType::kChromebox;
    case chromeos::DeviceType::kUnknown:
      [[fallthrough]];
    default:
      return crosapi::mojom::BrowserInitParams::DeviceType::kUnknown;
  }
}

}  // namespace

base::flat_map<base::Token, uint32_t> GetInterfaceVersions() {
  base::flat_map<base::Token, uint32_t> versions;
  for (const auto& entry : kInterfaceVersionEntries)
    versions.emplace(entry.uuid, entry.version);
  return versions;
}

InitialBrowserAction::InitialBrowserAction(
    crosapi::mojom::InitialBrowserAction action)
    : action(action) {
  // kOpnWindowWIthUrls should take the argument, so the ctor below should be
  // used.
  DCHECK_NE(action, crosapi::mojom::InitialBrowserAction::kOpenWindowWithUrls);
}

InitialBrowserAction::InitialBrowserAction(
    crosapi::mojom::InitialBrowserAction action,
    std::vector<GURL> urls,
    crosapi::mojom::OpenUrlFrom from)
    : action(action), urls(std::move(urls)), from(from) {
  // Currently, only kOpenWindowWithUrls can take the URLs as its argument.
  DCHECK_EQ(action, crosapi::mojom::InitialBrowserAction::kOpenWindowWithUrls);
}

InitialBrowserAction::InitialBrowserAction(InitialBrowserAction&&) = default;
InitialBrowserAction& InitialBrowserAction::operator=(InitialBrowserAction&&) =
    default;

InitialBrowserAction::~InitialBrowserAction() = default;

mojom::BrowserInitParamsPtr GetBrowserInitParams(
    EnvironmentProvider* environment_provider,
    InitialBrowserAction initial_browser_action,
    bool is_keep_alive_enabled) {
  auto params = mojom::BrowserInitParams::New();
  params->crosapi_version = crosapi::mojom::Crosapi::Version_;
  params->deprecated_ash_metrics_enabled_has_value = true;
  PrefService* local_state = g_browser_process->local_state();
  params->ash_metrics_enabled =
      local_state->GetBoolean(metrics::prefs::kMetricsReportingEnabled);
  params->ash_metrics_managed =
      local_state->IsManagedPreference(metrics::prefs::kMetricsReportingEnabled)
          ? mojom::MetricsReportingManaged::kManaged
          : mojom::MetricsReportingManaged::kNotManaged;

  params->session_type = environment_provider->GetSessionType();
  params->device_mode = environment_provider->GetDeviceMode();
  params->interface_versions = GetInterfaceVersions();
  params->default_paths = environment_provider->GetDefaultPaths();
  params->use_new_account_manager =
      environment_provider->GetUseNewAccountManager();

  params->device_account_gaia_id =
      environment_provider->GetDeviceAccountGaiaId();
  const absl::optional<account_manager::Account> maybe_device_account =
      environment_provider->GetDeviceAccount();
  if (maybe_device_account) {
    params->device_account =
        account_manager::ToMojoAccount(maybe_device_account.value());
  }

  // TODO(crbug.com/1093194): This should be updated to a new value when
  // the long term fix is made in ash-chrome, atomically.
  params->exo_ime_support =
      crosapi::mojom::ExoImeSupport::kConsumedByImeWorkaround;
  params->cros_user_id_hash = ash::ProfileHelper::GetUserIdHashFromProfile(
      ProfileManager::GetPrimaryUserProfile());
  params->device_account_policy = GetDeviceAccountPolicy(environment_provider);
  params->idle_info = IdleServiceAsh::ReadIdleInfoFromSystem();
  params->native_theme_info = NativeThemeServiceAsh::GetNativeThemeInfo();

  params->initial_browser_action = initial_browser_action.action;
  if (initial_browser_action.action ==
      crosapi::mojom::InitialBrowserAction::kOpenWindowWithUrls) {
    params->startup_urls = std::move(initial_browser_action.urls);
    params->startup_urls_from = initial_browser_action.from;
  }

  params->web_apps_enabled = web_app::IsWebAppsCrosapiEnabled();
  params->standalone_browser_is_primary = IsLacrosPrimaryBrowser();
  params->device_properties = GetDeviceProperties();
  params->device_settings = GetDeviceSettings();
  // |metrics_service| could be nullptr in tests.
  if (auto* metrics_service = g_browser_process->metrics_service()) {
    // Send metrics service client id to Lacros if it's present.
    std::string client_id = metrics_service->GetClientId();
    if (!client_id.empty())
      params->metrics_service_client_id = client_id;
  }

  if (auto* metrics_services_manager =
          g_browser_process->GetMetricsServicesManager()) {
    if (auto* ukm_service = metrics_services_manager->GetUkmService()) {
      params->ukm_client_id = ukm_service->client_id();
    }
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kOndeviceHandwritingSwitch)) {
    const auto handwriting_switch =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            ash::switches::kOndeviceHandwritingSwitch);

    // TODO(https://crbug.com/1168978): Query mlservice instead of using
    // hard-coded values.
    if (handwriting_switch == "use_rootfs") {
      params->ondevice_handwriting_support =
          crosapi::mojom::OndeviceHandwritingSupport::kUseRootfs;
    } else if (handwriting_switch == "use_dlc") {
      params->ondevice_handwriting_support =
          crosapi::mojom::OndeviceHandwritingSupport::kUseDlc;
    } else {
      params->ondevice_handwriting_support =
          crosapi::mojom::OndeviceHandwritingSupport::kUnsupported;
    }
  }

  // Add any BUILDFLAGs we use to pass our per-platform/ build configuration to
  // lacros for runtime handling instead.
  std::vector<crosapi::mojom::BuildFlag> build_flags;
#if BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_HEVC)
  build_flags.emplace_back(
      crosapi::mojom::BuildFlag::kEnablePlatformEncryptedHevc);
#endif  // BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_HEVC)
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  build_flags.emplace_back(crosapi::mojom::BuildFlag::kEnablePlatformHevc);
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
  build_flags.emplace_back(
      crosapi::mojom::BuildFlag::kUseChromeosProtectedMedia);
#endif  // BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_AV1)
  build_flags.emplace_back(crosapi::mojom::BuildFlag::kUseChromeosProtectedAv1);
#endif  // BUILDFLAG(USE_CHROMEOS_PROTECTED_AV1)
  params->build_flags = std::move(build_flags);

  params->standalone_browser_is_only_browser = !IsAshWebBrowserEnabled();
  params->publish_chrome_apps = browser_util::IsLacrosChromeAppsEnabled();
  params->publish_hosted_apps = apps::ShouldHostedAppsRunInLacros();

  // Keep-alive mojom API is now used by the current ash-chrome.
  params->initial_keep_alive =
      is_keep_alive_enabled
          ? crosapi::mojom::BrowserInitParams::InitialKeepAlive::kEnabled
          : crosapi::mojom::BrowserInitParams::InitialKeepAlive::kDisabled;

  params->is_unfiltered_bluetooth_device_enabled =
      ash::switches::IsUnfilteredBluetoothDevicesEnabled();

  // Pass the accepted internal urls to lacros. Only accepted urls are allowed
  // to be passed via OpenURL from Lacros to Ash.
  params->accepted_internal_ash_urls =
      ChromeWebUIControllerFactory::GetInstance()->GetListOfAcceptableURLs();

  // Pass holding space feature flag state to lacros.
  params->is_holding_space_incognito_profile_integration_enabled = true;
  params
      ->is_holding_space_in_progress_downloads_notification_suppression_enabled =
      ash::features::
          IsHoldingSpaceInProgressDownloadsNotificationSuppressionEnabled();

  params->ash_capabilities = {{kBrowserManagerReloadBrowserCapability}};

  params->is_device_enterprised_managed =
      ash::InstallAttributes::Get()->IsEnterpriseManaged();

  params->device_type = ConvertDeviceType(chromeos::GetDeviceType());
  params->device_account_component_policy =
      GetDeviceAccountComponentPolicy(environment_provider);

  params->is_ondevice_speech_supported =
      base::FeatureList::IsEnabled(ash::features::kOnDeviceSpeechRecognition);

  return params;
}

base::ScopedFD CreateStartupData(EnvironmentProvider* environment_provider,
                                 InitialBrowserAction initial_browser_action,
                                 bool is_keep_alive_enabled) {
  auto data = GetBrowserInitParams(environment_provider,
                                   std::move(initial_browser_action),
                                   is_keep_alive_enabled);
  std::vector<uint8_t> serialized =
      crosapi::mojom::BrowserInitParams::Serialize(&data);

  base::ScopedFD fd(memfd_create("startup_data", 0));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Failed to create a memory backed file";
    return base::ScopedFD();
  }

  if (!base::WriteFileDescriptor(fd.get(), serialized)) {
    LOG(ERROR) << "Failed to dump the serialized startup data";
    return base::ScopedFD();
  }

  if (lseek(fd.get(), 0, SEEK_SET) < 0) {
    PLOG(ERROR) << "Failed to reset the FD position";
    return base::ScopedFD();
  }

  return fd;
}

bool IsSigninProfileOrBelongsToAffiliatedUser(Profile* profile) {
  if (ash::ProfileHelper::IsSigninProfile(profile))
    return true;

  if (profile->IsOffTheRecord())
    return false;

  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return false;
  return user->IsAffiliated();
}

// Returns the device policy data needed for Lacros.
mojom::DeviceSettingsPtr GetDeviceSettings() {
  mojom::DeviceSettingsPtr result = mojom::DeviceSettings::New();

  result->attestation_for_content_protection_enabled = MojoOptionalBool::kUnset;
  if (ash::CrosSettings::IsInitialized()) {
    // It's expected that the CrosSettings values are trusted. The only
    // theoretical exception is when device ownership is taken on consumer
    // device. Then there's no settings to be passed to Lacros anyway.
    auto trusted_result =
        ash::CrosSettings::Get()->PrepareTrustedValues(base::DoNothing());
    if (trusted_result == ash::CrosSettingsProvider::TRUSTED) {
      const auto* cros_settings = ash::CrosSettings::Get();
      bool attestation_enabled = false;
      if (cros_settings->GetBoolean(
              ash::kAttestationForContentProtectionEnabled,
              &attestation_enabled)) {
        result->attestation_for_content_protection_enabled =
            attestation_enabled ? MojoOptionalBool::kTrue
                                : MojoOptionalBool::kFalse;
      }

      const base::ListValue* usb_detachable_allow_list;
      if (cros_settings->GetList(ash::kUsbDetachableAllowlist,
                                 &usb_detachable_allow_list)) {
        mojom::UsbDetachableAllowlistPtr allow_list =
            mojom::UsbDetachableAllowlist::New();
        for (const auto& entry :
             usb_detachable_allow_list->GetListDeprecated()) {
          mojom::UsbDeviceIdPtr usb_device_id = mojom::UsbDeviceId::New();
          absl::optional<int> vid =
              entry.FindIntKey(ash::kUsbDetachableAllowlistKeyVid);
          if (vid) {
            usb_device_id->has_vendor_id = true;
            usb_device_id->vendor_id = vid.value();
          }
          absl::optional<int> pid =
              entry.FindIntKey(ash::kUsbDetachableAllowlistKeyPid);
          if (pid) {
            usb_device_id->has_product_id = true;
            usb_device_id->product_id = pid.value();
          }
          allow_list->usb_device_ids.push_back(std::move(usb_device_id));
        }
        result->usb_detachable_allow_list = std::move(allow_list);
      }
    } else {
      LOG(WARNING) << "Unexpected crossettings trusted values status: "
                   << trusted_result;
    }
  }

  result->device_system_wide_tracing_enabled = MojoOptionalBool::kUnset;
  auto* local_state = g_browser_process->local_state();
  if (local_state) {
    auto* pref = local_state->FindPreference(
        ash::prefs::kDeviceSystemWideTracingEnabled);
    if (pref && pref->IsManaged()) {
      result->device_system_wide_tracing_enabled =
          pref->GetValue()->GetBool() ? MojoOptionalBool::kTrue
                                      : MojoOptionalBool::kFalse;
    }
  }

  return result;
}

}  // namespace browser_util
}  // namespace crosapi
