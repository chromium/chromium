// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_CHROME_APP_MANAGER_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_CHROME_APP_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/extensions/external_cache.h"
#include "chrome/browser/ash/extensions/external_cache_delegate.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_installer.h"
#include "chromeos/crosapi/mojom/chrome_app_kiosk_service.mojom.h"
#include "components/account_id/account_id.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/common/extension_id.h"
#include "net/base/backoff_entry.h"

class GURL;
class PrefRegistrySimple;
class Profile;

namespace base {
class CommandLine;
}

namespace extensions {
class Extension;
}

namespace ash {

class KioskAppData;
class KioskExternalUpdater;

extern const char kKioskPrimaryAppInstallErrorHistogram[];
extern const char kKioskPrimaryAppUpdateResultHistogram[];
extern const char kKioskExternalUpdateSuccessHistogram[];

// KioskChromeAppManager manages cached app data.
class KioskChromeAppManager : public KioskAppManagerBase,
                              public chromeos::ExternalCacheDelegate {
 public:
  // Result of downloading primary app from ExternalCache. Should be in sync
  // with extensions::ExtensionDownloaderDelegate::Error. Used in UMA metrics.
  enum class PrimaryAppDownloadResult {
    // Successful update.
    kSuccess,
    // Background networking is disabled.
    kDisabled,
    // Failed to fetch the manifest for this extension.
    kManifestFetchFailed,
    // The manifest couldn't be parsed.
    kManifestInvalid,
    // The manifest was fetched and parsed, and there are no updates for
    // this extension.
    kNoUpdateAvailable,
    // The update entry for the extension contained no fetch URL.
    kCrxFetchUrlEmpty,
    // The update entry for the extension contained invalid fetch URL.
    kCrxFetchUrlInvalid,
    // There was an update for this extension but the download of the crx
    // failed.
    kCrxFetchFailed,
    kMaxValue = kCrxFetchFailed,
  };

  typedef std::vector<App> Apps;

  // Interface that can be used to override default KioskChromeAppManager
  // behavior. For example, it can be used in tests to inject test components
  // implementations.
  class Overrides {
   public:
    virtual ~Overrides() = default;

    // Creates the external cache that should be used by the
    // KioskChromeAppManager. It should always return a valid object.
    virtual std::unique_ptr<chromeos::ExternalCache> CreateExternalCache(
        chromeos::ExternalCacheDelegate* delegate,
        bool always_check_updates) = 0;
  };

  // Name of a dictionary that holds kiosk app info in Local State.
  // Sample layout:
  //   "kiosk": {
  //     "auto_login_enabled": true  //
  //   }
  static const char kKioskDictionaryName[];
  static const char kKeyAutoLoginState[];

  // Returns the manager instance. Crashes if it is not yet initialized.
  static KioskChromeAppManager* Get();

  static bool IsInitialized();

  // Initializes KioskChromeAppManager for testing. An `overrides` can be given
  // to customize behavior in tests, or `null` to use the default behavior.
  static void InitializeForTesting(Overrides* overrides);

  // Registers kiosk app entries in local state.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Registers kiosk app prefs that will be attached to a user profile. It would
  // be applied to Kiosk, because a Kiosk session has a special user profile.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  KioskChromeAppManager();
  KioskChromeAppManager(const KioskChromeAppManager&) = delete;
  KioskChromeAppManager& operator=(const KioskChromeAppManager&) = delete;
  ~KioskChromeAppManager() override;

  // Returns auto launcher app id or an empty string if there is none.
  std::string GetAutoLaunchApp() const;

  // Returns the cached required platform version of the auto launch with
  // zero delay kiosk app.
  std::string GetAutoLaunchAppRequiredPlatformVersion() const;

  // `KioskAppManagerBase` implementation:
  // Gets info of all apps that have no meta data load error.
  std::vector<App> GetApps() const override;

  // Gets app data for the given app id. Returns true if `app_id` is known and
  // `app` is populated. Otherwise, return false.
  bool GetApp(const std::string& app_id, App* app) const;

  // Clears locally cached Gdata.
  void ClearAppData(const std::string& app_id);

  // Updates app data from the `app` in `profile`. `app` is provided to cover
  // the case of app update case where `app` is the new version and is not
  // finished installing (e.g. because old version is still running). Otherwise,
  // `app` could be NULL and the current installed app in `profile` will be
  // used.
  void UpdateAppDataFromProfile(const std::string& app_id,
                                Profile* profile,
                                const extensions::Extension* app);

  void RetryFailedAppDataFetch();

  // Returns true if the app is found in cache.
  bool HasCachedCrx(const std::string& app_id) const;

  // Gets the path and version of the cached crx with `app_id`.
  // Returns true if the app is found in cache.
  bool GetCachedCrx(const std::string& app_id,
                    base::FilePath* file_path,
                    std::string* version) const;

  crosapi::mojom::AppInstallParams CreatePrimaryAppInstallData(
      const std::string& id) const;

  void UpdateExternalCache();

  // Monitors kiosk external update from usb stick.
  void MonitorKioskExternalUpdate();

  // Notify this manager that a Kiosk session started with the given `app_id`.
  void OnKioskSessionStarted(const KioskAppId& app_id);

  // Invoked when kiosk app cache has been updated.
  void OnKioskAppCacheUpdated(const std::string& app_id);

  // Invoked when kiosk app updating from usb stick has been completed.
  // `success` indicates if all the updates are completed successfully.
  void OnKioskAppExternalUpdateComplete(bool success);

  // Installs the validated external extension into cache.
  void PutValidatedExternalExtension(
      const std::string& app_id,
      const base::FilePath& crx_path,
      const std::string& version,
      chromeos::ExternalCache::PutExternalExtensionCallback callback);

  // Whether the current platform is compliant with the given required
  // platform version.
  bool IsPlatformCompliant(const std::string& required_platform_version) const;

  // Whether the platform is compliant for the given app.
  bool IsPlatformCompliantWithApp(const extensions::Extension* app) const;

  // Notifies the KioskChromeAppManager that a given app was auto-launched
  // automatically with no delay on startup. Certain privacy-sensitive
  // kiosk-mode behavior (such as network reporting) is only enabled for
  // kiosk apps that are immediately auto-launched on startup.
  void SetAppWasAutoLaunchedWithZeroDelay(const std::string& app_id);

  // Sets retry backoff policy of extension downloader. Set `std::nullopt` to
  // restore to the default. Used to reduce backoff while Kiosk is launching.
  void SetExtensionDownloaderBackoffPolicy(
      std::optional<net::BackoffEntry::Policy> backoff_policy);

  // Adds an app with the given meta data directly and skips meta data fetching
  // for test.
  void AddAppForTest(const std::string& app_id,
                     const AccountId& account_id,
                     const GURL& update_url,
                     const std::string& required_platform_version);

 private:
  friend class GlobalManager;
  friend class ChromeAppKioskAppManagerTest;
  friend class KioskAutoLaunchViewsTest;
  friend class KioskBaseTest;

  // Gets KioskAppData for the given app id.
  const KioskAppData* GetAppData(const std::string& app_id) const;
  KioskAppData* GetAppDataMutable(const std::string& app_id);

  // KioskAppManagerBase:
  // Updates app data `apps_` based on CrosSettings.
  void UpdateAppsFromPolicy() override;

  // Updates the prefs of `external_cache_` from `apps_`.
  void UpdateExternalCachePrefs();

  // chromeos::ExternalCacheDelegate:
  void OnExtensionLoadedInCache(const extensions::ExtensionId& id,
                                bool is_updated) override;
  void OnExtensionDownloadFailed(
      const extensions::ExtensionId& id,
      extensions::ExtensionDownloaderDelegate::Error error) override;

  // Returns the auto launch delay.
  base::TimeDelta GetAutoLaunchDelay() const;

  // Gets list of user switches that should be passed to Chrome in case current
  // session has to be restored, e.g. in case of a crash. The switches will be
  // returned as `switches` command line arguments.
  // Returns whether the set of switches would have to be changed in respect to
  // the current set of switches - if that is not the case `switches` might not
  // get populated.
  bool GetSwitchesForSessionRestore(const std::string& app_id,
                                    base::CommandLine* switches);

  // KioskAppDataDelegate:
  void OnExternalCacheDamaged(const std::string& app_id) override;

  // Converts kiosk app data from internal representation KioskAppData to
  // App.
  App ConstructApp(const KioskAppData& data) const;

  std::vector<std::unique_ptr<KioskAppData>> apps_;
  std::string auto_launch_app_id_;
  std::string currently_auto_launched_with_zero_delay_app_;

  std::unique_ptr<chromeos::ExternalCache> external_cache_;

  std::unique_ptr<KioskExternalUpdater> usb_stick_updater_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_CHROME_APP_MANAGER_H_
