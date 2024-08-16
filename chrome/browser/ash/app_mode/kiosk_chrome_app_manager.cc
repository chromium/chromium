// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_paths.h"
#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data_base.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_cryptohome_remover.h"
#include "chrome/browser/ash/app_mode/kiosk_external_updater.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/extensions/external_cache.h"
#include "chrome/browser/ash/extensions/external_cache_impl.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_installer.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_external_loader_broker.h"
#include "chrome/browser/chromeos/app_mode/kiosk_browser_session.h"
#include "chrome/browser/extensions/external_loader.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/crosapi/mojom/chrome_app_kiosk_service.mojom.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"

namespace ash {

namespace {

// Sub directory under DIR_USER_DATA to store cached crx files.
constexpr char kCrxCacheDir[] = "kiosk/crx";

// Sub directory under DIR_USER_DATA to store unpacked crx file for validating
// its signature.
constexpr char kCrxUnpackDir[] = "kiosk_unpack";

KioskChromeAppManager::Overrides* g_test_overrides = nullptr;

base::FilePath GetCrxCacheDir() {
  base::FilePath user_data_dir;
  CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  return user_data_dir.AppendASCII(kCrxCacheDir);
}

base::FilePath GetCrxUnpackDir() {
  base::FilePath temp_dir;
  base::GetTempDir(&temp_dir);
  return temp_dir.AppendASCII(kCrxUnpackDir);
}

scoped_refptr<base::SequencedTaskRunner> GetBackgroundTaskRunner() {
  // TODO(eseckler): The ExternalCacheImpl that uses this TaskRunner seems to be
  // important during startup, which is why we cannot currently use the
  // BEST_EFFORT TaskPriority here.
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

std::unique_ptr<chromeos::ExternalCache> CreateExternalCache(
    chromeos::ExternalCacheDelegate* delegate) {
  if (g_test_overrides) {
    return g_test_overrides->CreateExternalCache(delegate, true);
  }

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory =
      g_browser_process->shared_url_loader_factory();
  auto cache = std::make_unique<chromeos::ExternalCacheImpl>(
      GetCrxCacheDir(), shared_url_loader_factory, GetBackgroundTaskRunner(),
      delegate, /*always_check_updates=*/true,
      /*wait_for_cache_initialization=*/false,
      /*allow_scheduled_updates=*/true);
  cache->set_flush_on_put(true);
  return cache;
}

base::Version GetPlatformVersion() {
  return base::Version(base::SysInfo::OperatingSystemVersion());
}

// Converts a flag constant to actual command line switch value.
std::string GetSwitchString(const std::string& flag_name) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  cmd_line.AppendSwitch(flag_name);
  DCHECK_EQ(2U, cmd_line.argv().size());
  return cmd_line.argv()[1];
}

bool IsWebstoreUpdateUrl(const std::string* url) {
  return url && extension_urls::IsWebstoreUpdateUrl(GURL(*url));
}

KioskChromeAppManager::PrimaryAppDownloadResult
PrimaryAppDownloadResultFromError(
    extensions::ExtensionDownloaderDelegate::Error error) {
  switch (error) {
    case extensions::ExtensionDownloaderDelegate::Error::DISABLED:
      return KioskChromeAppManager::PrimaryAppDownloadResult::kDisabled;
    case extensions::ExtensionDownloaderDelegate::Error::MANIFEST_FETCH_FAILED:
      return KioskChromeAppManager::PrimaryAppDownloadResult::
          kManifestFetchFailed;
    case extensions::ExtensionDownloaderDelegate::Error::MANIFEST_INVALID:
      return KioskChromeAppManager::PrimaryAppDownloadResult::kManifestInvalid;
    case extensions::ExtensionDownloaderDelegate::Error::NO_UPDATE_AVAILABLE:
      return KioskChromeAppManager::PrimaryAppDownloadResult::
          kNoUpdateAvailable;
    case extensions::ExtensionDownloaderDelegate::Error::CRX_FETCH_URL_EMPTY:
      return KioskChromeAppManager::PrimaryAppDownloadResult::kCrxFetchUrlEmpty;
    case extensions::ExtensionDownloaderDelegate::Error::CRX_FETCH_URL_INVALID:
      return KioskChromeAppManager::PrimaryAppDownloadResult::
          kCrxFetchUrlInvalid;
    case extensions::ExtensionDownloaderDelegate::Error::CRX_FETCH_FAILED:
      return KioskChromeAppManager::PrimaryAppDownloadResult::kCrxFetchFailed;
  }
}

}  // namespace

// static
const char KioskChromeAppManager::kKioskDictionaryName[] = "kiosk";

const char kKioskPrimaryAppInstallErrorHistogram[] =
    "Kiosk.ChromeApp.PrimaryAppInstallError";
const char kKioskPrimaryAppUpdateResultHistogram[] =
    "Kiosk.ChromeApp.PrimaryAppUpdateResult";
const char kKioskExternalUpdateSuccessHistogram[] =
    "Kiosk.ChromeApp.ExternalUpdateSuccess";

namespace {
// This class is owned by `ChromeBrowserMainPartsAsh`.
static KioskChromeAppManager* g_instance = nullptr;
}  // namespace

// static
KioskChromeAppManager* KioskChromeAppManager::Get() {
  CHECK(g_instance);
  return g_instance;
}

// static
bool KioskChromeAppManager::IsInitialized() {
  return g_instance;
}

// static
void KioskChromeAppManager::InitializeForTesting(Overrides* overrides) {
  DCHECK(!g_instance);
  g_test_overrides = overrides;
}

// static
void KioskChromeAppManager::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kKioskDictionaryName);
  chromeos::KioskBrowserSession::RegisterLocalStatePrefs(registry);
}

// static
void KioskChromeAppManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  chromeos::KioskBrowserSession::RegisterProfilePrefs(registry);
}

std::string KioskChromeAppManager::GetAutoLaunchApp() const {
  return auto_launch_app_id_;
}

void KioskChromeAppManager::SetAppWasAutoLaunchedWithZeroDelay(
    const std::string& app_id) {
  DCHECK_EQ(auto_launch_app_id_, app_id);
  currently_auto_launched_with_zero_delay_app_ = app_id;
  auto_launched_with_zero_delay_ = true;
}

void KioskChromeAppManager::SetExtensionDownloaderBackoffPolicy(
    std::optional<net::BackoffEntry::Policy> backoff_policy) {
  // In browser tests `external_cache_` is reset before `StartupAppLauncher`.
  // Check before trying to set backoff policy here.
  if (!external_cache_) {
    return;
  }
  external_cache_->SetBackoffPolicy(backoff_policy);
}

bool KioskChromeAppManager::GetSwitchesForSessionRestore(
    const std::string& app_id,
    base::CommandLine* switches) {
  bool auto_launched = app_id == currently_auto_launched_with_zero_delay_app_;
  const base::CommandLine* current_command_line =
      base::CommandLine::ForCurrentProcess();
  bool has_auto_launched_flag =
      current_command_line->HasSwitch(switches::kAppAutoLaunched);
  if (auto_launched == has_auto_launched_flag) {
    return false;
  }

  // Collect current policy defined switches, so they can be passed on to the
  // session manager as well - otherwise they would get lost on restart.
  // This ignores 'flag-switches-begin' - 'flag-switches-end' flags, but those
  // should not be present for kiosk sessions.
  bool in_policy_switches_block = false;
  const std::string policy_switches_begin =
      GetSwitchString(chromeos::switches::kPolicySwitchesBegin);
  const std::string policy_switches_end =
      GetSwitchString(chromeos::switches::kPolicySwitchesEnd);

  for (const auto& it : current_command_line->argv()) {
    if (it == policy_switches_begin) {
      DCHECK(!in_policy_switches_block);
      in_policy_switches_block = true;
    }

    if (in_policy_switches_block) {
      switches->AppendSwitch(it);
    }

    if (it == policy_switches_end) {
      DCHECK(in_policy_switches_block);
      in_policy_switches_block = false;
    }
  }

  DCHECK(!in_policy_switches_block);

  if (auto_launched) {
    switches->AppendSwitch(switches::kAppAutoLaunched);
  }

  return true;
}

void KioskChromeAppManager::OnExternalCacheDamaged(const std::string& app_id) {
  CHECK(external_cache_);
  base::FilePath crx_path;
  std::string version;
  GetCachedCrx(app_id, &crx_path, &version);
  external_cache_->OnDamagedFileDetected(crx_path);
}

void KioskChromeAppManager::AddAppForTest(
    const std::string& app_id,
    const AccountId& account_id,
    const GURL& update_url,
    const std::string& required_platform_version) {
  for (auto it = apps_.begin(); it != apps_.end(); ++it) {
    if ((*it)->app_id() == app_id) {
      apps_.erase(it);
      break;
    }
  }

  apps_.emplace_back(KioskAppData::CreateForTest(
      this, app_id, account_id, update_url, required_platform_version));
}

std::string KioskChromeAppManager::GetAutoLaunchAppRequiredPlatformVersion()
    const {
  // Bail out if there is no auto launched app with zero delay.
  if (auto_launch_app_id_.empty() || !GetAutoLaunchDelay().is_zero()) {
    return std::string();
  }

  const KioskAppData* data = GetAppData(auto_launch_app_id_);
  return data == nullptr ? std::string() : data->required_platform_version();
}

std::vector<KioskChromeAppManager::App> KioskChromeAppManager::GetApps() const {
  std::vector<App> apps;
  for (const auto& app : apps_) {
    if (app->status() != KioskAppData::Status::kError) {
      apps.push_back(ConstructApp(*app));
    }
  }
  return apps;
}

KioskChromeAppManager::App KioskChromeAppManager::ConstructApp(
    const KioskAppData& data) const {
  App app(data);
  app.required_platform_version = data.required_platform_version();
  app.is_loading = external_cache_->ExtensionFetchPending(app.app_id);
  app.was_auto_launched_with_zero_delay =
      app.app_id == currently_auto_launched_with_zero_delay_app_;
  return app;
}

bool KioskChromeAppManager::GetApp(const std::string& app_id, App* app) const {
  const KioskAppData* data = GetAppData(app_id);
  if (!data) {
    return false;
  }
  *app = ConstructApp(*data);
  return true;
}

void KioskChromeAppManager::ClearAppData(const std::string& app_id) {
  KioskAppData* app_data = GetAppDataMutable(app_id);
  if (!app_data) {
    return;
  }

  app_data->ClearCache();
}

void KioskChromeAppManager::UpdateAppDataFromProfile(
    const std::string& app_id,
    Profile* profile,
    const extensions::Extension* app) {
  KioskAppData* app_data = GetAppDataMutable(app_id);
  if (!app_data) {
    return;
  }

  app_data->LoadFromInstalledApp(profile, app);
}

void KioskChromeAppManager::RetryFailedAppDataFetch() {
  for (const auto& app : apps_) {
    if (app->status() == KioskAppData::Status::kError) {
      app->Load();
    }
  }
}

bool KioskChromeAppManager::HasCachedCrx(const std::string& app_id) const {
  base::FilePath crx_path;
  std::string version;
  return GetCachedCrx(app_id, &crx_path, &version);
}

bool KioskChromeAppManager::GetCachedCrx(const std::string& app_id,
                                         base::FilePath* file_path,
                                         std::string* version) const {
  return external_cache_->GetExtension(app_id, file_path, version);
}

crosapi::mojom::AppInstallParams
KioskChromeAppManager::CreatePrimaryAppInstallData(
    const std::string& id) const {
  const base::Value::Dict* extension =
      external_cache_->GetCachedExtensions().FindDict(id);
  if (!extension) {
    return crosapi::mojom::AppInstallParams(id, std::string(), std::string(),
                                            false);
  }

  const std::optional<bool> is_store_app_maybe =
      extension->FindBool(extensions::ExternalProviderImpl::kIsFromWebstore);
  const std::string* external_update_url_value = extension->FindString(
      extensions::ExternalProviderImpl::kExternalUpdateUrl);
  bool is_store_app_bool = is_store_app_maybe.value_or(false) ||
                           IsWebstoreUpdateUrl(external_update_url_value);

  const std::string* crx_file_location =
      extension->FindString(extensions::ExternalProviderImpl::kExternalCrx);
  DCHECK(crx_file_location);

  const std::string* external_version =
      extension->FindString(extensions::ExternalProviderImpl::kExternalVersion);
  DCHECK(external_version);

  return crosapi::mojom::AppInstallParams(id, *crx_file_location,
                                          *external_version, is_store_app_bool);
}

void KioskChromeAppManager::OnKioskSessionStarted(const KioskAppId& app_id) {
  base::CommandLine session_flags(base::CommandLine::NO_PROGRAM);
  if (GetSwitchesForSessionRestore(app_id.app_id.value(), &session_flags)) {
    base::CommandLine::StringVector flags;
    // argv[0] is the program name `base::CommandLine::NO_PROGRAM`.
    flags.assign(session_flags.argv().begin() + 1, session_flags.argv().end());

    // Update user flags, but do not restart Chrome - the purpose of the flags
    // set here is to be able to properly restore session if the session is
    // restarted - e.g. due to crash. For example, this will ensure restarted
    // app session restores auto-launched state.
    UserSessionManager::GetInstance()->SetSwitchesForUser(
        user_manager::UserManager::Get()->GetActiveUser()->GetAccountId(),
        UserSessionManager::CommandLineSwitchesType::kPolicyAndKioskControl,
        flags);
  }

  NotifySessionInitialized();
}

void KioskChromeAppManager::UpdateExternalCache() {
  UpdateAppsFromPolicy();
}

void KioskChromeAppManager::OnKioskAppCacheUpdated(const std::string& app_id) {
  for (auto& observer : observers_) {
    observer.OnKioskAppCacheUpdated(app_id);
  }
}

void KioskChromeAppManager::OnKioskAppExternalUpdateComplete(bool success) {
  base::UmaHistogramBoolean(kKioskExternalUpdateSuccessHistogram, success);
  for (auto& observer : observers_) {
    observer.OnKioskAppExternalUpdateComplete(success);
  }
}

void KioskChromeAppManager::PutValidatedExternalExtension(
    const std::string& app_id,
    const base::FilePath& crx_path,
    const std::string& version,
    chromeos::ExternalCache::PutExternalExtensionCallback callback) {
  external_cache_->PutExternalExtension(app_id, crx_path, version,
                                        std::move(callback));
}

bool KioskChromeAppManager::IsPlatformCompliant(
    const std::string& required_platform_version) const {
  // Empty required version is compliant with any platform version.
  if (required_platform_version.empty()) {
    return true;
  }

  // Not compliant for bad formatted required versions.
  const base::Version required_version(required_platform_version);
  if (!required_version.IsValid() ||
      required_version.components().size() > 3u) {
    LOG(ERROR) << "Bad formatted required platform version: "
               << required_platform_version;
    return false;
  }

  // Not compliant if the platform version components do not match.
  const size_t count = required_version.components().size();
  const base::Version platform_version = GetPlatformVersion();
  const auto& platform_version_components = platform_version.components();
  const auto& required_version_components = required_version.components();
  for (size_t i = 0; i < count; ++i) {
    if (platform_version_components[i] != required_version_components[i]) {
      return false;
    }
  }

  return true;
}

bool KioskChromeAppManager::IsPlatformCompliantWithApp(
    const extensions::Extension* app) const {
  // Compliant if the app is not the auto launched with zero delay app.
  if (currently_auto_launched_with_zero_delay_app_ != app->id()) {
    return true;
  }

  // Compliant if the app does not specify required platform version.
  const extensions::KioskModeInfo* info = extensions::KioskModeInfo::Get(app);
  if (info == nullptr) {
    return true;
  }

  // Compliant if the app wants to be always updated.
  if (info->always_update) {
    return true;
  }

  return IsPlatformCompliant(info->required_platform_version);
}

KioskChromeAppManager::KioskChromeAppManager() {
  CHECK(!g_instance);  // Only one instance is allowed.
  external_cache_ = CreateExternalCache(this);
  g_instance = this;
  UpdateAppsFromPolicy();
}

KioskChromeAppManager::~KioskChromeAppManager() {
  chromeos::ChromeKioskExternalLoaderBroker::Shutdown();
  observers_.Clear();
  g_test_overrides = nullptr;
  g_instance = nullptr;
}

void KioskChromeAppManager::MonitorKioskExternalUpdate() {
  usb_stick_updater_ = std::make_unique<KioskExternalUpdater>(
      GetBackgroundTaskRunner(), GetCrxCacheDir(), GetCrxUnpackDir());
}

const KioskAppData* KioskChromeAppManager::GetAppData(
    const std::string& app_id) const {
  for (const auto& app : apps_) {
    if (app->app_id() == app_id) {
      return app.get();
    }
  }

  return nullptr;
}

KioskAppData* KioskChromeAppManager::GetAppDataMutable(
    const std::string& app_id) {
  return const_cast<KioskAppData*>(GetAppData(app_id));
}

void KioskChromeAppManager::UpdateAppsFromPolicy() {
  // Gets app id to data mapping for existing apps.
  std::map<std::string, std::unique_ptr<KioskAppData>> old_apps;
  for (auto& app : apps_) {
    old_apps[app->app_id()] = std::move(app);
  }
  apps_.clear();

  auto_launch_app_id_.clear();
  std::string auto_login_account_id;
  CrosSettings::Get()->GetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                                 &auto_login_account_id);

  // Re-populates `apps_` and reuses existing KioskAppData when possible.
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(CrosSettings::Get());
  for (const auto& device_local_account : device_local_accounts) {
    if (device_local_account.type !=
        policy::DeviceLocalAccountType::kKioskApp) {
      continue;
    }

    if (device_local_account.account_id == auto_login_account_id) {
      auto_launch_app_id_ = device_local_account.kiosk_app_id;
    }

    // Note that app ids are not canonical, i.e. they can contain upper
    // case letters.
    const AccountId account_id(
        AccountId::FromUserEmail(device_local_account.user_id));
    auto old_it = old_apps.find(device_local_account.kiosk_app_id);
    if (old_it != old_apps.end()) {
      apps_.push_back(std::move(old_it->second));
      old_apps.erase(old_it);
    } else {
      base::FilePath cached_crx;
      std::string version;
      GetCachedCrx(device_local_account.kiosk_app_id, &cached_crx, &version);

      apps_.push_back(std::make_unique<KioskAppData>(
          this, device_local_account.kiosk_app_id, account_id,
          GURL(device_local_account.kiosk_app_update_url), cached_crx));
      apps_.back()->Load();
    }
    KioskCryptohomeRemover::CancelDelayedCryptohomeRemoval(account_id);
  }

  std::vector<KioskAppDataBase*> apps_to_remove;
  std::vector<std::string> app_ids_to_remove;
  for (auto& entry : old_apps) {
    apps_to_remove.emplace_back(entry.second.get());
    app_ids_to_remove.push_back(entry.second->app_id());
  }
  ClearRemovedApps(apps_to_remove);
  external_cache_->RemoveExtensions(app_ids_to_remove);

  UpdateExternalCachePrefs();
  RetryFailedAppDataFetch();

  NotifyKioskAppsChanged();
}

void KioskChromeAppManager::UpdateExternalCachePrefs() {
  // Request external_cache_ to download new apps and update the existing
  // apps.
  base::Value::Dict prefs;
  for (const auto& app : apps_) {
    base::Value::Dict entry;

    if (app->update_url().is_valid()) {
      entry.Set(extensions::ExternalProviderImpl::kExternalUpdateUrl,
                app->update_url().spec());
    } else {
      entry.Set(extensions::ExternalProviderImpl::kExternalUpdateUrl,
                extension_urls::GetWebstoreUpdateUrl().spec());
    }

    prefs.SetByDottedPath(app->app_id(), std::move(entry));
  }
  external_cache_->UpdateExtensionsList(std::move(prefs));
}

void KioskChromeAppManager::OnExtensionLoadedInCache(
    const extensions::ExtensionId& id,
    bool is_updated) {
  KioskAppData* app_data = GetAppDataMutable(id);
  if (!app_data) {
    return;
  }

  base::FilePath crx_path;
  std::string version;
  if (GetCachedCrx(id, &crx_path, &version)) {
    app_data->SetCachedCrx(crx_path);
  }

  for (auto& observer : observers_) {
    observer.OnKioskExtensionLoadedInCache(id);
  }

  if (is_updated) {
    base::UmaHistogramEnumeration(kKioskPrimaryAppUpdateResultHistogram,
                                  PrimaryAppDownloadResult::kSuccess);
  }
}

void KioskChromeAppManager::OnExtensionDownloadFailed(
    const extensions::ExtensionId& id,
    extensions::ExtensionDownloaderDelegate::Error error) {
  KioskAppData* app_data = GetAppDataMutable(id);
  if (!app_data) {
    return;
  }
  for (auto& observer : observers_) {
    observer.OnKioskExtensionDownloadFailed(id);
  }

  if (!external_cache_->GetExtension(id, nullptr, nullptr)) {
    // Initial install fail.
    base::UmaHistogramEnumeration(kKioskPrimaryAppInstallErrorHistogram,
                                  PrimaryAppDownloadResultFromError(error));
    return;
  }
  base::UmaHistogramEnumeration(kKioskPrimaryAppUpdateResultHistogram,
                                PrimaryAppDownloadResultFromError(error));
}

base::TimeDelta KioskChromeAppManager::GetAutoLaunchDelay() const {
  int delay;
  if (!CrosSettings::Get()->GetInteger(
          kAccountsPrefDeviceLocalAccountAutoLoginDelay, &delay)) {
    return base::TimeDelta();  // Default delay is 0ms.
  }
  return base::Milliseconds(delay);
}

}  // namespace ash
