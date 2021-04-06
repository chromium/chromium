// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_platform_part_chromeos.h"

#include <utility>

#include "ash/components/account_manager/account_manager_factory.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/memory/singleton.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/ash/login/saml/in_session_password_change_manager.h"
#include "chrome/browser/ash/login/session/chrome_session_manager.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/system/automatic_reboot_manager.h"
#include "chrome/browser/ash/system/device_disabling_manager.h"
#include "chrome/browser/ash/system/device_disabling_manager_default_delegate.h"
#include "chrome/browser/ash/system/system_clock.h"
#include "chrome/browser/ash/system/timezone_resolver_manager.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/net/delay_network_call.h"
#include "chrome/browser/chromeos/net/system_proxy_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/scheduler_configuration_manager.h"
#include "chrome/browser/component_updater/metadata_table_chromeos.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/geolocation/simple_geolocation_provider.h"
#include "chromeos/timezone/timezone_resolver.h"
#include "components/arc/enterprise/arc_data_snapshotd_manager.h"
#include "components/arc/enterprise/snapshot_hours_policy_service.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/preferences/public/mojom/preferences.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/cpp/service.h"

namespace {

class PrimaryProfileServicesShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static PrimaryProfileServicesShutdownNotifierFactory* GetInstance() {
    return base::Singleton<
        PrimaryProfileServicesShutdownNotifierFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<
      PrimaryProfileServicesShutdownNotifierFactory>;

  PrimaryProfileServicesShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "PrimaryProfileServices") {}
  ~PrimaryProfileServicesShutdownNotifierFactory() override {}

  DISALLOW_COPY_AND_ASSIGN(PrimaryProfileServicesShutdownNotifierFactory);
};

}  // namespace

BrowserProcessPlatformPart::BrowserProcessPlatformPart()
    : created_profile_helper_(false),
      account_manager_factory_(std::make_unique<ash::AccountManagerFactory>()) {
}

BrowserProcessPlatformPart::~BrowserProcessPlatformPart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BrowserProcessPlatformPart::InitializeAutomaticRebootManager() {
  DCHECK(!automatic_reboot_manager_);

  automatic_reboot_manager_.reset(new ash::system::AutomaticRebootManager(
      base::DefaultTickClock::GetInstance()));
}

void BrowserProcessPlatformPart::ShutdownAutomaticRebootManager() {
  automatic_reboot_manager_.reset();
}

void BrowserProcessPlatformPart::InitializeChromeUserManager() {
  DCHECK(!chrome_user_manager_);
  chrome_user_manager_ = ash::ChromeUserManagerImpl::CreateChromeUserManager();
  chrome_user_manager_->Initialize();
}

void BrowserProcessPlatformPart::DestroyChromeUserManager() {
  chrome_user_manager_->Destroy();
  chrome_user_manager_.reset();
}

void BrowserProcessPlatformPart::InitializeDeviceDisablingManager() {
  DCHECK(!device_disabling_manager_);

  device_disabling_manager_delegate_.reset(
      new ash::system::DeviceDisablingManagerDefaultDelegate);
  device_disabling_manager_.reset(new ash::system::DeviceDisablingManager(
      device_disabling_manager_delegate_.get(), ash::CrosSettings::Get(),
      user_manager::UserManager::Get()));
  device_disabling_manager_->Init();
}

void BrowserProcessPlatformPart::ShutdownDeviceDisablingManager() {
  device_disabling_manager_.reset();
  device_disabling_manager_delegate_.reset();
}

void BrowserProcessPlatformPart::InitializeSessionManager() {
  DCHECK(!session_manager_);
  session_manager_ = std::make_unique<chromeos::ChromeSessionManager>();
}

void BrowserProcessPlatformPart::ShutdownSessionManager() {
  session_manager_.reset();
}

void BrowserProcessPlatformPart::InitializeCrosComponentManager() {
  if (using_testing_cros_component_manager_)
    return;

  DCHECK(!cros_component_manager_);
  cros_component_manager_ =
      base::MakeRefCounted<component_updater::CrOSComponentInstaller>(
          std::make_unique<component_updater::MetadataTable>(
              g_browser_process->local_state()),
          g_browser_process->component_updater());

  // Register all installed components for regular update.
  cros_component_manager_->RegisterInstalled();
}

void BrowserProcessPlatformPart::ShutdownCrosComponentManager() {
  if (using_testing_cros_component_manager_)
    return;

  cros_component_manager_.reset();
}

void BrowserProcessPlatformPart::InitializeSchedulerConfigurationManager() {
  DCHECK(!scheduler_configuration_manager_);
  scheduler_configuration_manager_ =
      std::make_unique<chromeos::SchedulerConfigurationManager>(
          chromeos::DBusThreadManager::Get()->GetDebugDaemonClient(),
          g_browser_process->local_state());
}

void BrowserProcessPlatformPart::ShutdownSchedulerConfigurationManager() {
  scheduler_configuration_manager_.reset();
}

void BrowserProcessPlatformPart::InitializePrimaryProfileServices(
    Profile* primary_profile) {
  DCHECK(primary_profile);

  DCHECK(!in_session_password_change_manager_);
  in_session_password_change_manager_ =
      chromeos::InSessionPasswordChangeManager::CreateIfEnabled(
          primary_profile);

  primary_profile_shutdown_subscription_ =
      PrimaryProfileServicesShutdownNotifierFactory::GetInstance()
          ->Get(primary_profile)
          ->Subscribe(base::BindRepeating(
              &BrowserProcessPlatformPart::ShutdownPrimaryProfileServices,
              base::Unretained(this)));

  if (chromeos::SystemProxyManager::Get()) {
    chromeos::SystemProxyManager::Get()->StartObservingPrimaryProfilePrefs(
        primary_profile);
  }

  auto* manager = arc::data_snapshotd::ArcDataSnapshotdManager::Get();
  if (manager) {
    manager->policy_service()->StartObservingPrimaryProfilePrefs(
        primary_profile->GetPrefs());
  }
}

void BrowserProcessPlatformPart::ShutdownPrimaryProfileServices() {
  auto* manager = arc::data_snapshotd::ArcDataSnapshotdManager::Get();
  if (manager)
    manager->policy_service()->StopObservingPrimaryProfilePrefs();

  if (chromeos::SystemProxyManager::Get())
    chromeos::SystemProxyManager::Get()->StopObservingPrimaryProfilePrefs();
  in_session_password_change_manager_.reset();
}

void BrowserProcessPlatformPart::RegisterKeepAlive() {
  DCHECK(!keep_alive_);
  keep_alive_.reset(
      new ScopedKeepAlive(KeepAliveOrigin::BROWSER_PROCESS_CHROMEOS,
                          KeepAliveRestartOption::DISABLED));
}

void BrowserProcessPlatformPart::UnregisterKeepAlive() {
  keep_alive_.reset();
}

ash::ProfileHelper* BrowserProcessPlatformPart::profile_helper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!created_profile_helper_)
    CreateProfileHelper();
  return profile_helper_.get();
}

policy::BrowserPolicyConnectorChromeOS*
BrowserProcessPlatformPart::browser_policy_connector_chromeos() {
  return static_cast<policy::BrowserPolicyConnectorChromeOS*>(
      g_browser_process->browser_policy_connector());
}

chromeos::system::TimeZoneResolverManager*
BrowserProcessPlatformPart::GetTimezoneResolverManager() {
  if (!timezone_resolver_manager_.get()) {
    timezone_resolver_manager_.reset(
        new chromeos::system::TimeZoneResolverManager());
  }
  return timezone_resolver_manager_.get();
}

chromeos::TimeZoneResolver* BrowserProcessPlatformPart::GetTimezoneResolver() {
  if (!timezone_resolver_.get()) {
    timezone_resolver_.reset(new chromeos::TimeZoneResolver(
        GetTimezoneResolverManager(),
        g_browser_process->shared_url_loader_factory(),
        chromeos::SimpleGeolocationProvider::DefaultGeolocationProviderURL(),
        base::BindRepeating(&ash::system::ApplyTimeZone),
        base::BindRepeating(&chromeos::DelayNetworkCall,
                            base::TimeDelta::FromMilliseconds(
                                chromeos::kDefaultNetworkRetryDelayMS)),
        g_browser_process->local_state()));
  }
  return timezone_resolver_.get();
}

void BrowserProcessPlatformPart::StartTearDown() {
  // interactive_ui_tests check for memory leaks before this object is
  // destroyed.  So we need to destroy |timezone_resolver_| here.
  timezone_resolver_.reset();
  profile_helper_.reset();
}

chromeos::system::SystemClock* BrowserProcessPlatformPart::GetSystemClock() {
  if (!system_clock_.get())
    system_clock_.reset(new chromeos::system::SystemClock());
  return system_clock_.get();
}

void BrowserProcessPlatformPart::DestroySystemClock() {
  system_clock_.reset();
}

void BrowserProcessPlatformPart::CreateProfileHelper() {
  DCHECK(!created_profile_helper_ && !profile_helper_);
  created_profile_helper_ = true;
  profile_helper_ = ash::ProfileHelper::CreateInstance();
}

ash::AccountManagerFactory*
BrowserProcessPlatformPart::GetAccountManagerFactory() {
  return account_manager_factory_.get();
}
