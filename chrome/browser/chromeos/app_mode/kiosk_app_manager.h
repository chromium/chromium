// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/chromeos/extensions/external_cache.h"
#include "chrome/browser/chromeos/extensions/external_cache_delegate.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/account_id/account_id.h"
#include "extensions/common/extension_id.h"

class GURL;
class PrefRegistrySimple;
class Profile;

namespace base {
class CommandLine;
}

namespace extensions {
class Extension;
}

namespace chromeos {

class ExternalCache;
class KioskAppData;
class KioskExternalUpdater;
class OwnerSettingsServiceChromeOS;

// KioskAppManager manages cached app data.
class KioskAppManager : public KioskAppManagerBase,
                        public ExternalCacheDelegate {
 public:
  enum ConsumerKioskAutoLaunchStatus {
    // Consumer kiosk mode auto-launch feature can be enabled on this machine.
    CONSUMER_KIOSK_AUTO_LAUNCH_CONFIGURABLE,
    // Consumer kiosk auto-launch feature is enabled on this machine.
    CONSUMER_KIOSK_AUTO_LAUNCH_ENABLED,
    // Consumer kiosk mode auto-launch feature is disabled and cannot any longer
    // be enabled on this machine.
    CONSUMER_KIOSK_AUTO_LAUNCH_DISABLED,
  };

  using EnableKioskAutoLaunchCallback = base::OnceCallback<void(bool success)>;
  using GetConsumerKioskAutoLaunchStatusCallback =
      base::OnceCallback<void(ConsumerKioskAutoLaunchStatus status)>;

  typedef std::vector<App> Apps;

  // Interface that can be used to override default KioskAppManager behavior.
  // For example, it can be used in tests to inject test components
  // implementations.
  class Overrides {
   public:
    virtual ~Overrides() = default;

    // Creates the external cache that should be used by the
    // KioskAppManager. It should always return a valid object.
    virtual std::unique_ptr<ExternalCache> CreateExternalCache(
        ExternalCacheDelegate* delegate,
        bool always_check_updates) = 0;

    // Creates an AppSession object that will mantain a started kiosk app
    // session.
    // Called when the KioskAppManager initializes the session.
    // It can return nullptr.
    virtual std::unique_ptr<AppSession> CreateAppSession() = 0;
  };

  // Name of a dictionary that holds kiosk app info in Local State.
  // Sample layout:
  //   "kiosk": {
  //     "auto_login_enabled": true  //
  //   }
  static const char kKioskDictionaryName[];
  static const char kKeyAutoLoginState[];

  // Gets the KioskAppManager instance, which is lazily created on first call.
  static KioskAppManager* Get();

  // Initializes KioskAppManager for testing, injecting the provided overrides.
  // |overrides| can be null, in which case KioskAppManager will use default
  // behavior.
  // Must be called before Get().
  static void InitializeForTesting(Overrides* overrides);

  // Prepares for shutdown and calls CleanUp() if needed.
  static void Shutdown();

  // Registers kiosk app entries in local state.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  static bool IsConsumerKioskEnabled();

  // Initiates reading of consumer kiosk mode auto-launch status.
  void GetConsumerKioskAutoLaunchStatus(
      GetConsumerKioskAutoLaunchStatusCallback callback);

  // Enables consumer kiosk mode app auto-launch feature. Upon completion,
  // |callback| will be invoked with outcome of this operation.
  void EnableConsumerKioskAutoLaunch(EnableKioskAutoLaunchCallback callback);

  // Returns true if this device is consumer kiosk auto launch enabled.
  bool IsConsumerKioskDeviceWithAutoLaunch();

  // Returns auto launcher app id or an empty string if there is none.
  std::string GetAutoLaunchApp() const;

  // Sets |app_id| as the app to auto launch at start up.
  void SetAutoLaunchApp(const std::string& app_id,
                        OwnerSettingsServiceChromeOS* service);

  // Returns true if there is a pending auto-launch request.
  bool IsAutoLaunchRequested() const;

  // Returns true if owner/policy enabled auto launch.
  bool IsAutoLaunchEnabled() const;

  // Enable auto launch setter.
  void SetEnableAutoLaunch(bool value);

  // Returns the cached required platform version of the auto launch with
  // zero delay kiosk app.
  std::string GetAutoLaunchAppRequiredPlatformVersion() const;

  // Adds/removes a kiosk app by id. When removed, all locally cached data
  // will be removed as well.
  void AddApp(const std::string& app_id, OwnerSettingsServiceChromeOS* service);
  void RemoveApp(const std::string& app_id,
                 OwnerSettingsServiceChromeOS* service);

  // KioskAppManagerBase:
  // Gets info of all apps that have no meta data load error.
  void GetApps(Apps* apps) const override;

  // Gets app data for the given app id. Returns true if |app_id| is known and
  // |app| is populated. Otherwise, return false.
  bool GetApp(const std::string& app_id, App* app) const;

  // Clears locally cached Gdata.
  void ClearAppData(const std::string& app_id);

  // Updates app data from the |app| in |profile|. |app| is provided to cover
  // the case of app update case where |app| is the new version and is not
  // finished installing (e.g. because old version is still running). Otherwise,
  // |app| could be NULL and the current installed app in |profile| will be
  // used.
  void UpdateAppDataFromProfile(const std::string& app_id,
                                Profile* profile,
                                const extensions::Extension* app);

  void RetryFailedAppDataFetch();

  // Returns true if the app is found in cache.
  bool HasCachedCrx(const std::string& app_id) const;

  // Gets the path and version of the cached crx with |app_id|.
  // Returns true if the app is found in cache.
  bool GetCachedCrx(const std::string& app_id,
                    base::FilePath* file_path,
                    std::string* version) const;

  // Initialized or updates the app whose prefs are available to primary kiosk
  // app external extensions loader.
  void UpdatePrimaryAppLoaderPrefs(const std::string& id);

  // Returns the primary app prefs that can be used by external extensions
  // loader - this will return null until |UpdatePrimaryAppLoaderPrefs| is
  // called.
  std::unique_ptr<base::DictionaryValue> GetPrimaryAppLoaderPrefs();

  // Registers a callback called whenever the available primary app external
  // extension prefs get updated (i.e. when UpdatePrimaryAppLoaderPrefs() is
  // called).
  void SetPrimaryAppLoaderPrefsChangedHandler(base::RepeatingClosure handler);

  // Initialized or updates the apps whose prefs are available to secondary
  // kiosk apps external extensions loader.
  void UpdateSecondaryAppsLoaderPrefs(const std::vector<std::string>& ids);

  // Returns the secondary apps prefs that can be used by external extensions
  // loader - this will return null until |UpdateSecondaryAppsLoaderPrefs| is
  // called.
  std::unique_ptr<base::DictionaryValue> GetSecondaryAppsLoaderPrefs();

  // Registers a callback called whenever the available secondary apps external
  // extension prefs get updated (i.e. when UpdateSecondayAppsLoaderPrefs() is
  // called).
  void SetSecondaryAppsLoaderPrefsChangedHandler(
      base::RepeatingClosure handler);

  void UpdateExternalCache();

  // Monitors kiosk external update from usb stick.
  void MonitorKioskExternalUpdate();

  // Invoked when kiosk app cache has been updated.
  void OnKioskAppCacheUpdated(const std::string& app_id);

  // Invoked when kiosk app updating from usb stick has been completed.
  // |success| indicates if all the updates are completed successfully.
  void OnKioskAppExternalUpdateComplete(bool success);

  // Installs the validated external extension into cache.
  void PutValidatedExternalExtension(
      const std::string& app_id,
      const base::FilePath& crx_path,
      const std::string& version,
      ExternalCache::PutExternalExtensionCallback callback);

  // Whether the current platform is compliant with the given required
  // platform version.
  bool IsPlatformCompliant(const std::string& required_platform_version) const;

  // Whether the platform is compliant for the given app.
  bool IsPlatformCompliantWithApp(const extensions::Extension* app) const;

  // Notifies the KioskAppManager that a given app was auto-launched
  // automatically with no delay on startup. Certain privacy-sensitive
  // kiosk-mode behavior (such as network reporting) is only enabled for
  // kiosk apps that are immediately auto-launched on startup.
  void SetAppWasAutoLaunchedWithZeroDelay(const std::string& app_id);

  // Initialize |app_session_|.
  void InitSession(Profile* profile, const std::string& app_id);

  // Adds an app with the given meta data directly and skips meta data fetching
  // for test.
  void AddAppForTest(const std::string& app_id,
                     const AccountId& account_id,
                     const GURL& update_url,
                     const std::string& required_platform_version);

 private:
  friend struct base::LazyInstanceTraitsBase<KioskAppManager>;
  friend std::default_delete<KioskAppManager>;
  friend class KioskAppManagerTest;
  friend class KioskAutoLaunchViewsTest;
  friend class KioskTest;
  friend class KioskUpdateTest;

  enum AutoLoginState {
    AUTOLOGIN_NONE      = 0,
    AUTOLOGIN_REQUESTED = 1,
    AUTOLOGIN_APPROVED  = 2,
    AUTOLOGIN_REJECTED  = 3,
  };

  KioskAppManager();
  ~KioskAppManager() override;

  // Stop all data loading and remove its dependency on CrosSettings.
  void CleanUp();

  // Gets KioskAppData for the given app id.
  const KioskAppData* GetAppData(const std::string& app_id) const;
  KioskAppData* GetAppDataMutable(const std::string& app_id);

  // KioskAppManagerBase:
  // Updates app data |apps_| based on CrosSettings.
  void UpdateAppsFromPolicy() override;

  // Updates the prefs of |external_cache_| from |apps_|.
  void UpdateExternalCachePrefs();

  // ExternalCacheDelegate:
  void OnExtensionLoadedInCache(const extensions::ExtensionId& id) override;
  void OnExtensionDownloadFailed(const extensions::ExtensionId& id) override;

  // Callback for InstallAttributes::LockDevice() during
  // EnableConsumerModeKiosk() call.
  void OnLockDevice(EnableKioskAutoLaunchCallback callback,
                    InstallAttributes::LockResult result);

  // Callback for InstallAttributes::ReadImmutableAttributes() during
  // GetConsumerKioskModeStatus() call.
  void OnReadImmutableAttributes(
      GetConsumerKioskAutoLaunchStatusCallback callback);

  // Callback for reading handling checks of the owner public.
  void OnOwnerFileChecked(GetConsumerKioskAutoLaunchStatusCallback callback,
                          bool* owner_present);

  // Reads/writes auto login state from/to local state.
  AutoLoginState GetAutoLoginState() const;
  void SetAutoLoginState(AutoLoginState state);

  // Returns the auto launch delay.
  base::TimeDelta GetAutoLaunchDelay() const;

  // Gets list of user switches that should be passed to Chrome in case current
  // session has to be restored, e.g. in case of a crash. The switches will be
  // returned as |switches| command line arguments.
  // Returns whether the set of switches would have to be changed in respect to
  // the current set of switches - if that is not the case |switches| might not
  // get populated.
  bool GetSwitchesForSessionRestore(const std::string& app_id,
                                    base::CommandLine* switches);

  // KioskAppDataDelegate:
  void OnExternalCacheDamaged(const std::string& app_id) override;

  // Converts kiosk app data from internal representation KioskAppData to
  // App.
  App ConstructApp(const KioskAppData& data) const;

  // True if machine ownership is already established.
  bool ownership_established_ = false;
  std::vector<std::unique_ptr<KioskAppData>> apps_;
  std::string auto_launch_app_id_;
  std::string currently_auto_launched_with_zero_delay_app_;

  std::unique_ptr<ExternalCache> external_cache_;

  std::unique_ptr<KioskExternalUpdater> usb_stick_updater_;

  // Last app id set by UpdatePrimaryAppLoaderPrefs().
  base::Optional<std::string> primary_app_id_;

  // Callback registered using SetPrimaryAppLoaderPrefsChangedHandler().
  base::RepeatingClosure primary_app_changed_handler_;

  // Extensions id set by UpdateSecondatyAppsLoaderPrefs().
  base::Optional<std::vector<std::string>> secondary_app_ids_;

  // Callback registered using SetSecondaryAppsLoaderPrefsChangedHandler().
  base::RepeatingClosure secondary_apps_changed_handler_;

  DISALLOW_COPY_AND_ASSIGN(KioskAppManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_MANAGER_H_
