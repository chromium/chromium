// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/ash_dbus_helper.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_paths.h"
#include "ash/webui/shimless_rma/shimless_rma.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "build/config/chromebox_for_meetings/buildflags.h"  // PLATFORM_CFM
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/attestation/attestation_features.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/anomaly_detector/anomaly_detector_client.h"
#include "chromeos/ash/components/dbus/arc/arc_appfuse_provider_client.h"
#include "chromeos/ash/components/dbus/arc/arc_camera_client.h"
#include "chromeos/ash/components/dbus/arc/arc_keymaster_client.h"
#include "chromeos/ash/components/dbus/arc/arc_keymint_client.h"
#include "chromeos/ash/components/dbus/arc/arc_midis_client.h"
#include "chromeos/ash/components/dbus/arc/arc_obb_mounter_client.h"
#include "chromeos/ash/components/dbus/arc/arcvm_data_migrator_client.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/audio/cras_audio_client.h"
#include "chromeos/ash/components/dbus/audio/floss_media_client.h"
#include "chromeos/ash/components/dbus/biod/biod_client.h"
#include "chromeos/ash/components/dbus/cdm_factory_daemon/cdm_factory_daemon_client.h"
#include "chromeos/ash/components/dbus/cec_service/cec_service_client.h"
#include "chromeos/ash/components/dbus/chaps/chaps_client.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/dbus/cups_proxy/cups_proxy_client.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/device_management/install_attributes_client.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/dbus/easy_unlock/easy_unlock_client.h"
#include "chromeos/ash/components/dbus/featured/featured_client.h"
#include "chromeos/ash/components/dbus/federated/federated_client.h"
#include "chromeos/ash/components/dbus/gnubby/gnubby_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_clients.h"
#include "chromeos/ash/components/dbus/human_presence/human_presence_dbus_client.h"
#include "chromeos/ash/components/dbus/image_burner/image_burner_client.h"
#include "chromeos/ash/components/dbus/image_loader/image_loader_client.h"
#include "chromeos/ash/components/dbus/kerberos/kerberos_client.h"
#include "chromeos/ash/components/dbus/lorgnette_manager/lorgnette_manager_client.h"
#include "chromeos/ash/components/dbus/media_analytics/media_analytics_client.h"
#include "chromeos/ash/components/dbus/oobe_config/oobe_configuration_client.h"
#include "chromeos/ash/components/dbus/os_install/os_install_client.h"
#include "chromeos/ash/components/dbus/patchpanel/patchpanel_client.h"
#include "chromeos/ash/components/dbus/pciguard/pciguard_client.h"
#include "chromeos/ash/components/dbus/printscanmgr/printscanmgr_client.h"
#include "chromeos/ash/components/dbus/private_computing/private_computing_client.h"
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"
#include "chromeos/ash/components/dbus/rgbkbd/rgbkbd_client.h"
#include "chromeos/ash/components/dbus/rmad/rmad_client.h"
#include "chromeos/ash/components/dbus/runtime_probe/runtime_probe_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/smbprovider/smb_provider_client.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "chromeos/ash/components/dbus/swap_management/swap_management_client.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_client.h"
#include "chromeos/ash/components/dbus/system_proxy/system_proxy_client.h"
#include "chromeos/ash/components/dbus/typecd/typecd_client.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_pkcs11_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/dbus/virtual_file_provider/virtual_file_provider_client.h"
#include "chromeos/ash/components/dbus/vm_plugin_dispatcher/vm_plugin_dispatcher_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/language_packs/language_pack_manager.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/init/initialize_dbus_client.h"
#include "chromeos/dbus/ip_peripheral/ip_peripheral_service_client.h"
#include "chromeos/dbus/machine_learning/machine_learning_client.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "chromeos/dbus/permission_broker/permission_broker_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/regmon/regmon_client.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "chromeos/dbus/u2f/u2f_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_features.h"

#if BUILDFLAG(PLATFORM_CFM)
#include "chromeos/ash/components/chromebox_for_meetings/features.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_hotline_client.h"
#endif

namespace ash {

namespace {

// If running on desktop, override paths so that enrollment and cloud policy
// work correctly, and can be tested.
void OverrideStubPathsIfNeeded() {
  base::FilePath user_data_dir;
  if (!base::SysInfo::IsRunningOnChromeOS() &&
      base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    RegisterStubPathOverrides(user_data_dir);
    chromeos::dbus_paths::RegisterStubPathOverrides(user_data_dir);
  }
}

DBusHelperObserverForTest* g_dbus_helper_observer = nullptr;

}  // namespace

DBusHelperObserverForTest::~DBusHelperObserverForTest() = default;  // IN-TEST

// static
void DBusHelperObserverForTest::Set(DBusHelperObserverForTest* observer) {
  // Only allow set `g_dbus_helper_observer` when it is null or resets it.
  DCHECK(!g_dbus_helper_observer || !observer);

  g_dbus_helper_observer = observer;
}

void InitializeDBus() {
  using chromeos::InitializeDBusClient;

  OverrideStubPathsIfNeeded();

  SystemSaltGetter::Initialize();

  // Initialize DBusThreadManager for the browser.
  DBusThreadManager::Initialize();

  // Initialize Chrome dbus clients.
  dbus::Bus* bus = DBusThreadManager::Get()->GetSystemBus();

  shill_clients::Initialize(bus);

  // NOTE: base::Feature is not initialized yet, so any non MultiProcessMash
  // dbus client initialization for Ash should be done in Shell::Init.
  InitializeDBusClient<AnomalyDetectorClient>(bus);
  InitializeDBusClient<ArcAppfuseProviderClient>(bus);
  InitializeDBusClient<ArcCameraClient>(bus);
  InitializeDBusClient<ArcKeymasterClient>(bus);
  InitializeDBusClient<ArcKeyMintClient>(bus);
  InitializeDBusClient<ArcMidisClient>(bus);
  InitializeDBusClient<ArcObbMounterClient>(bus);
  InitializeDBusClient<ArcVmDataMigratorClient>(bus);
  InitializeDBusClient<AttestationClient>(bus);
  InitializeDBusClient<BiodClient>(bus);  // For device::Fingerprint.
  InitializeDBusClient<CdmFactoryDaemonClient>(bus);
  InitializeDBusClient<CecServiceClient>(bus);
  InitializeDBusClient<ChapsClient>(bus);
  InitializeDBusClient<ChunneldClient>(bus);
  InitializeDBusClient<CiceroneClient>(bus);
  // ConciergeClient depends on CiceroneClient.
  InitializeDBusClient<ConciergeClient>(bus);
  InitializeDBusClient<CrasAudioClient>(bus);
  InitializeDBusClient<CrosDisksClient>(bus);
  InitializeDBusClient<CryptohomeMiscClient>(bus);
  InitializeDBusClient<CryptohomePkcs11Client>(bus);
  InitializeDBusClient<CupsProxyClient>(bus);
  InitializeDBusClient<DebugDaemonClient>(bus);
  InitializeDBusClient<DlcserviceClient>(bus);
  InitializeDBusClient<chromeos::DlpClient>(bus);
  InitializeDBusClient<EasyUnlockClient>(bus);
  InitializeDBusClient<FederatedClient>(bus);
  InitializeDBusClient<GnubbyClient>(bus);
  hermes_clients::Initialize(bus);
  InitializeDBusClient<ImageBurnerClient>(bus);
  InitializeDBusClient<ImageLoaderClient>(bus);
  InitializeDBusClient<InstallAttributesClient>(bus);
  InitializeDBusClient<chromeos::IpPeripheralServiceClient>(bus);
  InitializeDBusClient<KerberosClient>(bus);
  InitializeDBusClient<LorgnetteManagerClient>(bus);
  InitializeDBusClient<chromeos::MachineLearningClient>(bus);
  InitializeDBusClient<MediaAnalyticsClient>(bus);
  InitializeDBusClient<OobeConfigurationClient>(bus);
  InitializeDBusClient<OsInstallClient>(bus);
  InitializeDBusClient<PatchPanelClient>(bus);
  InitializeDBusClient<PciguardClient>(bus);
  InitializeDBusClient<PrintscanmgrClient>(bus);
  InitializeDBusClient<PrivateComputingClient>(bus);
  InitializeDBusClient<chromeos::PermissionBrokerClient>(bus);
  InitializeDBusClient<chromeos::PowerManagerClient>(bus);
  InitializeDBusClient<ResourcedClient>(bus);
  InitializeDBusClient<RuntimeProbeClient>(bus);
  InitializeDBusClient<SeneschalClient>(bus);
  InitializeDBusClient<SessionManagerClient>(bus);
  InitializeDBusClient<SmbProviderClient>(bus);
  InitializeDBusClient<SpacedClient>(bus);
  InitializeDBusClient<SwapManagementClient>(bus);
  InitializeDBusClient<SystemClockClient>(bus);
  InitializeDBusClient<SystemProxyClient>(bus);
  InitializeDBusClient<chromeos::TpmManagerClient>(bus);
  InitializeDBusClient<TypecdClient>(bus);
  InitializeDBusClient<chromeos::U2FClient>(bus);
  InitializeDBusClient<UpdateEngineClient>(bus);
  InitializeDBusClient<UserDataAuthClient>(bus);
  InitializeDBusClient<UpstartClient>(bus);
  InitializeDBusClient<VirtualFileProviderClient>(bus);
  InitializeDBusClient<VmPluginDispatcherClient>(bus);
  InitializeDBusClient<chromeos::RegmonClient>(bus);

  attestation::AttestationFeatures::Initialize();
  // Initialize the device settings service so that we'll take actions per
  // signals sent from the session manager. This needs to happen before
  // g_browser_process initializes BrowserPolicyConnector.
  DeviceSettingsService::Initialize();
  InstallAttributes::Initialize();

  // Depends on `DlcserviceClient`.
  language_packs::LanguagePackManager::Initialise();

  if (g_dbus_helper_observer) {
    g_dbus_helper_observer->PostInitializeDBus();
  }
}

void InitializeFeatureListDependentDBus() {
  using chromeos::InitializeDBusClient;

  dbus::Bus* bus = DBusThreadManager::Get()->GetSystemBus();

  // MissiveClient depends on APIKey which needs to be accessed after the
  // feature list is initialized.
  InitializeDBusClient<chromeos::MissiveClient>(bus);

  if (floss::features::IsFlossEnabled()) {
    InitializeDBusClient<floss::FlossDBusManager>(bus);
    if (bus) {
      int active_adapter =
          floss::FlossDBusManager::Get()->HasActiveAdapter()
              ? floss::FlossDBusManager::Get()->GetActiveAdapter()
              : 0;

      FlossMediaClient::Initialize(
          bus, dbus::ObjectPath(base::StringPrintf(floss::kMediaObjectFormat,
                                                   active_adapter)));
    } else {
      FlossMediaClient::InitializeFake();
    }
  } else {
    InitializeDBusClient<bluez::BluezDBusManager>(bus);
  }
#if BUILDFLAG(PLATFORM_CFM)
  if (base::FeatureList::IsEnabled(cfm::features::kMojoServices)) {
    InitializeDBusClient<CfmHotlineClient>(bus);
  }
#endif
  if (shimless_rma::IsShimlessRmaAllowed()) {
    InitializeDBusClient<RmadClient>(bus);
  }
  InitializeDBusClient<RgbkbdClient>(bus);

  if (features::IsSnoopingProtectionEnabled() ||
      features::IsQuickDimEnabled()) {
    InitializeDBusClient<HumanPresenceDBusClient>(bus);
  }

  // FeaturedClient is not a feature and instead uses the FieldTrialList (which
  // is initialized with the FeatureList) to record early-boot trials in UMA.
  InitializeDBusClient<featured::FeaturedClient>(bus);
}

void ShutdownDBus() {
  if (g_dbus_helper_observer) {
    g_dbus_helper_observer->PreShutdownDBus();
  }

  // Feature list-dependent D-Bus clients are shut down first because we try to
  // shut down in reverse order of initialization (in case of dependencies).
  if (features::IsSnoopingProtectionEnabled() ||
      features::IsQuickDimEnabled()) {
    HumanPresenceDBusClient::Shutdown();
  }
#if BUILDFLAG(PLATFORM_CFM)
  if (base::FeatureList::IsEnabled(cfm::features::kMojoServices)) {
    CfmHotlineClient::Shutdown();
  }
#endif
  if (floss::features::IsFlossEnabled()) {
    FlossMediaClient::Shutdown();
    floss::FlossDBusManager::Shutdown();
  } else {
    bluez::BluezDBusManager::Shutdown();
  }

  // Depends on `DlcserviceClient`.
  language_packs::LanguagePackManager::Shutdown();

  // Other D-Bus clients are shut down, also in reverse order of initialization.
  chromeos::RegmonClient::Shutdown();
  VmPluginDispatcherClient::Shutdown();
  VirtualFileProviderClient::Shutdown();
  UpstartClient::Shutdown();
  UserDataAuthClient::Shutdown();
  UpdateEngineClient::Shutdown();
  chromeos::U2FClient::Shutdown();
  TypecdClient::Shutdown();
  chromeos::TpmManagerClient::Shutdown();
  SystemProxyClient::Shutdown();
  SystemClockClient::Shutdown();
  SwapManagementClient::Shutdown();
  SpacedClient::Shutdown();
  SmbProviderClient::Shutdown();
  SessionManagerClient::Shutdown();
  SeneschalClient::Shutdown();
  RuntimeProbeClient::Shutdown();
  ResourcedClient::Shutdown();
  RgbkbdClient::Shutdown();
  if (shimless_rma::IsShimlessRmaAllowed()) {
    RmadClient::Shutdown();
  }
  chromeos::PowerManagerClient::Shutdown();
  chromeos::PermissionBrokerClient::Shutdown();
  PrintscanmgrClient::Shutdown();
  PciguardClient::Shutdown();
  PatchPanelClient::Shutdown();
  PrivateComputingClient::Shutdown();
  OsInstallClient::Shutdown();
  OobeConfigurationClient::Shutdown();
  chromeos::MissiveClient::Shutdown();
  MediaAnalyticsClient::Shutdown();
  chromeos::MachineLearningClient::Shutdown();
  LorgnetteManagerClient::Shutdown();
  KerberosClient::Shutdown();
  chromeos::IpPeripheralServiceClient::Shutdown();
  InstallAttributesClient::Shutdown();
  ImageLoaderClient::Shutdown();
  ImageBurnerClient::Shutdown();
  hermes_clients::Shutdown();
  GnubbyClient::Shutdown();
  featured::FeaturedClient::Shutdown();
  FederatedClient::Shutdown();
  EasyUnlockClient::Shutdown();
  DlcserviceClient::Shutdown();
  chromeos::DlpClient::Shutdown();
  DebugDaemonClient::Shutdown();
  CupsProxyClient::Shutdown();
  CryptohomePkcs11Client::Shutdown();
  CryptohomeMiscClient::Shutdown();
  CrosDisksClient::Shutdown();
  CrasAudioClient::Shutdown();
  ConciergeClient::Shutdown();
  CiceroneClient::Shutdown();
  ChunneldClient::Shutdown();
  CecServiceClient::Shutdown();
  CdmFactoryDaemonClient::Shutdown();
  BiodClient::Shutdown();
  AttestationClient::Shutdown();
  ArcVmDataMigratorClient::Shutdown();
  ArcObbMounterClient::Shutdown();
  ArcMidisClient::Shutdown();
  ArcKeyMintClient::Shutdown();
  ArcKeymasterClient::Shutdown();
  ArcCameraClient::Shutdown();
  ArcAppfuseProviderClient::Shutdown();
  AnomalyDetectorClient::Shutdown();

  shill_clients::Shutdown();
  DBusThreadManager::Shutdown();
  SystemSaltGetter::Shutdown();
}

}  // namespace ash
