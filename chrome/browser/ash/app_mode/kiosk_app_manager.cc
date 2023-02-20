// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"

#include <stddef.h>

#include <utility>

#include "ash/constants/ash_paths.h"
#include "ash/constants/ash_switches.h"
#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/ash/app_mode/app_session_ash.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ash/app_mode/kiosk_cryptohome_remover.h"
#include "chrome/browser/ash/app_mode/kiosk_external_updater.h"
#include "chrome/browser/ash/app_mode/pref_names.h"
#include "chrome/browser/ash/extensions/external_cache_impl.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_installer.h"
#include "chrome/browser/extensions/external_loader.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/crosapi/mojom/chrome_app_kiosk_service.mojom.h"
#include "components/account_id/account_id.h"
#include "components/ownership/owner_key_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"

namespace ash {

namespace {

// Domain that is used for kiosk-app account IDs.
constexpr char kKioskAppAccountDomain[] = "kiosk-apps";

// Sub directory under DIR_USER_DATA to store cached crx files.
constexpr char kCrxCacheDir[] = "kiosk/crx";

// Sub directory under DIR_USER_DATA to store unpacked crx file for validating
// its signature.
constexpr char kCrxUnpackDir[] = "kiosk_unpack";

KioskAppManager::Overrides* g_test_overrides = nullptr;

std::string GenerateKioskAppAccountId(const std::string& app_id) {
  return app_id + '@' + kKioskAppAccountDomain;
}

// Check for presence of machine owner public key file.
void CheckOwnerFilePresence(bool* present) {
  scoped_refptr<ownership::OwnerKeyUtil> util =
      OwnerSettingsServiceAshFactory::GetInstance()->GetOwnerKeyUtil();
  *present = util.get() && util->IsPublicKeyPresent();
}

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
      delegate, true /* always_check_updates */,
      false /* wait_for_cache_initialization */,
      true /* allow_scheduled_updates */);
  cache->set_flush_on_put(true);
  return cache;
}

std::unique_ptr<AppSessionAsh> CreateAppSession(Profile* profile) {
  if (g_test_overrides) {
    return g_test_overrides->CreateAppSession();
  }
  return std::make_unique<AppSessionAsh>(profile);
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

KioskAppManager::PrimaryAppDownloadResult PrimaryAppDownloadResultFromError(
    extensions::ExtensionDownloaderDelegate::Error error) {
  switch (error) {
    case extensions::ExtensionDownloaderDelegate::Error::DISABLED:
      return KioskAppManager::PrimaryAppDownloadResult::kDisabled;
    case extensions::ExtensionDownloaderDelegate::Error::MANIFEST_FETCH_FAILED:
      return KioskAppManager::PrimaryAppDownloadResult::kManifestFetchFailed;
    case extensions::ExtensionDownloaderDelegate::Error::MANIFEST_INVALID:
      return KioskAppManager::PrimaryAppDownloadResult::kManifestInvalid;
    case extensions::ExtensionDownloaderDelegate::Error::NO_UPDATE_AVAILABLE:
      return KioskAppManager::PrimaryAppDownloadResult::kNoUpdateAvailable;
    case extensions::ExtensionDownloaderDelegate::Error::CRX_FETCH_URL_EMPTY:
      return KioskAppManager::PrimaryAppDownloadResult::kCrxFetchUrlEmpty;
    case extensions::ExtensionDownloaderDelegate::Error::CRX_FETCH_URL_INVALID:
      return KioskAppManager::PrimaryAppDownloadResult::kCrxFetchUrlInvalid;
    case extensions::ExtensionDownloaderDelegate::Error::CRX_FETCH_FAILED:
      return KioskAppManager::PrimaryAppDownloadResult::kCrxFetchFailed;
  }
}

}  // namespace

// static
const char KioskAppManager::kKioskDictionaryName[] = "kiosk";
const char KioskAppManager::kKeyAutoLoginState[] = "auto_login_state";

const char kKioskPrimaryAppInstallErrorHistogram[] =
    "Kiosk.ChromeApp.PrimaryAppInstallError";
const char kKioskPrimaryAppUpdateResultHistogram[] =
    "Kiosk.ChromeApp.PrimaryAppUpdateResult";
const char kKioskExternalUpdateSuccessHistogram[] =
    "Kiosk.ChromeApp.ExternalUpdateSuccess";

namespace {
// This class is owned by `ChromeBrowserMainPartsAsh`.
static KioskAppManager* g_kiosk_app_manager = nullptr;
}  // namespace

// static
KioskAppManager* KioskAppManager::Get() {
  CHECK(g_kiosk_app_manager);
  return g_kiosk_app_manager;
}

// static
bool KioskAppManager::IsInitialized() {
  return g_kiosk_app_manager;
}

// static
void KioskAppManager::InitializeForTesting(Overrides* overrides) {
  DCHECK(!g_kiosk_app_manager);
  g_test_overrides = overrides;
}

// static
void KioskAppManager::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kKioskDictionaryName);
  chromeos::AppSession::RegisterLocalStatePrefs(registry);
}

// static
void KioskAppManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  chromeos::AppSession::RegisterProfilePrefs(registry);
}

// static
bool KioskAppManager::IsConsumerKioskEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableConsumerKiosk);
}

std::string KioskAppManager::GetAutoLaunchApp() const {
  return auto_launch_app_id_;
}

void KioskAppManager::SetAutoLaunchApp(const std::string& app_id,
                                       OwnerSettingsServiceAsh* service) {
  SetAutoLoginState(AutoLoginState::kRequested);
  // Clean first, so the proper change callbacks are triggered even
  // if we are only changing AutoLoginState here.
  if (!auto_launch_app_id_.empty()) {
    service->SetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                       std::string());
  }

  service->SetString(
      kAccountsPrefDeviceLocalAccountAutoLoginId,
      app_id.empty() ? std::string() : GenerateKioskAppAccountId(app_id));
  service->SetInteger(kAccountsPrefDeviceLocalAccountAutoLoginDelay, 0);
}

void KioskAppManager::SetAppWasAutoLaunchedWithZeroDelay(
    const std::string& app_id) {
  DCHECK_EQ(auto_launch_app_id_, app_id);
  currently_auto_launched_with_zero_delay_app_ = app_id;
  auto_launched_with_zero_delay_ = true;
}

void KioskAppManager::SetExtensionDownloaderBackoffPolicy(
    absl::optional<net::BackoffEntry::Policy> backoff_policy) {
  // In browser tests `external_cache_` is reset before `StartupAppLauncher`.
  // Check before trying to set backoff policy here.
  if (!external_cache_) {
    return;
  }
  external_cache_->SetBackoffPolicy(backoff_policy);
}

void KioskAppManager::InitSession(Profile* profile, const std::string& app_id) {
  LOG_IF(FATAL, app_session_) << "Kiosk session is already initialized.";

  base::CommandLine session_flags(base::CommandLine::NO_PROGRAM);
  if (GetSwitchesForSessionRestore(app_id, &session_flags)) {
    base::CommandLine::StringVector flags;
    // argv[0] is the program name |base::CommandLine::NO_PROGRAM|.
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

  app_session_ = CreateAppSession(profile);
  if (app_session_) {
    app_session_->Init(app_id);
  }
  NotifySessionInitialized();
}

bool KioskAppManager::GetSwitchesForSessionRestore(
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

void KioskAppManager::OnExternalCacheDamaged(const std::string& app_id) {
  CHECK(external_cache_);
  base::FilePath crx_path;
  std::string version;
  GetCachedCrx(app_id, &crx_path, &version);
  external_cache_->OnDamagedFileDetected(crx_path);
}

void KioskAppManager::AddAppForTest(
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

void KioskAppManager::EnableConsumerKioskAutoLaunch(
    KioskAppManager::EnableKioskAutoLaunchCallback callback) {
  if (!IsConsumerKioskEnabled()) {
    if (callback) {
      std::move(callback).Run(false);
    }
    return;
  }

  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  connector->GetInstallAttributes()->LockDevice(
      policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH,
      std::string(),  // domain
      std::string(),  // realm
      std::string(),  // device_id
      base::BindOnce(&KioskAppManager::OnLockDevice, base::Unretained(this),
                     std::move(callback)));
}

void KioskAppManager::GetConsumerKioskAutoLaunchStatus(
    KioskAppManager::GetConsumerKioskAutoLaunchStatusCallback callback) {
  if (!IsConsumerKioskEnabled()) {
    if (callback) {
      std::move(callback).Run(ConsumerKioskAutoLaunchStatus::kDisabled);
    }
    return;
  }

  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  connector->GetInstallAttributes()->ReadImmutableAttributes(
      base::BindOnce(&KioskAppManager::OnReadImmutableAttributes,
                     base::Unretained(this), std::move(callback)));
}

bool KioskAppManager::IsConsumerKioskDeviceWithAutoLaunch() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return connector->GetInstallAttributes() &&
         connector->GetInstallAttributes()
             ->IsConsumerKioskDeviceWithAutoLaunch();
}

void KioskAppManager::OnLockDevice(
    KioskAppManager::EnableKioskAutoLaunchCallback callback,
    InstallAttributes::LockResult result) {
  if (!callback) {
    return;
  }

  std::move(callback).Run(result == InstallAttributes::LOCK_SUCCESS);
}

void KioskAppManager::OnOwnerFileChecked(
    KioskAppManager::GetConsumerKioskAutoLaunchStatusCallback callback,
    bool* owner_present) {
  ownership_established_ = *owner_present;

  if (!callback) {
    return;
  }

  // If we have owner already established on the machine, don't let
  // consumer kiosk to be enabled.
  if (ownership_established_) {
    std::move(callback).Run(ConsumerKioskAutoLaunchStatus::kDisabled);
  } else {
    std::move(callback).Run(ConsumerKioskAutoLaunchStatus::kConfigurable);
  }
}

void KioskAppManager::OnReadImmutableAttributes(
    KioskAppManager::GetConsumerKioskAutoLaunchStatusCallback callback) {
  if (!callback) {
    return;
  }

  ConsumerKioskAutoLaunchStatus status =
      ConsumerKioskAutoLaunchStatus::kDisabled;
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  InstallAttributes* attributes = connector->GetInstallAttributes();
  switch (attributes->GetMode()) {
    case policy::DEVICE_MODE_NOT_SET: {
      if (!base::SysInfo::IsRunningOnChromeOS()) {
        status = ConsumerKioskAutoLaunchStatus::kConfigurable;
      } else if (!ownership_established_) {
        bool* owner_present = new bool(false);
        base::ThreadPool::PostTaskAndReply(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
            base::BindOnce(&CheckOwnerFilePresence, owner_present),
            base::BindOnce(&KioskAppManager::OnOwnerFileChecked,
                           base::Unretained(this), std::move(callback),
                           base::Owned(owner_present)));
        return;
      }
      break;
    }
    case policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH:
      status = ConsumerKioskAutoLaunchStatus::kEnabled;
      break;
    default:
      break;
  }

  std::move(callback).Run(status);
}

void KioskAppManager::SetEnableAutoLaunch(bool value) {
  SetAutoLoginState(value ? AutoLoginState::kApproved
                          : AutoLoginState::kRejected);
}

bool KioskAppManager::IsAutoLaunchRequested() const {
  if (GetAutoLaunchApp().empty()) {
    return false;
  }

  // Apps that were installed by the policy don't require machine owner
  // consent through UI.
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (connector->IsDeviceEnterpriseManaged()) {
    return false;
  }

  return GetAutoLoginState() == AutoLoginState::kRequested;
}

bool KioskAppManager::IsAutoLaunchEnabled() const {
  if (GetAutoLaunchApp().empty()) {
    return false;
  }

  // Apps that were installed by the policy don't require machine owner
  // consent through UI.
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (connector->IsDeviceEnterpriseManaged()) {
    return true;
  }

  return GetAutoLoginState() == AutoLoginState::kApproved;
}

std::string KioskAppManager::GetAutoLaunchAppRequiredPlatformVersion() const {
  // Bail out if there is no auto launched app with zero delay.
  if (!IsAutoLaunchEnabled() || !GetAutoLaunchDelay().is_zero()) {
    return std::string();
  }

  const KioskAppData* data = GetAppData(GetAutoLaunchApp());
  return data == nullptr ? std::string() : data->required_platform_version();
}

void KioskAppManager::AddApp(const std::string& app_id,
                             OwnerSettingsServiceAsh* service) {
  std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(CrosSettings::Get());

  // Don't insert the app if it's already in the list.
  for (std::vector<policy::DeviceLocalAccount>::const_iterator it =
           device_local_accounts.begin();
       it != device_local_accounts.end(); ++it) {
    if (it->type == policy::DeviceLocalAccount::TYPE_KIOSK_APP &&
        it->kiosk_app_id == app_id) {
      return;
    }
  }

  // Add the new account.
  device_local_accounts.emplace_back(policy::DeviceLocalAccount::TYPE_KIOSK_APP,
                                     GenerateKioskAppAccountId(app_id), app_id,
                                     std::string());

  policy::SetDeviceLocalAccounts(service, device_local_accounts);
}

void KioskAppManager::RemoveApp(const std::string& app_id,
                                OwnerSettingsServiceAsh* service) {
  // Resets auto launch app if it is the removed app.
  if (auto_launch_app_id_ == app_id) {
    SetAutoLaunchApp(std::string(), service);
  }

  std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(CrosSettings::Get());
  if (device_local_accounts.empty()) {
    return;
  }

  // Remove entries that match |app_id|.
  for (std::vector<policy::DeviceLocalAccount>::iterator it =
           device_local_accounts.begin();
       it != device_local_accounts.end(); ++it) {
    if (it->type == policy::DeviceLocalAccount::TYPE_KIOSK_APP &&
        it->kiosk_app_id == app_id) {
      device_local_accounts.erase(it);
      break;
    }
  }

  policy::SetDeviceLocalAccounts(service, device_local_accounts);
}

void KioskAppManager::GetApps(Apps* apps) const {
  apps->clear();
  for (const auto& app : apps_) {
    const KioskAppData& app_data = *app;
    if (app_data.status() != KioskAppData::Status::kError) {
      apps->push_back(ConstructApp(app_data));
    }
  }
}

KioskAppManager::App KioskAppManager::ConstructApp(
    const KioskAppData& data) const {
  App app(data);
  app.required_platform_version = data.required_platform_version();
  app.is_loading = external_cache_->ExtensionFetchPending(app.app_id);
  app.was_auto_launched_with_zero_delay =
      app.app_id == currently_auto_launched_with_zero_delay_app_;
  return app;
}

bool KioskAppManager::GetApp(const std::string& app_id, App* app) const {
  const KioskAppData* data = GetAppData(app_id);
  if (!data) {
    return false;
  }
  *app = ConstructApp(*data);
  return true;
}

void KioskAppManager::ClearAppData(const std::string& app_id) {
  KioskAppData* app_data = GetAppDataMutable(app_id);
  if (!app_data) {
    return;
  }

  app_data->ClearCache();
}

void KioskAppManager::UpdateAppDataFromProfile(
    const std::string& app_id,
    Profile* profile,
    const extensions::Extension* app) {
  KioskAppData* app_data = GetAppDataMutable(app_id);
  if (!app_data) {
    return;
  }

  app_data->LoadFromInstalledApp(profile, app);
}

void KioskAppManager::RetryFailedAppDataFetch() {
  for (const auto& app : apps_) {
    if (app->status() == KioskAppData::Status::kError) {
      app->Load();
    }
  }
}

bool KioskAppManager::HasCachedCrx(const std::string& app_id) const {
  base::FilePath crx_path;
  std::string version;
  return GetCachedCrx(app_id, &crx_path, &version);
}

bool KioskAppManager::GetCachedCrx(const std::string& app_id,
                                   base::FilePath* file_path,
                                   std::string* version) const {
  return external_cache_->GetExtension(app_id, file_path, version);
}

crosapi::mojom::AppInstallParams KioskAppManager::CreatePrimaryAppInstallData(
    const std::string& id) const {
  const base::Value::Dict* extension =
      external_cache_->GetCachedExtensions().FindDict(id);
  if (!extension) {
    return crosapi::mojom::AppInstallParams(id, std::string(), std::string(),
                                            false);
  }

  const absl::optional<bool> is_store_app_maybe =
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

void KioskAppManager::UpdateExternalCache() {
  UpdateAppsFromPolicy();
}

void KioskAppManager::OnKioskAppCacheUpdated(const std::string& app_id) {
  for (auto& observer : observers_) {
    observer.OnKioskAppCacheUpdated(app_id);
  }
}

void KioskAppManager::OnKioskAppExternalUpdateComplete(bool success) {
  base::UmaHistogramBoolean(kKioskExternalUpdateSuccessHistogram, success);
  for (auto& observer : observers_) {
    observer.OnKioskAppExternalUpdateComplete(success);
  }
}

void KioskAppManager::PutValidatedExternalExtension(
    const std::string& app_id,
    const base::FilePath& crx_path,
    const std::string& version,
    chromeos::ExternalCache::PutExternalExtensionCallback callback) {
  external_cache_->PutExternalExtension(app_id, crx_path, version,
                                        std::move(callback));
}

bool KioskAppManager::IsPlatformCompliant(
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

bool KioskAppManager::IsPlatformCompliantWithApp(
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

KioskAppManager::KioskAppManager() {
  CHECK(!g_kiosk_app_manager);  // Only one instance is allowed.
  external_cache_ = CreateExternalCache(this);
  g_kiosk_app_manager = this;
  UpdateAppsFromPolicy();
}

KioskAppManager::~KioskAppManager() {
  ChromeKioskExternalLoaderBroker::Shutdown();
  observers_.Clear();
  g_test_overrides = nullptr;
  g_kiosk_app_manager = nullptr;
}

void KioskAppManager::MonitorKioskExternalUpdate() {
  usb_stick_updater_ = std::make_unique<KioskExternalUpdater>(
      GetBackgroundTaskRunner(), GetCrxCacheDir(), GetCrxUnpackDir());
}

const KioskAppData* KioskAppManager::GetAppData(
    const std::string& app_id) const {
  for (const auto& app : apps_) {
    if (app->app_id() == app_id) {
      return app.get();
    }
  }

  return nullptr;
}

KioskAppData* KioskAppManager::GetAppDataMutable(const std::string& app_id) {
  return const_cast<KioskAppData*>(GetAppData(app_id));
}

void KioskAppManager::UpdateAppsFromPolicy() {
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

  // Re-populates |apps_| and reuses existing KioskAppData when possible.
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(CrosSettings::Get());
  for (const auto& device_local_account : device_local_accounts) {
    if (device_local_account.type !=
        policy::DeviceLocalAccount::TYPE_KIOSK_APP) {
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

void KioskAppManager::UpdateExternalCachePrefs() {
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

void KioskAppManager::OnExtensionLoadedInCache(
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

void KioskAppManager::OnExtensionDownloadFailed(
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

KioskAppManager::AutoLoginState KioskAppManager::GetAutoLoginState() const {
  PrefService* prefs = g_browser_process->local_state();
  const base::Value::Dict& dict =
      prefs->GetDict(KioskAppManager::kKioskDictionaryName);
  absl::optional<int> value = dict.FindInt(kKeyAutoLoginState);
  if (!value.has_value()) {
    return AutoLoginState::kNone;
  }

  return static_cast<AutoLoginState>(value.value());
}

void KioskAppManager::SetAutoLoginState(AutoLoginState state) {
  PrefService* prefs = g_browser_process->local_state();
  ScopedDictPrefUpdate dict_update(prefs,
                                   KioskAppManager::kKioskDictionaryName);
  dict_update->Set(kKeyAutoLoginState, static_cast<int>(state));
  prefs->CommitPendingWrite();
}

base::TimeDelta KioskAppManager::GetAutoLaunchDelay() const {
  int delay;
  if (!CrosSettings::Get()->GetInteger(
          kAccountsPrefDeviceLocalAccountAutoLoginDelay, &delay)) {
    return base::TimeDelta();  // Default delay is 0ms.
  }
  return base::Milliseconds(delay);
}

}  // namespace ash
