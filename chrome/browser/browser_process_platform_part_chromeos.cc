// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_platform_part_chromeos.h"

#include <memory>
#include <utility>

#include "ash/components/account_manager/account_manager_factory.h"
#include "ash/components/arc/enterprise/arc_data_snapshotd_manager.h"
#include "ash/components/arc/enterprise/snapshot_hours_policy_service.h"
#include "ash/components/geolocation/simple_geolocation_provider.h"
#include "ash/components/timezone/timezone_resolver.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/memory/singleton.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/login/saml/in_session_password_change_manager.h"
#include "chrome/browser/ash/login/session/chrome_session_manager.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/ash/net/delay_network_call.h"
#include "chrome/browser/ash/net/system_proxy_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/scheduler_configuration_manager.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/system/automatic_reboot_manager.h"
#include "chrome/browser/ash/system/device_disabling_manager.h"
#include "chrome/browser/ash/system/device_disabling_manager_default_delegate.h"
#include "chrome/browser/ash/system/system_clock.h"
#include "chrome/browser/ash/system/timezone_resolver_manager.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/metadata_table_chromeos.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sessions/core/session_types.h"
#include "components/user_manager/user_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/preferences/public/mojom/preferences.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/cpp/service.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

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

BrowserProcessPlatformPart::BrowserRestoreObserver::BrowserRestoreObserver() {
  BrowserList::AddObserver(this);
}

BrowserProcessPlatformPart::BrowserRestoreObserver::~BrowserRestoreObserver() {
  BrowserList::RemoveObserver(this);
}

void BrowserProcessPlatformPart::BrowserRestoreObserver::OnBrowserAdded(
    Browser* browser) {
  // If |browser| is the only browser, restores urls based on the on startup
  // setting.
  if (BrowserList::GetInstance()->size() == 1 && ShouldRestoreUrls(browser)) {
    if (ShouldOpenUrlsInNewBrowser(browser)) {
      // Delay creating a new browser until |browser| is activated.
      on_session_restored_callback_subscription_ =
          SessionRestore::RegisterOnSessionRestoredCallback(base::BindRepeating(
              &BrowserProcessPlatformPart::BrowserRestoreObserver::
                  OnSessionRestoreDone,
              base::Unretained(this)));
    } else {
      RestoreUrls(browser);
    }
  }

  // If the startup urls from LAST_AND_URLS pref are already opened in a new
  // browser, skip opening the same browser.
  if (browser->creation_source() ==
      Browser::CreationSource::kLastAndUrlsStartupPref) {
    DCHECK(on_session_restored_callback_subscription_);
    on_session_restored_callback_subscription_ = {};
  }
}

void BrowserProcessPlatformPart::BrowserRestoreObserver::OnSessionRestoreDone(
    Profile* profile,
    int num_tabs_restored) {
  // Ensure this callback to be called exactly once.
  on_session_restored_callback_subscription_ = {};

  // All browser windows are created. Open startup urls in a new browser.
  auto create_params = Browser::CreateParams(profile, /*user_gesture*/ false);
  Browser* browser = Browser::Create(create_params);
  RestoreUrls(browser);
  browser->window()->Show();
  browser->window()->Activate();
}

bool BrowserProcessPlatformPart::BrowserRestoreObserver::ShouldRestoreUrls(
    Browser* browser) const {
  Profile* profile = browser->profile();

  // Only open urls for regular sign in users.
  DCHECK(profile);
  if (profile->IsSystemProfile() ||
      !ash::ProfileHelper::IsRegularProfile(profile) ||
      ash::ProfileHelper::IsEphemeralUserProfile(profile)) {
    return false;
  }

  // If during the restore process, or restore from a crash, don't launch urls.
  // However, in case of LAST_AND_URLS startup setting, urls should be opened
  // even when the restore session is in progress.
  SessionStartupPref pref =
      SessionStartupPref::GetStartupPref(browser->profile()->GetPrefs());
  if ((SessionRestore::IsRestoring(profile) &&
       pref.type != SessionStartupPref::LAST_AND_URLS) ||
      HasPendingUncleanExit(profile)) {
    return false;
  }

  // App windows should not be restored.
  auto window_type = WindowTypeForBrowserType(browser->type());
  if (window_type == sessions::SessionWindow::TYPE_APP ||
      window_type == sessions::SessionWindow::TYPE_APP_POPUP) {
    return false;
  }

  // If the browser is created by StartupBrowserCreator,
  // StartupBrowserCreatorImpl::OpenTabsInBrowser can open tabs, so don't
  // restore urls here.
  if (browser->creation_source() == Browser::CreationSource::kStartupCreator)
    return false;

  // If the startup setting is not open urls, don't launch urls.
  if (!pref.ShouldOpenUrls() || pref.urls.empty())
    return false;

  return true;
}

// If the startup setting is both the restore last session and the open urls,
// those should be opened in a new browser.
bool BrowserProcessPlatformPart::BrowserRestoreObserver::
    ShouldOpenUrlsInNewBrowser(Browser* browser) const {
  SessionStartupPref pref =
      SessionStartupPref::GetStartupPref(browser->profile()->GetPrefs());
  return pref.type == SessionStartupPref::LAST_AND_URLS;
}

void BrowserProcessPlatformPart::BrowserRestoreObserver::RestoreUrls(
    Browser* browser) {
  DCHECK(browser);

  SessionStartupPref pref =
      SessionStartupPref::GetStartupPref(browser->profile()->GetPrefs());
  std::vector<GURL> urls;
  for (const auto& url : pref.urls)
    urls.push_back(url);

  custom_handlers::ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(browser->profile());
  for (const GURL& url : urls) {
    // We skip URLs that we'd have to launch an external protocol handler for.
    // This avoids us getting into an infinite loop asking ourselves to open
    // a URL, should the handler be (incorrectly) configured to be us. Anyone
    // asking us to open such a URL should really ask the handler directly.
    bool handled_by_chrome =
        ProfileIOData::IsHandledURL(url) ||
        (registry && registry->IsHandledProtocol(url.scheme()));
    if (!handled_by_chrome)
      continue;

    int add_types = TabStripModel::ADD_NONE | TabStripModel::ADD_FORCE_INDEX;
    NavigateParams params(browser, url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
    params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
    params.tabstrip_add_types = add_types;
    Navigate(&params);
  }
}

BrowserProcessPlatformPart::BrowserProcessPlatformPart()
    : created_profile_helper_(false),
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
  DCHECK(!session_manager_);
  session_manager_ = std::make_unique<ash::ChromeSessionManager>();
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
      std::make_unique<ash::SchedulerConfigurationManager>(
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

  if (ash::SystemProxyManager::Get())
    ash::SystemProxyManager::Get()->StopObservingPrimaryProfilePrefs();
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

chromeos::system::TimeZoneResolverManager*
BrowserProcessPlatformPart::GetTimezoneResolverManager() {
  if (!timezone_resolver_manager_.get()) {
    timezone_resolver_manager_ =
        std::make_unique<chromeos::system::TimeZoneResolverManager>();
  }
  return timezone_resolver_manager_.get();
}

ash::TimeZoneResolver* BrowserProcessPlatformPart::GetTimezoneResolver() {
  if (!timezone_resolver_.get()) {
    timezone_resolver_ = std::make_unique<ash::TimeZoneResolver>(
        GetTimezoneResolverManager(),
        g_browser_process->shared_url_loader_factory(),
        ash::SimpleGeolocationProvider::DefaultGeolocationProviderURL(),
        base::BindRepeating(&ash::system::ApplyTimeZone),
        base::BindRepeating(
            &ash::DelayNetworkCall,
            base::Milliseconds(ash::kDefaultNetworkRetryDelayMS)),
        g_browser_process->local_state());
  }
  return timezone_resolver_.get();
}

void BrowserProcessPlatformPart::StartTearDown() {
  // interactive_ui_tests check for memory leaks before this object is
  // destroyed.  So we need to destroy |timezone_resolver_| here.
  timezone_resolver_.reset();
  profile_helper_.reset();
}

void BrowserProcessPlatformPart::AttemptExit(bool try_to_quit_application) {
  // Request Lacros terminate early during shutdown to give it the opportunity
  // to shutdown gracefully. Check to make sure `browser_manager` is available
  // as it may be null in tests.
  if (auto* browser_manager = crosapi::BrowserManager::Get())
    browser_manager->Shutdown();

  BrowserProcessPlatformPartBase::AttemptExit(try_to_quit_application);
}

chromeos::system::SystemClock* BrowserProcessPlatformPart::GetSystemClock() {
  if (!system_clock_.get())
    system_clock_ = std::make_unique<chromeos::system::SystemClock>();
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
