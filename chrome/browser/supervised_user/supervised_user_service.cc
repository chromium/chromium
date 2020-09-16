// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_service.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/user_metrics.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/supervised_user_whitelist_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/permission_request_creator.h"
#include "chrome/browser/supervised_user/supervised_user_allowlist_service.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_features.h"
#include "chrome/browser/supervised_user/supervised_user_filtering_switches.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_observer.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_site_list.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/browser/url_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/buildflags/buildflags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chromeos/settings/cros_settings_names.h"
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
    "https://www.gstatic.com/chrome/supervised_user/blacklist-20141001-1k.bin";
// The filename under which we'll store the denylist (in the user data dir).
const char kDenylistFilename[] = "su-blacklist.bin";

#if BUILDFLAG(ENABLE_EXTENSIONS)
// These extensions are allowed for supervised users for internal development
// purposes.
constexpr char const* kAllowlistExtensionIds[] = {
    "behllobkkfkfnphdnhnkndlbkcpglgmj"  // Tast extension.
};

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

const char* const kCustodianInfoPrefs[] = {
    prefs::kSupervisedUserCustodianName,
    prefs::kSupervisedUserCustodianEmail,
    prefs::kSupervisedUserCustodianObfuscatedGaiaId,
    prefs::kSupervisedUserCustodianProfileImageURL,
    prefs::kSupervisedUserCustodianProfileURL,
    prefs::kSupervisedUserSecondCustodianName,
    prefs::kSupervisedUserSecondCustodianEmail,
    prefs::kSupervisedUserSecondCustodianObfuscatedGaiaId,
    prefs::kSupervisedUserSecondCustodianProfileImageURL,
    prefs::kSupervisedUserSecondCustodianProfileURL,
};

void CreateURLAccessRequest(const GURL& url,
                            PermissionRequestCreator* creator,
                            SupervisedUserService::SuccessCallback callback) {
  creator->CreateURLAccessRequest(url, std::move(callback));
}

base::FilePath GetDenylistPath() {
  base::FilePath denylist_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &denylist_dir);
  return denylist_dir.AppendASCII(kDenylistFilename);
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
  for (const char* pref : kCustodianInfoPrefs) {
    registry->RegisterStringPref(pref, std::string());
  }
}

void SupervisedUserService::Init() {
  DCHECK(!did_init_);
  did_init_ = true;
  DCHECK(GetSettingsService()->IsReady());

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kSupervisedUserId,
      base::Bind(&SupervisedUserService::OnSupervisedUserIdChanged,
          base::Unretained(this)));

  allowlist_service_->AddSiteListsChangedCallback(
      base::Bind(&SupervisedUserService::OnSiteListsChanged,
                 weak_ptr_factory_.GetWeakPtr()));

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

SupervisedUserAllowlistService* SupervisedUserService::GetAllowlistService() {
  return allowlist_service_.get();
}

bool SupervisedUserService::AccessRequestsEnabled() {
  return FindEnabledPermissionRequestCreator(0) < permissions_creators_.size();
}

void SupervisedUserService::AddURLAccessRequest(const GURL& url,
                                                SuccessCallback callback) {
  GURL effective_url = policy::url_util::GetEmbeddedURL(url);
  if (!effective_url.is_valid())
    effective_url = url;
  AddPermissionRequestInternal(
      base::BindRepeating(CreateURLAccessRequest,
                          policy::url_util::Normalize(effective_url)),
      std::move(callback), 0);
}

// static
std::string SupervisedUserService::GetExtensionRequestId(
    const std::string& extension_id,
    const base::Version& version) {
  return base::StringPrintf("%s:%s", extension_id.c_str(),
                            version.GetString().c_str());
}

std::string SupervisedUserService::GetCustodianEmailAddress() const {
  std::string email = profile_->GetPrefs()->GetString(
      prefs::kSupervisedUserCustodianEmail);
#if defined(OS_CHROMEOS)
  // |GetActiveUser()| can return null in unit tests.
  if (email.empty() && !!user_manager::UserManager::Get()->GetActiveUser()) {
    email = chromeos::ChromeUserManager::Get()
                ->GetSupervisedUserManager()
                ->GetManagerDisplayEmail(user_manager::UserManager::Get()
                                             ->GetActiveUser()
                                             ->GetAccountId()
                                             .GetUserEmail());
  }
#endif
  return email;
}

std::string SupervisedUserService::GetCustodianObfuscatedGaiaId() const {
  return profile_->GetPrefs()->GetString(
      prefs::kSupervisedUserCustodianObfuscatedGaiaId);
}

std::string SupervisedUserService::GetCustodianName() const {
  std::string name = profile_->GetPrefs()->GetString(
      prefs::kSupervisedUserCustodianName);
#if defined(OS_CHROMEOS)
  // |GetActiveUser()| can return null in unit tests.
  if (name.empty() && !!user_manager::UserManager::Get()->GetActiveUser()) {
    name = base::UTF16ToUTF8(
        chromeos::ChromeUserManager::Get()
            ->GetSupervisedUserManager()
            ->GetManagerDisplayName(user_manager::UserManager::Get()
                                        ->GetActiveUser()
                                        ->GetAccountId()
                                        .GetUserEmail()));
  }
#endif
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

base::string16 SupervisedUserService::GetExtensionsLockedMessage() const {
  return l10n_util::GetStringFUTF16(IDS_EXTENSIONS_LOCKED_SUPERVISED_USER,
                                    base::UTF8ToUTF16(GetCustodianName()));
}

bool SupervisedUserService::IsSupervisedUserIframeFilterEnabled() const {
  return base::FeatureList::IsEnabled(
      supervised_users::kSupervisedUserIframeFilter);
}

// static
std::string SupervisedUserService::GetEduCoexistenceLoginUrl() {
  return base::FeatureList::IsEnabled(supervised_users::kEduCoexistenceFlowV2)
             ? chrome::kChromeUIEDUCoexistenceLoginURLV2
             : chrome::kChromeUIEDUCoexistenceLoginURLV1;
}

bool SupervisedUserService::IsChild() const {
  return profile_->IsChild();
}

bool SupervisedUserService::IsSupervisedUserExtensionInstallEnabled() const {
  return base::FeatureList::IsEnabled(
      supervised_users::kSupervisedUserInitiatedExtensionInstall);
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

void SupervisedUserService::AddPermissionRequestCreator(
    std::unique_ptr<PermissionRequestCreator> creator) {
  permissions_creators_.push_back(std::move(creator));
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
  registry_observer_.Add(extensions::ExtensionRegistry::Get(profile));
#endif

  std::string client_id = component_updater::SupervisedUserWhitelistInstaller::
      ClientIdForProfilePath(profile_->GetPath());
  allowlist_service_ = std::make_unique<SupervisedUserAllowlistService>(
      profile_->GetPrefs(),
      g_browser_process->supervised_user_whitelist_installer(), client_id);
}

void SupervisedUserService::SetPrimaryPermissionCreatorForTest(
    std::unique_ptr<PermissionRequestCreator> permission_creator) {
  if (permissions_creators_.empty()) {
    permissions_creators_.push_back(std::move(permission_creator));
    return;
  }

  // Else there are other permission creators.
  permissions_creators_.insert(permissions_creators_.begin(),
                               std::move(permission_creator));
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
  GetSettingsService()->SetLocalSetting(
      supervised_users::kGeolocationDisabled,
      std::make_unique<base::Value>(!enabled));
  profile_->GetPrefs()->SetBoolean(
      prefs::kSupervisedUserExtensionsMayRequestPermissions, enabled);
}

bool SupervisedUserService::CanInstallExtensions() const {
  return IsSupervisedUserExtensionInstallEnabled() && HasACustodian() &&
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

void SupervisedUserService::SetActive(bool active) {
  if (active_ == active)
    return;
  active_ = active;

  if (!delegate_ || !delegate_->SetActive(active_)) {
#if defined(OS_ANDROID)
    DCHECK(!active_);
#endif
  }

  // Now activate/deactivate anything not handled by the delegate yet.

#if !defined(OS_ANDROID)
  // Re-set the default theme to turn the SU theme on/off.
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile_);
  if (theme_service->UsingDefaultTheme() || theme_service->UsingSystemTheme())
    theme_service->UseDefaultTheme();
#endif

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
    for (const char* pref : kCustodianInfoPrefs) {
      pref_change_registrar_.Add(
          pref,
          base::BindRepeating(&SupervisedUserService::OnCustodianInfoChanged,
                              base::Unretained(this)));
    }

    // Initialize the filter.
    OnDefaultFilteringBehaviorChanged();
    OnSafeSitesSettingChanged();
    allowlist_service_->Init();
    UpdateManualHosts();
    UpdateManualURLs();

#if BUILDFLAG(ENABLE_EXTENSIONS)
    RefreshApprovedExtensionsFromPrefs();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if !defined(OS_ANDROID)
    // TODO(bauerb): Get rid of the platform-specific #ifdef here.
    // http://crbug.com/313377
    BrowserList::AddObserver(this);
#endif
  } else {
    permissions_creators_.clear();

    pref_change_registrar_.Remove(
        prefs::kDefaultSupervisedUserFilteringBehavior);
#if BUILDFLAG(ENABLE_EXTENSIONS)
    pref_change_registrar_.Remove(prefs::kSupervisedUserApprovedExtensions);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
    pref_change_registrar_.Remove(prefs::kSupervisedUserManualHosts);
    pref_change_registrar_.Remove(prefs::kSupervisedUserManualURLs);
    for (const char* pref : kCustodianInfoPrefs) {
      pref_change_registrar_.Remove(pref);
    }

    url_filter_.Clear();
    for (SupervisedUserServiceObserver& observer : observer_list_)
      observer.OnURLFilterChanged();

#if !defined(OS_ANDROID)
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

size_t SupervisedUserService::FindEnabledPermissionRequestCreator(
    size_t start) {
  for (size_t i = start; i < permissions_creators_.size(); ++i) {
    if (permissions_creators_[i]->IsEnabled())
      return i;
  }
  return permissions_creators_.size();
}

void SupervisedUserService::AddPermissionRequestInternal(
    const CreatePermissionRequestCallback& create_request,
    SuccessCallback callback,
    size_t index) {
  // Find a permission request creator that is enabled.
  size_t next_index = FindEnabledPermissionRequestCreator(index);
  if (next_index >= permissions_creators_.size()) {
    std::move(callback).Run(false);
    return;
  }

  create_request.Run(
      permissions_creators_[next_index].get(),
      base::BindOnce(&SupervisedUserService::OnPermissionRequestIssued,
                     weak_ptr_factory_.GetWeakPtr(), create_request,
                     std::move(callback), next_index));
}

void SupervisedUserService::OnPermissionRequestIssued(
    const CreatePermissionRequestCallback& create_request,
    SuccessCallback callback,
    size_t index,
    bool success) {
  if (success) {
    std::move(callback).Run(true);
    return;
  }

  AddPermissionRequestInternal(create_request, std::move(callback), index + 1);
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
}

void SupervisedUserService::OnSafeSitesSettingChanged() {
  bool use_denylist = supervised_users::IsSafeSitesDenylistEnabled(profile_);
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
}

void SupervisedUserService::UpdateAsyncUrlChecker() {
  int behavior_value = profile_->GetPrefs()->GetInteger(
      prefs::kDefaultSupervisedUserFilteringBehavior);
  SupervisedUserURLFilter::FilteringBehavior behavior =
      SupervisedUserURLFilter::BehaviorFromInt(behavior_value);

  bool use_online_check =
      supervised_users::IsSafeSitesOnlineCheckEnabled(profile_) ||
      behavior == SupervisedUserURLFilter::FilteringBehavior::BLOCK;

  if (use_online_check != url_filter_.HasAsyncURLChecker()) {
    if (use_online_check) {
      url_filter_.InitAsyncURLChecker(
          content::BrowserContext::GetDefaultStoragePartition(profile_)
              ->GetURLLoaderFactoryForBrowserProcess());
    } else {
      url_filter_.ClearAsyncURLChecker();
    }
  }
}

void SupervisedUserService::OnSiteListsChanged(
    const std::vector<scoped_refptr<SupervisedUserSiteList> >& site_lists) {
  allowlists_ = site_lists;
  url_filter_.LoadAllowlists(site_lists);
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
    return;
  }

  DCHECK(!denylist_downloader_);

  // Create traffic annotation tag.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("supervised_users_blacklist", R"(
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

  auto factory = content::BrowserContext::GetDefaultStoragePartition(profile_)
                     ->GetURLLoaderFactoryForBrowserProcess();
  denylist_downloader_.reset(new FileDownloader(
      url, path, false, std::move(factory),
      base::BindOnce(&SupervisedUserService::OnDenylistDownloadDone,
                     base::Unretained(this), path),
      traffic_annotation));
}

void SupervisedUserService::LoadDenylistFromFile(const base::FilePath& path) {
  DCHECK(denylist_state_ == DenylistLoadState::LOAD_STARTED);
  denylist_.ReadFromFile(path,
                         base::Bind(&SupervisedUserService::OnDenylistLoaded,
                                    base::Unretained(this)));
}

void SupervisedUserService::OnDenylistDownloadDone(
    const base::FilePath& path,
    FileDownloader::Result result) {
  DCHECK(denylist_state_ == DenylistLoadState::LOAD_STARTED);
  if (FileDownloader::IsSuccess(result)) {
    LoadDenylistFromFile(path);
  } else {
    LOG(WARNING) << "Denylist download failed";
    // TODO(treib): Retry downloading after some time?
  }
  denylist_downloader_.reset();
}

void SupervisedUserService::OnDenylistLoaded() {
  DCHECK(denylist_state_ == DenylistLoadState::LOAD_STARTED);
  denylist_state_ = DenylistLoadState::LOADED;
  UpdateDenylist();
}

void SupervisedUserService::UpdateDenylist() {
  bool use_denylist = supervised_users::IsSafeSitesDenylistEnabled(profile_);
  url_filter_.SetDenylist(use_denylist ? &denylist_ : nullptr);
  for (SupervisedUserServiceObserver& observer : observer_list_)
    observer.OnURLFilterChanged();
}

void SupervisedUserService::UpdateManualHosts() {
  const base::DictionaryValue* dict =
      profile_->GetPrefs()->GetDictionary(prefs::kSupervisedUserManualHosts);
  std::map<std::string, bool> host_map;
  for (auto it : dict->DictItems()) {
    bool allow = false;
    bool result = it.second.GetAsBoolean(&allow);
    DCHECK(result);
    host_map[it.first] = allow;
  }
  url_filter_.SetManualHosts(std::move(host_map));

  for (SupervisedUserServiceObserver& observer : observer_list_)
    observer.OnURLFilterChanged();
}

void SupervisedUserService::UpdateManualURLs() {
  const base::DictionaryValue* dict =
      profile_->GetPrefs()->GetDictionary(prefs::kSupervisedUserManualURLs);
  std::map<GURL, bool> url_map;
  for (auto it : dict->DictItems()) {
    bool allow = false;
    bool result = it.second.GetAsBoolean(&allow);
    DCHECK(result);
    url_map[GURL(it.first)] = allow;
  }
  url_filter_.SetManualURLs(std::move(url_map));

  for (SupervisedUserServiceObserver& observer : observer_list_)
    observer.OnURLFilterChanged();
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
#if defined(OS_CHROMEOS)
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
      extension.is_theme() || extension.from_bookmark() ||
      extension.is_shared_module() || was_installed_by_default) {
    return ExtensionState::ALLOWED;
  }

  if (base::Contains(kAllowlistExtensionIds, extension.id())) {
    return ExtensionState::ALLOWED;
  }

  // Feature flag for gating new behavior.
  if (!base::FeatureList::IsEnabled(
          supervised_users::kSupervisedUserInitiatedExtensionInstall)) {
    return ExtensionState::BLOCKED;
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
  IMMEDIATE_CRASH();
#endif
}

bool SupervisedUserService::UserMayLoad(const Extension* extension,
                                        base::string16* error) const {
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
    base::string16* error) const {
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
  DictionaryPrefUpdate update(pref_service,
                              prefs::kSupervisedUserApprovedExtensions);
  base::DictionaryValue* approved_extensions = update.Get();
  DCHECK(approved_extensions)
      << "kSupervisedUserApprovedExtensions pref not found";
  bool success = false;
  switch (type) {
    case ApprovedExtensionChange::kAdd:
      DCHECK(!approved_extensions->FindStringKey(extension_id));
      approved_extensions->SetStringKey(extension_id, std::move(version));
      SupervisedUserExtensionsMetricsRecorder::RecordExtensionsUmaMetrics(
          SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
              kApprovalGranted);
      break;
    case ApprovedExtensionChange::kRemove:
      success = approved_extensions->RemoveKey(extension_id);
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
  const base::DictionaryValue* dict = profile_->GetPrefs()->GetDictionary(
      prefs::kSupervisedUserApprovedExtensions);
  for (auto it : dict->DictItems()) {
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

bool SupervisedUserService::IsEncryptEverythingAllowed() const {
  return !active_;
}

#if !defined(OS_ANDROID)
void SupervisedUserService::OnBrowserSetLastActive(Browser* browser) {
  bool profile_became_active = profile_->IsSameOrParent(browser->profile());
  if (!is_profile_active_ && profile_became_active)
    base::RecordAction(UserMetricsAction("ManagedUsers_OpenProfile"));
  else if (is_profile_active_ && !profile_became_active)
    base::RecordAction(UserMetricsAction("ManagedUsers_SwitchProfile"));

  is_profile_active_ = profile_became_active;
}
#endif  // !defined(OS_ANDROID)

void SupervisedUserService::OnSiteListUpdated() {
  for (SupervisedUserServiceObserver& observer : observer_list_)
    observer.OnURLFilterChanged();
}
