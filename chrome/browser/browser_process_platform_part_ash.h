// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_ASH_H_
#define CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_ASH_H_

#include <memory>

#include "base/sequence_checker.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"

class BrowserProcessPlatformPartTestApi;
class Profile;

namespace ash {
class AccountManagerFactory;
class ChromeSessionManager;
class ChromeUserManager;
class InSessionPasswordChangeManager;
class ProfileHelper;
class SchedulerConfigurationManager;
class TimeZoneResolver;

namespace system {
class AutomaticRebootManager;
class DeviceDisablingManager;
class DeviceDisablingManagerDefaultDelegate;
class TimeZoneResolverManager;
class SystemClock;
}  // namespace system
}  // namespace ash

namespace policy {
class BrowserPolicyConnectorAsh;
}

class ScopedKeepAlive;

class BrowserProcessPlatformPart : public BrowserProcessPlatformPartChromeOS {
 public:
  BrowserProcessPlatformPart();

  BrowserProcessPlatformPart(const BrowserProcessPlatformPart&) = delete;
  BrowserProcessPlatformPart& operator=(const BrowserProcessPlatformPart&) =
      delete;

  ~BrowserProcessPlatformPart() override;

  void InitializeAutomaticRebootManager();
  void ShutdownAutomaticRebootManager();

  void InitializeChromeUserManager();
  void DestroyChromeUserManager();

  void InitializeDeviceDisablingManager();
  void ShutdownDeviceDisablingManager();

  void InitializeSessionManager();
  void ShutdownSessionManager();

  void InitializeCrosComponentManager();
  void ShutdownCrosComponentManager();

  void InitializeSchedulerConfigurationManager();
  void ShutdownSchedulerConfigurationManager();

  // Initializes all services that need the primary profile. Gets called as soon
  // as the primary profile is available, which implies that the primary user
  // has logged in. The services are shut down automatically when the primary
  // profile is destroyed.
  // Use this for simple 'leaf-type' services with no or negligible inter-
  // dependencies. If your service has more complex dependencies, consider using
  // a BrowserContextKeyedService and restricting service creation to the
  // primary profile.
  void InitializePrimaryProfileServices(Profile* primary_profile);

  // Used to register a KeepAlive when Ash is initialized, and release it
  // when until Chrome starts exiting. Ensure we stay running the whole time.
  void RegisterKeepAlive();
  void UnregisterKeepAlive();

  // Returns the ProfileHelper instance that is used to identify
  // users and their profiles in Chrome OS multi user session.
  ash::ProfileHelper* profile_helper();

  ash::system::AutomaticRebootManager* automatic_reboot_manager() {
    return automatic_reboot_manager_.get();
  }

  policy::BrowserPolicyConnectorAsh* browser_policy_connector_ash();

  ash::ChromeSessionManager* session_manager() {
    return session_manager_.get();
  }

  ash::ChromeUserManager* user_manager() { return chrome_user_manager_.get(); }

  ash::SchedulerConfigurationManager* scheduler_configuration_manager() {
    return scheduler_configuration_manager_.get();
  }

  ash::system::DeviceDisablingManager* device_disabling_manager() {
    return device_disabling_manager_.get();
  }

  scoped_refptr<component_updater::CrOSComponentManager>
  cros_component_manager() {
    return cros_component_manager_;
  }

  ash::system::TimeZoneResolverManager* GetTimezoneResolverManager();

  ash::TimeZoneResolver* GetTimezoneResolver();

  // Overridden from BrowserProcessPlatformPartBase:
  void StartTearDown() override;
  void AttemptExit(bool try_to_quit_application) override;

  ash::system::SystemClock* GetSystemClock();
  void DestroySystemClock();

  ash::AccountManagerFactory* GetAccountManagerFactory();

  ash::InSessionPasswordChangeManager* in_session_password_change_manager() {
    return in_session_password_change_manager_.get();
  }

  static void EnsureFactoryBuilt();

 protected:
  // BrowserProcessPlatformPartChromeOS:
  bool CanRestoreUrlsForProfile(const Profile* profile) const override;

 private:
  friend class BrowserProcessPlatformPartTestApi;

  void CreateProfileHelper();

  void ShutdownPrimaryProfileServices();

  std::unique_ptr<ash::ChromeSessionManager> session_manager_;

  bool created_profile_helper_;
  std::unique_ptr<ash::ProfileHelper> profile_helper_;

  std::unique_ptr<ash::system::AutomaticRebootManager>
      automatic_reboot_manager_;

  std::unique_ptr<ash::ChromeUserManager> chrome_user_manager_;

  std::unique_ptr<ash::system::DeviceDisablingManagerDefaultDelegate>
      device_disabling_manager_delegate_;
  std::unique_ptr<ash::system::DeviceDisablingManager>
      device_disabling_manager_;

  std::unique_ptr<ash::system::TimeZoneResolverManager>
      timezone_resolver_manager_;
  std::unique_ptr<ash::TimeZoneResolver> timezone_resolver_;

  std::unique_ptr<ash::system::SystemClock> system_clock_;

  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  // Whether cros_component_manager_ has been initialized for test. Set by
  // BrowserProcessPlatformPartTestApi.
  bool using_testing_cros_component_manager_ = false;
  scoped_refptr<component_updater::CrOSComponentManager>
      cros_component_manager_;

  std::unique_ptr<ash::AccountManagerFactory> account_manager_factory_;

  std::unique_ptr<ash::InSessionPasswordChangeManager>
      in_session_password_change_manager_;

  base::CallbackListSubscription primary_profile_shutdown_subscription_;

  std::unique_ptr<ash::SchedulerConfigurationManager>
      scheduler_configuration_manager_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_ASH_H_
