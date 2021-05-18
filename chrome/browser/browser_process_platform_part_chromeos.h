// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_CHROMEOS_H_
#define CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_CHROMEOS_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "chrome/browser/browser_process_platform_part_base.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"

class Browser;
class BrowserProcessPlatformPartTestApi;
class Profile;

namespace ash {
class AccountManagerFactory;
class ChromeSessionManager;
class ChromeUserManager;
class ProfileHelper;

namespace system {
class AutomaticRebootManager;
class DeviceDisablingManager;
class DeviceDisablingManagerDefaultDelegate;
class TimeZoneResolverManager;
class SystemClock;
}  // namespace system
}  // namespace ash

namespace chromeos {
class InSessionPasswordChangeManager;
class KernelFeatureManager;
class SchedulerConfigurationManager;
class TimeZoneResolver;
}  // namespace chromeos

namespace policy {
class BrowserPolicyConnectorChromeOS;
}

class ScopedKeepAlive;

class BrowserProcessPlatformPart : public BrowserProcessPlatformPartBase {
 public:
  BrowserProcessPlatformPart();
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

  void InitializeKernelFeatureManager();
  void ShutdownKernelFeatureManager();

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

  policy::BrowserPolicyConnectorChromeOS* browser_policy_connector_chromeos();

  ash::ChromeSessionManager* session_manager() {
    return session_manager_.get();
  }

  ash::ChromeUserManager* user_manager() { return chrome_user_manager_.get(); }

  chromeos::SchedulerConfigurationManager* scheduler_configuration_manager() {
    return scheduler_configuration_manager_.get();
  }

  chromeos::KernelFeatureManager* kernel_feature_manager() {
    return kernel_feature_manager_.get();
  }

  ash::system::DeviceDisablingManager* device_disabling_manager() {
    return device_disabling_manager_.get();
  }

  scoped_refptr<component_updater::CrOSComponentManager>
  cros_component_manager() {
    return cros_component_manager_;
  }

  ash::system::TimeZoneResolverManager* GetTimezoneResolverManager();

  chromeos::TimeZoneResolver* GetTimezoneResolver();

  // Overridden from BrowserProcessPlatformPartBase:
  void StartTearDown() override;

  ash::system::SystemClock* GetSystemClock();
  void DestroySystemClock();

  ash::AccountManagerFactory* GetAccountManagerFactory();

  chromeos::InSessionPasswordChangeManager*
  in_session_password_change_manager() {
    return in_session_password_change_manager_.get();
  }

 private:
  friend class BrowserProcessPlatformPartTestApi;

  // An observer that restores urls based on the on startup setting after a new
  // browser is added to the BrowserList.
  class BrowserRestoreObserver : public BrowserListObserver {
   public:
    BrowserRestoreObserver();

    ~BrowserRestoreObserver() override;

   protected:
    // BrowserListObserver:
    void OnBrowserAdded(Browser* browser) override;

   private:
    // Returns true, if the url defined in the on startup setting should be
    // opened. Otherwise, returns false.
    bool ShouldRestoreUrls(Browser* browser);

    // Restores urls based on the on startup setting.
    void RestoreUrls(Browser* browser);
  };

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
  std::unique_ptr<chromeos::TimeZoneResolver> timezone_resolver_;

  std::unique_ptr<ash::system::SystemClock> system_clock_;

  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  // Whether cros_component_manager_ has been initialized for test. Set by
  // BrowserProcessPlatformPartTestApi.
  bool using_testing_cros_component_manager_ = false;
  scoped_refptr<component_updater::CrOSComponentManager>
      cros_component_manager_;

  std::unique_ptr<ash::AccountManagerFactory> account_manager_factory_;

  std::unique_ptr<chromeos::InSessionPasswordChangeManager>
      in_session_password_change_manager_;

  base::CallbackListSubscription primary_profile_shutdown_subscription_;

  std::unique_ptr<chromeos::SchedulerConfigurationManager>
      scheduler_configuration_manager_;
  std::unique_ptr<chromeos::KernelFeatureManager> kernel_feature_manager_;

  BrowserRestoreObserver browser_restore_observer;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(BrowserProcessPlatformPart);
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_CHROMEOS_H_
