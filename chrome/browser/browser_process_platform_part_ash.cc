// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_platform_part_ash.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/ash/app_list/search/essential_search/essential_search_manager.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/login/saml/in_session_password_change_manager.h"
#include "chrome/browser/ash/login/session/chrome_session_manager.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_registry.h"
#include "chrome/browser/ash/login/users/policy_user_manager_controller.h"
#include "chrome/browser/ash/login/users/profile_user_manager_controller.h"
#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"
#include "chrome/browser/ash/net/ash_proxy_monitor.h"
#include "chrome/browser/ash/net/secure_dns_manager.h"
#include "chrome/browser/ash/net/system_proxy_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/scheduler_config/scheduler_configuration_manager.h"
#include "chrome/browser/ash/settings/cros_settings_holder.h"
#include "chrome/browser/ash/system/automatic_reboot_manager.h"
#include "chrome/browser/ash/system/device_disabling_manager.h"
#include "chrome/browser/ash/system/device_disabling_manager_default_delegate.h"
#include "chrome/browser/ash/system/system_clock.h"
#include "chrome/browser/ash/system/timezone_resolver_manager.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/metadata_table_chromeos.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_flusher.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/policy/restriction_schedule/device_restriction_schedule_controller.h"
#include "chromeos/ash/components/policy/restriction_schedule/device_restriction_schedule_controller_delegate_impl.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/timezone/timezone_resolver.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"
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

  PrimaryProfileServicesShutdownNotifierFactory(
      const PrimaryProfileServicesShutdownNotifierFactory&) = delete;
  PrimaryProfileServicesShutdownNotifierFactory& operator=(
      const PrimaryProfileServicesShutdownNotifierFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<
      PrimaryProfileServicesShutdownNotifierFactory>;

  PrimaryProfileServicesShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "PrimaryProfileServices") {}
  ~PrimaryProfileServicesShutdownNotifierFactory() override {}
};

}  // namespace

BrowserProcessPlatformPart::BrowserProcessPlatformPart()
    : created_profile_helper_(false),
      browser_context_flusher_(std::make_unique<ash::BrowserContextFlusher>()),
      account_manager_factory_(std::make_unique<ash::AccountManagerFactory>()) {
}

BrowserProcessPlatformPart::~BrowserProcessPlatformPart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BrowserProcessPlatformPart::InitializeAutomaticRebootManager() {
  DCHECK(!automatic_reboot_manager_);

  automatic_reboot_manager_ =
      std::make_unique<ash::system::AutomaticRebootManager>(
          base::DefaultClock::GetInstance(),
          base::DefaultTickClock::GetInstance());
}

void BrowserProcessPlatformPart::ShutdownAutomaticRebootManager() {
  automatic_reboot_manager_.reset();
}

void BrowserProcessPlatformPart::InitializeUserManager() {
  DCHECK(!user_manager_);
  CHECK(session_manager_);
  user_manager_ = std::make_unique<user_manager::UserManagerImpl>(
      std::make_unique<ash::UserManagerDelegateImpl>(),
      g_browser_process->local_state(), ash::CrosSettings::Get());
  profile_user_manager_controller_ =
      std::make_unique<ash::ProfileUserManagerController>(
          g_browser_process->profile_manager(), user_manager_.get());
  policy_user_manager_controller_ =
      std::make_unique<ash::PolicyUserManagerController>(
          user_manager_.get(), ash::CrosSettings::Get(),
          ash::DeviceSettingsService::Get(),
          browser_policy_connector_ash()->GetMinimumVersionPolicyHandler());
  user_image_manager_registry_ =
      std::make_unique<ash::UserImageManagerRegistry>(user_manager_.get());
  session_manager_->OnUserManagerCreated(user_manager_.get());
  // LoginState and DeviceCloudPolicyManager outlives UserManager, so on
  // their initialization, there's no way to start observing UserManager.
  // This is the earliest timing to do so.
  // TODO(b/332481586): Consider move the initialization to the constructor
  // of each class.
  if (auto* login_state = ash::LoginState::Get()) {
    login_state->OnUserManagerCreated(user_manager_.get());
  }
  browser_policy_connector_ash()->OnUserManagerCreated(user_manager_.get());
  user_manager_->Initialize();
}

void BrowserProcessPlatformPart::ShutdownUserManager() {
  if (!user_manager_) {
    return;
  }
  user_image_manager_registry_->Shutdown();
  browser_policy_connector_ash()->OnUserManagerShutdown();
  policy_user_manager_controller_.reset();
  user_manager_->Shutdown();
}

void BrowserProcessPlatformPart::DestroyUserManager() {
  user_manager_->Destroy();
  browser_policy_connector_ash()->OnUserManagerWillBeDestroyed();
  if (auto* login_state = ash::LoginState::Get()) {
    login_state->OnUserManagerWillBeDestroyed(user_manager_.get());
  }

  user_image_manager_registry_.reset();
  profile_user_manager_controller_.reset();
  user_manager_.reset();
}

void BrowserProcessPlatformPart::
    InitializeDeviceRestrictionScheduleController() {
  device_restriction_schedule_controller_delegate_impl_ = std::make_unique<
      policy::DeviceRestrictionScheduleControllerDelegateImpl>();
  device_restriction_schedule_controller_ =
      std::make_unique<policy::DeviceRestrictionScheduleController>(
          *device_restriction_schedule_controller_delegate_impl_,
          CHECK_DEREF(g_browser_process->local_state()));
}

void BrowserProcessPlatformPart::ShutdownDeviceRestrictionScheduleController() {
  device_restriction_schedule_controller_.reset();
  device_restriction_schedule_controller_delegate_impl_.reset();
}

void BrowserProcessPlatformPart::InitializeDeviceDisablingManager() {
  DCHECK(!device_disabling_manager_);

  device_disabling_manager_delegate_ =
      std::make_unique<ash::system::DeviceDisablingManagerDefaultDelegate>();
  device_disabling_manager_ =
      std::make_unique<ash::system::DeviceDisablingManager>(
          device_disabling_manager_delegate_.get(), ash::CrosSettings::Get(),
          user_manager::UserManager::Get());
  device_disabling_manager_->Init();
}

void BrowserProcessPlatformPart::ShutdownDeviceDisablingManager() {
  device_disabling_manager_.reset();
  device_disabling_manager_delegate_.reset();
}

void BrowserProcessPlatformPart::InitializeSessionManager() {
  CHECK(!session_manager_);
  session_manager_ = std::make_unique<ash::ChromeSessionManager>();
}

void BrowserProcessPlatformPart::ShutdownSessionManager() {
  session_manager_.reset();
}

void BrowserProcessPlatformPart::InitializeCrosSettings() {
  CHECK(!cros_settings_holder_);
  cros_settings_holder_ = std::make_unique<ash::CrosSettingsHolder>(
      ash::DeviceSettingsService::Get(), g_browser_process->local_state());
}

void BrowserProcessPlatformPart::ShutdownCrosSettings() {
  cros_settings_holder_.reset();
}

void BrowserProcessPlatformPart::InitializeComponentManager() {
  if (using_testing_component_manager_ash_) {
    return;
  }

  DCHECK(!component_manager_ash_);
  component_manager_ash_ =
      base::MakeRefCounted<component_updater::CrOSComponentInstaller>(
          std::make_unique<component_updater::MetadataTable>(
              g_browser_process->local_state()),
          g_browser_process->component_updater());

  // Register all installed components for regular update.
  component_manager_ash_->RegisterInstalled();
}

void BrowserProcessPlatformPart::ShutdownComponentManager() {
  if (using_testing_component_manager_ash_) {
    return;
  }

  component_manager_ash_.reset();
}

void BrowserProcessPlatformPart::InitializeSchedulerConfigurationManager() {
  DCHECK(!scheduler_configuration_manager_);
  scheduler_configuration_manager_ =
      std::make_unique<ash::SchedulerConfigurationManager>(
          ash::DebugDaemonClient::Get(), g_browser_process->local_state());
}

void BrowserProcessPlatformPart::ShutdownSchedulerConfigurationManager() {
  scheduler_configuration_manager_.reset();
}

void BrowserProcessPlatformPart::InitializeAshProxyMonitor() {
  DCHECK(!ash_proxy_monitor_);
  ash_proxy_monitor_ = std::make_unique<ash::AshProxyMonitor>(
      g_browser_process->local_state(), g_browser_process->profile_manager());
}

void BrowserProcessPlatformPart::ShutdownAshProxyMonitor() {
  ash_proxy_monitor_.reset();
}

void BrowserProcessPlatformPart::InitializePrimaryProfileServices(
    Profile* primary_profile) {
  DCHECK(primary_profile);

  DCHECK(!essential_search_manager_);
  essential_search_manager_ =
      app_list::EssentialSearchManager::Create(primary_profile);

  DCHECK(!in_session_password_change_manager_);
  in_session_password_change_manager_ =
      ash::InSessionPasswordChangeManager::CreateIfEnabled(primary_profile);

  primary_profile_shutdown_subscription_ =
      PrimaryProfileServicesShutdownNotifierFactory::GetInstance()
          ->Get(primary_profile)
          ->Subscribe(base::BindRepeating(
              &BrowserProcessPlatformPart::ShutdownPrimaryProfileServices,
              base::Unretained(this)));

  if (ash::SystemProxyManager::Get()) {
    ash::SystemProxyManager::Get()->StartObservingPrimaryProfilePrefs(
        primary_profile);
  }

  secure_dns_manager_ = std::make_unique<ash::SecureDnsManager>(
      g_browser_process->local_state(), primary_profile->GetPrefs(),
      primary_profile->GetProfilePolicyConnector()->IsManaged());
}

void BrowserProcessPlatformPart::ShutdownPrimaryProfileServices() {
  secure_dns_manager_.reset();
  if (ash::SystemProxyManager::Get())
    ash::SystemProxyManager::Get()->StopObservingPrimaryProfilePrefs();
  essential_search_manager_.reset();
  in_session_password_change_manager_.reset();
}

void BrowserProcessPlatformPart::RegisterKeepAlive() {
  DCHECK(!keep_alive_);
  keep_alive_ = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::BROWSER_PROCESS_CHROMEOS,
      KeepAliveRestartOption::DISABLED);
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

policy::BrowserPolicyConnectorAsh*
BrowserProcessPlatformPart::browser_policy_connector_ash() {
  return static_cast<policy::BrowserPolicyConnectorAsh*>(
      g_browser_process->browser_policy_connector());
}

ash::system::TimeZoneResolverManager*
BrowserProcessPlatformPart::GetTimezoneResolverManager() {
  if (!timezone_resolver_manager_.get()) {
    timezone_resolver_manager_ =
        std::make_unique<ash::system::TimeZoneResolverManager>(
            ash::SimpleGeolocationProvider::GetInstance(),
            session_manager::SessionManager::Get());
  }
  return timezone_resolver_manager_.get();
}

void BrowserProcessPlatformPart::StartTearDown() {
  // Some tests check for memory leaks before this object is
  // destroyed.  So we need to destroy |timezone_resolver_manager_| here.
  timezone_resolver_manager_.reset();
  profile_helper_.reset();
  browser_context_flusher_.reset();
}

void BrowserProcessPlatformPart::AttemptExit(bool try_to_quit_application) {
  // Request Lacros terminate early during shutdown to give it the opportunity
  // to shutdown gracefully. Check to make sure `browser_manager` is available
  // as it may be null in tests.
  if (auto* browser_manager = crosapi::BrowserManager::Get())
    browser_manager->Shutdown();

  BrowserProcessPlatformPartChromeOS::AttemptExit(try_to_quit_application);
}

ash::system::SystemClock* BrowserProcessPlatformPart::GetSystemClock() {
  if (!system_clock_.get())
    system_clock_ = std::make_unique<ash::system::SystemClock>();
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

bool BrowserProcessPlatformPart::CanRestoreUrlsForProfile(
    const Profile* profile) const {
  return profile->IsRegularProfile() && !profile->IsSystemProfile() &&
         ash::ProfileHelper::IsUserProfile(profile) &&
         !ash::ProfileHelper::IsEphemeralUserProfile(profile);
}

// static
void BrowserProcessPlatformPart::EnsureFactoryBuilt() {
  PrimaryProfileServicesShutdownNotifierFactory::GetInstance();
}
