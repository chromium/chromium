// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_service.h"

#include <memory>
#include <set>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/permission_request_creator.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_observer.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/buildflags/buildflags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/login/users/supervised_user_manager.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#endif

using base::UserMetricsAction;

#if BUILDFLAG(ENABLE_EXTENSIONS)
using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;
using extensions::ExtensionSystem;
#endif

namespace {

// The URL from which to download a host denylist if no local one exists yet.
const char kDenylistURL[] =
    "https://www.gstatic.com/chrome/supervised_user/denylist-20141001-1k.bin";
// The filename under which we'll store the denylist (in the user data dir).
const char kDenylistFilename[] = "su-denylist.bin";

const char kDenylistSourceHistogramName[] = "FamilyUser.DenylistSource";

#if BUILDFLAG(ENABLE_EXTENSIONS)
// These extensions are allowed for supervised users for internal development
// purposes.
constexpr char const* kAllowlistExtensionIds[] = {
    "behllobkkfkfnphdnhnkndlbkcpglgmj"  // Tast extension.
};

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

base::FilePath GetDenylistPath() {
  base::FilePath denylist_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &denylist_dir);
  return denylist_dir.AppendASCII(kDenylistFilename);
}

bool AreWebFilterPrefsDefault(PrefService* pref_service) {
  return pref_service
             ->FindPreference(prefs::kDefaultSupervisedUserFilteringBehavior)
             ->IsDefaultValue() ||
         pref_service->FindPreference(prefs::kSupervisedUserSafeSites)
             ->IsDefaultValue();
}

}  // namespace

SupervisedUserService::~SupervisedUserService() {
  DCHECK(!did_init_ || did_shutdown_);
  url_filter_.RemoveObserver(this);
}

// static
void SupervisedUserService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  registry->RegisterBooleanPref(
      prefs::kSupervisedUserExtensionsMayRequestPermissions, false);
  registry->RegisterDictionaryPref(
      prefs::kSupervisedUserApprovedExtensions,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif
  registry->RegisterDictionaryPref(prefs::kSupervisedUserManualHosts);
  registry->RegisterDictionaryPref(prefs::kSupervisedUserManualURLs);
  registry->RegisterIntegerPref(prefs::kDefaultSupervisedUserFilteringBehavior,
                                SupervisedUserURLFilter::ALLOW);
  registry->RegisterBooleanPref(prefs::kSupervisedUserSafeSites, true);
  for (const char* pref : supervised_users::kCustodianInfoPrefs) {
    registry->RegisterStringPref(pref, std::string());
  }
}

// static
const char* SupervisedUserService::GetDenylistSourceHistogramForTesting() {
  return kDenylistSourceHistogramName;
}

// static
base::FilePath SupervisedUserService::GetDenylistPathForTesting() {
  return GetDenylistPath();
}

void SupervisedUserService::Init() {
  DCHECK(!did_init_);
  did_init_ = true;
  DCHECK(GetSettingsService()->IsReady());

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kSupervisedUserId,
      base::BindRepeating(&SupervisedUserService::OnSupervisedUserIdChanged,
                          base::Unretained(this)));

  SetActive(IsChild());
}

void SupervisedUserService::SetDelegate(Delegate* delegate) {
  if (delegate) {
    // Changing delegates isn't allowed.
    DCHECK(!delegate_);
  } else {
    // If the delegate is removed, deactivate first to give the old delegate a
    // chance to clean up.
    SetActive(false);
  }
  delegate_ = delegate;
}

SupervisedUserURLFilter* SupervisedUserService::GetURLFilter() {
  return &url_filter_;
}

// static
std::string SupervisedUserService::GetExtensionRequestId(
    const std::string& extension_id,
    const base::Version& version) {
  return base::StringPrintf("%s:%s", extension_id.c_str(),
                            version.GetString().c_str());
}

std::string SupervisedUserService::GetCustodianEmailAddress() const {
  return profile_->GetPrefs()->GetString(prefs::kSupervisedUserCustodianEmail);
}

std::string SupervisedUserService::GetCustodianObfuscatedGaiaId() const {
  return profile_->GetPrefs()->GetString(
      prefs::kSupervisedUserCustodianObfuscatedGaiaId);
}

std::string SupervisedUserService::GetCustodianName() const {
  std::string name =
      profile_->GetPrefs()->GetString(prefs::kSupervisedUserCustodianName);
  return name.empty() ? GetCustodianEmailAddress() : name;
}

std::string SupervisedUserService::GetSecondCustodianEmailAddress() const {
  return profile_->GetPrefs()->GetString(
      prefs::kSupervisedUserSecondCustodianEmail);
}

std::string SupervisedUserService::GetSecondCustodianObfuscatedGaiaId() const {
  return profile_->GetPrefs()->GetString(
      prefs::kSupervisedUserSecondCustodianObfuscatedGaiaId);
}

std::string SupervisedUserService::GetSecondCustodianName() const {
  std::string name = profile_->GetPrefs()->GetString(
      prefs::kSupervisedUserSecondCustodianName);
  return name.empty() ? GetSecondCustodianEmailAddress() : name;
}

std::u16string SupervisedUserService::GetExtensionsLockedMessage() const {
  return l10n_util::GetStringFUTF16(IDS_EXTENSIONS_LOCKED_SUPERVISED_USER,
                                    base::UTF8ToUTF16(GetCustodianName()));
}

// static
std::string SupervisedUserService::GetEduCoexistenceLoginUrl() {
  return chrome::kChromeUIEDUCoexistenceLoginURLV2;
}

bool SupervisedUserService::IsChild() const {
  return profile_->IsChild();
}

bool SupervisedUserService::HasACustodian() const {
  return !GetCustodianEmailAddress().empty() ||
         !GetSecondCustodianEmailAddress().empty();
}

void SupervisedUserService::AddObserver(
    SupervisedUserServiceObserver* observer) {
  observer_list_.AddObserver(observer);
}

void SupervisedUserService::RemoveObserver(
    SupervisedUserServiceObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

SupervisedUserService::SupervisedUserService(Profile* profile)
    : profile_(profile),
      active_(false),
      delegate_(nullptr),
      is_profile_active_(false),
      did_init_(false),
      did_shutdown_(false),
      denylist_state_(DenylistLoadState::NOT_LOADED) {
  url_filter_.AddObserver(this);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  registry_observation_.Observe(extensions::ExtensionRegistry::Get(profile));
#endif
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void SupervisedUserService::AddExtensionApproval(
    const extensions::Extension& extension) {
  if (!active_)
    return;
  if (!base::Contains(approved_extensions_set_, extension.id())) {
    UpdateApprovedExtension(extension.id(), extension.VersionString(),
                            ApprovedExtensionChange::kAdd);
  } else if (ExtensionPrefs::Get(profile_)->DidExtensionEscalatePermissions(
                 extension.id())) {
    SupervisedUserExtensionsMetricsRecorder::RecordExtensionsUmaMetrics(
        SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
            kPermissionsIncreaseGranted);
  }
}

void SupervisedUserService::RemoveExtensionApproval(
    const extensions::Extension& extension) {
  if (!active_)
    return;
  if (base::Contains(approved_extensions_set_, extension.id())) {
    UpdateApprovedExtension(extension.id(), extension.VersionString(),
                            ApprovedExtensionChange::kRemove);
  }
}

void SupervisedUserService::UpdateApprovedExtensionForTesting(
    const std::string& extension_id,
    ApprovedExtensionChange type) {
  base::Version dummy_version("0");
  UpdateApprovedExtension(extension_id, dummy_version.GetString(), type);
}

bool SupervisedUserService::
    GetSupervisedUserExtensionsMayRequestPermissionsPref() const {
  DCHECK(IsChild())
      << "Calling GetSupervisedUserExtensionsMayRequestPermissionsPref() only "
         "makes sense for supervised users";
  return profile_->GetPrefs()->GetBoolean(
      prefs::kSupervisedUserExtensionsMayRequestPermissions);
}

void SupervisedUserService::
    SetSupervisedUserExtensionsMayRequestPermissionsPrefForTesting(
        bool enabled) {
  // TODO(crbug/1024646): kSupervisedUserExtensionsMayRequestPermissions is
  // currently set indirectly by setting geolocation requests. Update Kids
  // Management server to set a new bit for extension permissions and update
  // this setter function.
  GetSettingsService()->SetLocalSetting(supervised_users::kGeolocationDisabled,
                                        base::Value(!enabled));
  profile_->GetPrefs()->SetBoolean(
      prefs::kSupervisedUserExtensionsMayRequestPermissions, enabled);
}

bool SupervisedUserService::CanInstallExtensions() const {
  return HasACustodian() &&
         GetSupervisedUserExtensionsMayRequestPermissionsPref();
}

bool SupervisedUserService::IsExtensionAllowed(
    const extensions::Extension& extension) const {
  return GetExtensionState(extension) ==
         SupervisedUserService::ExtensionState::ALLOWED;
}

void SupervisedUserService::RecordExtensionEnablementUmaMetrics(
    bool enabled) const {
  if (!active_)
    return;
  auto state =
      enabled
          ? SupervisedUserExtensionsMetricsRecorder::EnablementState::kEnabled
          : SupervisedUserExtensionsMetricsRecorder::EnablementState::kDisabled;
  SupervisedUserExtensionsMetricsRecorder::RecordEnablementUmaMetrics(state);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

void SupervisedUserService::ReportNonDefaultWebFilterValue() const {
  if (AreWebFilterPrefsDefault(profile_->GetPrefs()))
    return;

  url_filter_.ReportManagedSiteListMetrics();
  url_filter_.ReportWebFilterTypeMetrics();
}

void SupervisedUserService::SetActive(bool active) {
  if (active_ == active)
    return;
  active_ = active;

  if (delegate_)
    delegate_->SetActive(active_);

    // Now activate/deactivate anything not handled by the delegate yet.
#if !BUILDFLAG(IS_ANDROID)
  // Re-set the default theme to turn the SU theme on/off.
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile_);
  if (theme_service->UsingDefaultTheme() || theme_service->UsingSystemTheme())
    theme_service->UseDefaultTheme();
#endif

  // Trigger a sync reconfig to enable/disable the right SU data types.
  // The logic to do this lives in the SupervisedUserSyncModelTypeController.
  // TODO(crbug.com/946473): Get rid of this hack and instead call
  // DataTypePreconditionChanged from the controller.
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  if (sync_service->GetUserSettings()->IsFirstSetupComplete()) {
    // Trigger a reconfig by grabbing a SyncSetupInProgressHandle and
    // immediately releasing it again (via the temporary unique_ptr going away).
    sync_service->GetSetupInProgressHandle();
  }

  GetSettingsService()->SetActive(active_);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  SetExtensionsActive();
#endif

  if (active_) {
    pref_change_registrar_.Add(
        prefs::kDefaultSupervisedUserFilteringBehavior,
        base::BindRepeating(
            &SupervisedUserService::OnDefaultFilteringBehaviorChanged,
            base::Unretained(this)));
#if BUILDFLAG(ENABLE_EXTENSIONS)
    pref_change_registrar_.Add(
        prefs::kSupervisedUserApprovedExtensions,
        base::BindRepeating(
            &SupervisedUserService::RefreshApprovedExtensionsFromPrefs,
            base::Unretained(this)));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
    pref_change_registrar_.Add(
        prefs::kSupervisedUserSafeSites,
        base::BindRepeating(&SupervisedUserService::OnSafeSitesSettingChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kSupervisedUserManualHosts,
        base::BindRepeating(&SupervisedUserService::UpdateManualHosts,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kSupervisedUserManualURLs,
        base::BindRepeating(&SupervisedUserService::UpdateManualURLs,
                            base::Unretained(this)));
    for (const char* pref : supervised_users::kCustodianInfoPrefs) {
      pref_change_registrar_.Add(
          pref,
          base::BindRepeating(&SupervisedUserService::OnCustodianInfoChanged,
                              base::Unretained(this)));
    }

    // Initialize the filter.
    OnDefaultFilteringBehaviorChanged();
    OnSafeSitesSettingChanged();
    UpdateManualHosts();
    UpdateManualURLs();

    GetURLFilter()->SetFilterInitialized(true);
    current_web_filter_type_ = url_filter_.GetWebFilterType();

#if BUILDFLAG(ENABLE_EXTENSIONS)
    RefreshApprovedExtensionsFromPrefs();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if !BUILDFLAG(IS_ANDROID)
    // TODO(bauerb): Get rid of the platform-specific #ifdef here.
    // http://crbug.com/313377
    BrowserList::AddObserver(this);
#endif
  } else {
    web_approvals_manager_.ClearRemoteApprovalRequestsCreators();

    pref_change_registrar_.Remove(
        prefs::kDefaultSupervisedUserFilteringBehavior);
#if BUILDFLAG(ENABLE_EXTENSIONS)
    pref_change_registrar_.Remove(prefs::kSupervisedUserApprovedExtensions);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
    pref_change_registrar_.Remove(prefs::kSupervisedUserManualHosts);
    pref_change_registrar_.Remove(prefs::kSupervisedUserManualURLs);
    for (const char* pref : supervised_users::kCustodianInfoPrefs) {
      pref_change_registrar_.Remove(pref);
    }

    url_filter_.Clear();
    for (SupervisedUserServiceObserver& observer : observer_list_)
      observer.OnURLFilterChanged();

#if !BUILDFLAG(IS_ANDROID)
    // TODO(bauerb): Get rid of the platform-specific #ifdef here.
    // http://crbug.com/313377
    BrowserList::RemoveObserver(this);
#endif
  }
}

void SupervisedUserService::OnCustodianInfoChanged() {
  for (SupervisedUserServiceObserver& observer : observer_list_)
    observer.OnCustodianInfoChanged();
}

SupervisedUserSettingsService* SupervisedUserService::GetSettingsService() {
  return SupervisedUserSettingsServiceFactory::GetForKey(
      profile_->GetProfileKey());
}

PrefService* SupervisedUserService::GetPrefService() {
  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service) << "PrefService not found";
  return pref_service;
}

void SupervisedUserService::OnSupervisedUserIdChanged() {
  SetActive(IsChild());
}

void SupervisedUserService::OnDefaultFilteringBehaviorChanged() {
  int behavior_value = profile_->GetPrefs()->GetInteger(
      prefs::kDefaultSupervisedUserFilteringBehavior);
  SupervisedUserURLFilter::FilteringBehavior behavior =
      SupervisedUserURLFilter::BehaviorFromInt(behavior_value);
  url_filter_.SetDefaultFilteringBehavior(behavior);
  UpdateAsyncUrlChecker();

  for (SupervisedUserServiceObserver& observer : observer_list_)
    observer.OnURLFilterChanged();

  SupervisedUserURLFilter::WebFilterType filter_type =
      url_filter_.GetWebFilterType();
  if (!AreWebFilterPrefsDefault(profile_->GetPrefs()) &&
      current_web_filter_type_ != filter_type) {
    url_filter_.ReportWebFilterTypeMetrics();
    current_web_filter_type_ = filter_type;
  }
}

bool SupervisedUserService::IsSafeSitesEnabled() const {
  return profile_->IsChild() &&
         profile_->GetPrefs()->GetBoolean(prefs::kSupervisedUserSafeSites);
}

void SupervisedUserService::OnSafeSitesSettingChanged() {
  bool use_denylist = IsSafeSitesEnabled();
  if (use_denylist != url_filter_.HasDenylist()) {
    if (use_denylist && denylist_state_ == DenylistLoadState::NOT_LOADED) {
      LoadDenylist(GetDenylistPath(), GURL(kDenylistURL));
    } else if (!use_denylist || denylist_state_ == DenylistLoadState::LOADED) {
      // Either the denylist was turned off, or it was turned on but has
      // already been loaded previously. Just update the setting.
      UpdateDenylist();
    }
    // Else: The denylist was enabled, but the load is already in progress.
    // Do nothing - we'll check the setting again when the load finishes.
  }

  UpdateAsyncUrlChecker();

  SupervisedUserURLFilter::WebFilterType filter_type =
      url_filter_.GetWebFilterType();
  if (!AreWebFilterPrefsDefault(profile_->GetPrefs()) &&
      current_web_filter_type_ != filter_type) {
    url_filter_.ReportWebFilterTypeMetrics();
    current_web_filter_type_ = filter_type;
  }
}

void SupervisedUserService::UpdateAsyncUrlChecker() {
  int behavior_value = profile_->GetPrefs()->GetInteger(
      prefs::kDefaultSupervisedUserFilteringBehavior);
  SupervisedUserURLFilter::FilteringBehavior behavior =
      SupervisedUserURLFilter::BehaviorFromInt(behavior_value);

  bool use_online_check =
      IsSafeSitesEnabled() ||
      behavior == SupervisedUserURLFilter::FilteringBehavior::BLOCK;

  if (use_online_check != url_filter_.HasAsyncURLChecker()) {
    if (use_online_check) {
      url_filter_.InitAsyncURLChecker();
    } else {
      url_filter_.ClearAsyncURLChecker();
    }
  }
}

void SupervisedUserService::LoadDenylist(const base::FilePath& path,
                                         const GURL& url) {
  DCHECK(denylist_state_ == DenylistLoadState::NOT_LOADED);
  denylist_state_ = DenylistLoadState::LOAD_STARTED;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&base::PathExists, path),
      base::BindOnce(&SupervisedUserService::OnDenylistFileChecked,
                     weak_ptr_factory_.GetWeakPtr(), path, url));
}

void SupervisedUserService::OnDenylistFileChecked(const base::FilePath& path,
                                                  const GURL& url,
                                                  bool file_exists) {
  DCHECK(denylist_state_ == DenylistLoadState::LOAD_STARTED);
  if (file_exists) {
    LoadDenylistFromFile(path);
    base::UmaHistogramEnumeration(kDenylistSourceHistogramName,
                                  DenylistSource::kDenylist);
    return;
  }

  DCHECK(!denylist_downloader_);

  // Create traffic annotation tag.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("supervised_users_denylist", R"(
        semantics {
          sender: "Supervised Users"
          description:
            "Downloads a static denylist consisting of hostname hashes of "
            "common inappropriate websites. This is only enabled for child "
            "accounts and only if the corresponding setting is enabled by the "
            "parent."
          trigger:
            "The file is downloaded on demand if the child account profile is "
            "created and the setting is enabled."
          data:
            "No additional data is sent to the server beyond the request "
            "itself."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "The feature can be remotely enabled or disabled by the parent. In "
            "addition, if sign-in is restricted to accounts from a managed "
            "domain, those accounts are not going to be child accounts."
          chrome_policy {
            RestrictSigninToPattern {
              policy_options {mode: MANDATORY}
              RestrictSigninToPattern: "*@manageddomain.com"
            }
          }
        })");

  auto factory = profile_->GetDefaultStoragePartition()
                     ->GetURLLoaderFactoryForBrowserProcess();
  denylist_downloader_ = std::make_unique<FileDownloader>(
      url, path, false, std::move(factory),
      base::BindOnce(&SupervisedUserService::OnDenylistDownloadDone,
                     base::Unretained(this), path),
      traffic_annotation);
}

void SupervisedUserService::LoadDenylistFromFile(const base::FilePath& path) {
  DCHECK(denylist_state_ == DenylistLoadState::LOAD_STARTED);
  denylist_.ReadFromFile(
      path, base::BindRepeating(&SupervisedUserService::OnDenylistLoaded,
                                base::Unretained(this)));
}

void SupervisedUserService::OnDenylistDownloadDone(
    const base::FilePath& path,
    FileDownloader::Result result) {
  DCHECK(denylist_state_ == DenylistLoadState::LOAD_STARTED);
  if (FileDownloader::IsSuccess(result)) {
    LoadDenylistFromFile(path);
    base::UmaHistogramEnumeration(kDenylistSourceHistogramName,
                                  DenylistSource::kDenylist);
  }
  LOG(WARNING) << "Denylist download failed";
  denylist_downloader_.reset();
}

void SupervisedUserService::OnDenylistLoaded() {
  DCHECK(denylist_state_ == DenylistLoadState::LOAD_STARTED);
  denylist_state_ = DenylistLoadState::LOADED;
  UpdateDenylist();
}

void SupervisedUserService::UpdateDenylist() {
  url_filter_.SetDenylist(IsSafeSitesEnabled() ? &denylist_ : nullptr);
  for (SupervisedUserServiceObserver& observer : observer_list_)
    observer.OnURLFilterChanged();
}

void SupervisedUserService::UpdateManualHosts() {
  const base::Value::Dict& dict =
      profile_->GetPrefs()->GetDict(prefs::kSupervisedUserManualHosts);
  std::map<std::string, bool> host_map;
  for (auto it : dict) {
    DCHECK(it.second.is_bool());
    host_map[it.first] = it.second.GetIfBool().value_or(false);
  }
  url_filter_.SetManualHosts(std::move(host_map));

  for (SupervisedUserServiceObserver& observer : observer_list_)
    observer.OnURLFilterChanged();

  if (!AreWebFilterPrefsDefault(profile_->GetPrefs()))
    url_filter_.ReportManagedSiteListMetrics();
}

void SupervisedUserService::UpdateManualURLs() {
  const base::Value::Dict& dict =
      profile_->GetPrefs()->GetDict(prefs::kSupervisedUserManualURLs);
  std::map<GURL, bool> url_map;
  for (auto it : dict) {
    DCHECK(it.second.is_bool());
    url_map[GURL(it.first)] = it.second.GetIfBool().value_or(false);
  }
  url_filter_.SetManualURLs(std::move(url_map));

  for (SupervisedUserServiceObserver& observer : observer_list_)
    observer.OnURLFilterChanged();

  if (!AreWebFilterPrefsDefault(profile_->GetPrefs()))
    url_filter_.ReportManagedSiteListMetrics();
}

void SupervisedUserService::Shutdown() {
  if (!did_init_)
    return;
  DCHECK(!did_shutdown_);
  did_shutdown_ = true;
  if (IsChild()) {
    base::RecordAction(UserMetricsAction("ManagedUsers_QuitBrowser"));
  }
  SetActive(false);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
SupervisedUserService::ExtensionState SupervisedUserService::GetExtensionState(
    const Extension& extension) const {
  bool was_installed_by_default = extension.was_installed_by_default();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(https://crbug.com/1218633): Check if this is needed for extensions in
  // LaCrOS.
  // On Chrome OS all external sources are controlled by us so it means that
  // they are "default". Method was_installed_by_default returns false because
  // extensions creation flags are ignored in case of default extensions with
  // update URL(the flags aren't passed to OnExternalExtensionUpdateUrlFound).
  // TODO(dpolukhin): remove this Chrome OS specific code as soon as creation
  // flags are not ignored.
  was_installed_by_default =
      extensions::Manifest::IsExternalLocation(extension.location());
#endif

  // Note: Component extensions are protected from modification/uninstallation
  // anyway, so there's no need to enforce them again for supervised users.
  // Also, leave policy-installed extensions alone - they have their own
  // management; in particular we don't want to override the force-install list.
  if (extensions::Manifest::IsComponentLocation(extension.location()) ||
      extensions::Manifest::IsPolicyLocation(extension.location()) ||
      extension.is_theme() || extension.is_shared_module() ||
      was_installed_by_default) {
    return ExtensionState::ALLOWED;
  }

  if (base::Contains(kAllowlistExtensionIds, extension.id())) {
    return ExtensionState::ALLOWED;
  }

  if (ShouldBlockExtension(extension.id())) {
    return ExtensionState::BLOCKED;
  }

  if (base::Contains(approved_extensions_set_, extension.id())) {
    return ExtensionState::ALLOWED;
  }
  return ExtensionState::REQUIRE_APPROVAL;
}

bool SupervisedUserService::ShouldBlockExtension(
    const std::string& extension_id) const {
  if (GetSupervisedUserExtensionsMayRequestPermissionsPref()) {
    return false;
  }
  if (!ExtensionRegistry::Get(profile_)->GetInstalledExtension(extension_id)) {
    // Block child users from installing new extensions. Already installed
    // extensions should not be affected.
    return true;
  }
  if (ExtensionPrefs::Get(profile_)->DidExtensionEscalatePermissions(
          extension_id)) {
    // Block child users from approving existing extensions asking for
    // additional permissions.
    return true;
  }
  return false;
}

std::string SupervisedUserService::GetDebugPolicyProviderName() const {
  // Save the string space in official builds.
#if DCHECK_IS_ON()
  return "Supervised User Service";
#else
  base::ImmediateCrash();
#endif
}

bool SupervisedUserService::UserMayLoad(const Extension* extension,
                                        std::u16string* error) const {
  DCHECK(IsChild());
  ExtensionState result = GetExtensionState(*extension);
  bool may_load = result != ExtensionState::BLOCKED;
  if (!may_load && error)
    *error = GetExtensionsLockedMessage();
  return may_load;
}

bool SupervisedUserService::MustRemainDisabled(
    const Extension* extension,
    extensions::disable_reason::DisableReason* reason,
    std::u16string* error) const {
  DCHECK(IsChild());
  ExtensionState state = GetExtensionState(*extension);
  // Only extensions that require approval should be disabled.
  // Blocked extensions should be not loaded at all, and are taken care of
  // at UserMayLoad.
  bool must_remain_disabled = state == ExtensionState::REQUIRE_APPROVAL;

  if (!must_remain_disabled)
    return false;
  if (reason)
    *reason = extensions::disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED;
  if (error)
    *error = GetExtensionsLockedMessage();
  return true;
}

void SupervisedUserService::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    bool is_update) {
  // This callback method is responsible for updating extension state and
  // approved_extensions_set_ upon extension updates.
  if (!is_update)
    return;

  // Upon extension update, a change in extension state might be required.
  ChangeExtensionStateIfNecessary(extension->id());
}

void SupervisedUserService::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  RemoveExtensionApproval(*extension);
}

void SupervisedUserService::ChangeExtensionStateIfNecessary(
    const std::string& extension_id) {
  // If the profile is not supervised, do nothing.
  // TODO(crbug/1026900): SupervisedUserService should not be active if the
  // profile is not even supervised during browser tests, i.e. this check
  // shouldn't be needed.
  if (!active_)
    return;
  DCHECK(IsChild());
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  const Extension* extension = registry->GetInstalledExtension(extension_id);
  // If the extension is not installed (yet), do nothing.
  // Things will be handled after installation.
  if (!extension)
    return;

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile_);
  extensions::ExtensionService* service =
      ExtensionSystem::Get(profile_)->extension_service();

  ExtensionState state = GetExtensionState(*extension);
  switch (state) {
    // BLOCKED extensions should be already disabled and we don't need to change
    // their state here.
    case ExtensionState::BLOCKED:
      break;
    case ExtensionState::REQUIRE_APPROVAL:
      service->DisableExtension(
          extension_id,
          extensions::disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);
      break;
    case ExtensionState::ALLOWED:
      extension_prefs->RemoveDisableReason(
          extension_id,
          extensions::disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);
      // If not disabled for other reasons, enable it.
      if (extension_prefs->GetDisableReasons(extension_id) ==
          extensions::disable_reason::DISABLE_NONE) {
        service->EnableExtension(extension_id);
      }
      break;
  }
}

// TODO(crbug/1072857): We don't need the extension version information. It's
// only included for backwards compatibility with previous versions of Chrome.
// Remove the version information once a sufficient number of users have
// migrated away from M83.
void SupervisedUserService::UpdateApprovedExtension(
    const std::string& extension_id,
    const std::string& version,
    ApprovedExtensionChange type) {
  PrefService* pref_service = GetPrefService();
  ScopedDictPrefUpdate update(pref_service,
                              prefs::kSupervisedUserApprovedExtensions);
  base::Value::Dict& approved_extensions = update.Get();
  bool success = false;
  switch (type) {
    case ApprovedExtensionChange::kAdd:
      DCHECK(!approved_extensions.FindString(extension_id));
      approved_extensions.Set(extension_id, std::move(version));
      SupervisedUserExtensionsMetricsRecorder::RecordExtensionsUmaMetrics(
          SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
              kApprovalGranted);
      break;
    case ApprovedExtensionChange::kRemove:
      success = approved_extensions.Remove(extension_id);
      DCHECK(success);
      SupervisedUserExtensionsMetricsRecorder::RecordExtensionsUmaMetrics(
          SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
              kApprovalRemoved);
      break;
  }
}

void SupervisedUserService::RefreshApprovedExtensionsFromPrefs() {
  // Keep track of currently approved extensions. We need to disable them if
  // they are not in the approved set anymore.
  std::set<std::string> extensions_to_be_checked(
      std::move(approved_extensions_set_));

  // The purpose here is to re-populate the approved_extensions_set_, which is
  // used in GetExtensionState() to keep track of approved extensions.
  approved_extensions_set_.clear();

  // TODO(crbug/1072857): This dict is actually just a set. The extension
  // version information stored in the values is unnecessary. It is only there
  // for backwards compatibility. Remove the version information once sufficient
  // users have migrated away from M83.
  const base::Value::Dict& dict =
      profile_->GetPrefs()->GetDict(prefs::kSupervisedUserApprovedExtensions);
  for (auto it : dict) {
    approved_extensions_set_.insert(it.first);
    extensions_to_be_checked.insert(it.first);
  }

  for (const auto& extension_id : extensions_to_be_checked) {
    ChangeExtensionStateIfNecessary(extension_id);
  }
}

void SupervisedUserService::SetExtensionsActive() {
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile_);
  extensions::ManagementPolicy* management_policy =
      extension_system->management_policy();

  if (management_policy) {
    if (active_)
      management_policy->RegisterProvider(this);
    else
      management_policy->UnregisterProvider(this);

    // Re-check the policy to make sure any new settings get applied.
    extension_system->extension_service()->CheckManagementPolicy();
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

bool SupervisedUserService::IsCustomPassphraseAllowed() const {
  return !active_;
}

#if !BUILDFLAG(IS_ANDROID)
void SupervisedUserService::OnBrowserSetLastActive(Browser* browser) {
  bool profile_became_active = profile_->IsSameOrParent(browser->profile());
  if (!is_profile_active_ && profile_became_active)
    base::RecordAction(UserMetricsAction("ManagedUsers_OpenProfile"));
  else if (is_profile_active_ && !profile_became_active)
    base::RecordAction(UserMetricsAction("ManagedUsers_SwitchProfile"));

  is_profile_active_ = profile_became_active;
}
#endif  // !BUILDFLAG(IS_ANDROID)

void SupervisedUserService::OnSiteListUpdated() {
  for (SupervisedUserServiceObserver& observer : observer_list_)
    observer.OnURLFilterChanged();
}
