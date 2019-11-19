// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/dbus_helper.h"

#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/wilco_dtc_supportd/wilco_dtc_supportd_client.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/constants/chromeos_paths.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/arc_camera_client.h"
#include "chromeos/dbus/audio/cras_audio_client.h"
#include "chromeos/dbus/auth_policy/auth_policy_client.h"
#include "chromeos/dbus/biod/biod_client.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cups_proxy/cups_proxy_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/dbus/initialize_dbus_client.h"
#include "chromeos/dbus/kerberos/kerberos_client.h"
#include "chromeos/dbus/machine_learning/machine_learning_client.h"
#include "chromeos/dbus/media_analytics/media_analytics_client.h"
#include "chromeos/dbus/permission_broker/permission_broker_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/dbus/system_clock/system_clock_client.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "chromeos/tpm/install_attributes.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"

namespace {

void OverrideStubPathsIfNeeded() {
  base::FilePath user_data_dir;
  if (!base::SysInfo::IsRunningOnChromeOS() &&
      base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    chromeos::RegisterStubPathOverrides(user_data_dir);
  }
}

}  // namespace

namespace chromeos {

void InitializeDBus() {
  OverrideStubPathsIfNeeded();

  SystemSaltGetter::Initialize();

  // Initialize DBusThreadManager for the browser.
  DBusThreadManager::Initialize(DBusThreadManager::kAll);

  // Initialize Chrome dbus clients.
  dbus::Bus* bus = DBusThreadManager::Get()->GetSystemBus();

  // NOTE: base::Feature is not initialized yet, so any non MultiProcessMash
  // dbus client initialization for Ash should be done in Shell::Init.
  InitializeDBusClient<ArcCameraClient>(bus);
  InitializeDBusClient<AuthPolicyClient>(bus);
  InitializeDBusClient<BiodClient>(bus);  // For device::Fingerprint.
  InitializeDBusClient<CrasAudioClient>(bus);
  InitializeDBusClient<CrosHealthdClient>(bus);
  InitializeDBusClient<CryptohomeClient>(bus);
  InitializeDBusClient<CupsProxyClient>(bus);
  InitializeDBusClient<DlcserviceClient>(bus);
  InitializeDBusClient<KerberosClient>(bus);
  InitializeDBusClient<MachineLearningClient>(bus);
  InitializeDBusClient<MediaAnalyticsClient>(bus);
  InitializeDBusClient<PermissionBrokerClient>(bus);
  InitializeDBusClient<PowerManagerClient>(bus);
  InitializeDBusClient<SessionManagerClient>(bus);
  InitializeDBusClient<SystemClockClient>(bus);
  InitializeDBusClient<UpstartClient>(bus);

  // Initialize the device settings service so that we'll take actions per
  // signals sent from the session manager. This needs to happen before
  // g_browser_process initializes BrowserPolicyConnector.
  DeviceSettingsService::Initialize();
  InstallAttributes::Initialize();
}

void InitializeFeatureListDependentDBus() {
  dbus::Bus* bus = DBusThreadManager::Get()->GetSystemBus();
  InitializeDBusClient<bluez::BluezDBusManager>(bus);
  InitializeDBusClient<WilcoDtcSupportdClient>(bus);
}

void ShutdownDBus() {
  // Feature list-dependent D-Bus clients are shut down first because we try to
  // shut down in reverse order of initialization (in case of dependencies).
  WilcoDtcSupportdClient::Shutdown();
  bluez::BluezDBusManager::Shutdown();

  // Other D-Bus clients are shut down, also in reverse order of initialization.
  UpstartClient::Shutdown();
  SystemClockClient::Shutdown();
  SessionManagerClient::Shutdown();
  PowerManagerClient::Shutdown();
  PermissionBrokerClient::Shutdown();
  MediaAnalyticsClient::Shutdown();
  MachineLearningClient::Shutdown();
  KerberosClient::Shutdown();
  DlcserviceClient::Shutdown();
  CupsProxyClient::Shutdown();
  CryptohomeClient::Shutdown();
  CrosHealthdClient::Shutdown();
  CrasAudioClient::Shutdown();
  BiodClient::Shutdown();
  AuthPolicyClient::Shutdown();
  ArcCameraClient::Shutdown();

  DBusThreadManager::Shutdown();
  SystemSaltGetter::Shutdown();
}

}  // namespace chromeos
