// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_service.h"

#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/user_metrics.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/supervised_user_whitelist_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/supervised_user/experimental/supervised_user_filtering_switches.h"
#include "chrome/browser/supervised_user/permission_request_creator.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_features.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_observer.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_site_list.h"
#include "chrome/browser/supervised_user/supervised_user_whitelist_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/policy/core/browser/url_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/buildflags/buildflags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
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

// The URL from which to download a host blacklist if no local one exists yet.
const char kBlacklistURL[] =
    "https://www.gstatic.com/chrome/supervised_user/blacklist-20141001-1k.bin";
// The filename under which we'll store the blacklist (in the user data dir).
const char kBlacklistFilename[] = "su-blacklist.bin";

const char* const kCustodianInfoPrefs[] = {
  prefs::kSupervisedUserCustodianName,
  prefs::kSupervisedUserCustodianEmail,
  prefs::kSupervisedUserCustodianProfileImageURL,
  prefs::kSupervisedUserCustodianProfileURL,
  prefs::kSupervisedUserSecondCustodianName,
  prefs::kSupervisedUserSecondCustodianEmail,
  prefs::kSupervisedUserSecondCustodianProfileImageURL,
  prefs::kSupervisedUserSecondCustodianProfileURL,
};

void CreateURLAccessRequest(const GURL& url,
                            PermissionRequestCreator* creator,
                            SupervisedUserService::SuccessCallback callback) {
  creator->CreateURLAccessRequest(url, std::move(callback));
}

void CreateExtensionInstallRequest(
    const std::string& id,
    PermissionRequestCreator* creator,
    SupervisedUserService::SuccessCallback callback) {
  creator->CreateExtensionInstallRequest(id, std::move(callback));
}

void CreateExtensionUpdateRequest(
    const std::string& id,
    PermissionRequestCreator* creator,
    SupervisedUserService::SuccessCallback callback) {
  creator->CreateExtensionUpdateRequest(id, std::move(callback));
}

// Default callback for AddExtensionInstallRequest.
void ExtensionInstallRequestSent(const std::string& id, bool success) {
  VLOG_IF(1, !success) << "Failed sending install request for " << id;
}

// Default callback for AddExtensionUpdateRequest.
void ExtensionUpdateRequestSent(const std::string& id, bool success) {
  VLOG_IF(1, !success) << "Failed sending update request for " << id;
}

base::FilePath GetBlacklistPath() {
  base::FilePath blacklist_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &blacklist_dir);
  return blacklist_dir.AppendASCII(kBlacklistFilename);
}

}  // namespace

SupervisedUserService::~SupervisedUserService() {
  DCHECK(!did_init_ || did_shutdown_);
  url_filter_.RemoveObserver(this);
}

// static
void SupervisedUserService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kSupervisedUserApprovedExtensions);
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
  pref_change_registrar_.Add(
      prefs::kForceSessionSync,
      base::Bind(&SupervisedUserService::OnForceSessionSyncChanged,
                 base::Unretained(this)));

  browser_sync::ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  // Can be null in tests.
  if (sync_service)
    sync_service->AddPreferenceProvider(this);

  std::string client_id = component_updater::SupervisedUserWhitelistInstaller::
      ClientIdForProfilePath(profile_->GetPath());
  whitelist_service_.reset(new SupervisedUserWhitelistService(
      profile_->GetPrefs(),
      g_browser_process->supervised_user_whitelist_installer(), client_id));
  whitelist_service_->AddSiteListsChangedCallback(
      base::Bind(&SupervisedUserService::OnSiteListsChanged,
                 weak_ptr_factory_.GetWeakPtr()));

  SetActive(ProfileIsSupervised());
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

SupervisedUserWhitelistService* SupervisedUserService::GetWhitelistService() {
  return whitelist_service_.get();
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

void SupervisedUserService::ReportURL(const GURL& url,
                                      SuccessCallback callback) {
  if (url_reporter_)
    url_reporter_->ReportUrl(url, std::move(callback));
  else
    std::move(callback).Run(false);
}

void SupervisedUserService::AddExtensionInstallRequest(
    const std::string& extension_id,
    const base::Version& version,
    SuccessCallback callback) {
  std::string id = GetExtensionRequestId(extension_id, version);
  AddPermissionRequestInternal(
      base::BindRepeating(CreateExtensionInstallRequest, id),
      std::move(callback), 0);
}

void SupervisedUserService::AddExtensionInstallRequest(
    const std::string& extension_id,
    const base::Version& version) {
  std::string id = GetExtensionRequestId(extension_id, version);
  AddExtensionInstallRequest(extension_id, version,
                             base::BindOnce(ExtensionInstallRequestSent, id));
}

void SupervisedUserService::AddExtensionUpdateRequest(
    const std::string& extension_id,
    const base::Version& version,
    SuccessCallback callback) {
  std::string id = GetExtensionRequestId(extension_id, version);
  AddPermissionRequestInternal(
      base::BindRepeating(CreateExtensionUpdateRequest, id),
      std::move(callback), 0);
}

void SupervisedUserService::AddExtensionUpdateRequest(
    const std::string& extension_id,
    const base::Version& version) {
  std::string id = GetExtensionRequestId(extension_id, version);
  AddExtensionUpdateRequest(extension_id, version,
                            base::BindOnce(ExtensionUpdateRequestSent, id));
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

std::string SupervisedUserService::GetSecondCustodianName() const {
  std::string name = profile_->GetPrefs()->GetString(
      prefs::kSupervisedUserSecondCustodianName);
  return name.empty() ? GetSecondCustodianEmailAddress() : name;
}

base::string16 SupervisedUserService::GetExtensionsLockedMessage() const {
  return l10n_util::GetStringFUTF16(IDS_EXTENSIONS_LOCKED_SUPERVISED_USER,
                                    base::UTF8ToUTF16(GetCustodianName()));
}

#if !defined(OS_ANDROID)
void SupervisedUserService::InitSync(const std::string& refresh_token) {
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  token_service->UpdateCredentials(supervised_users::kSupervisedUserPseudoEmail,
                                   refresh_token);
}
#endif  // !defined(OS_ANDROID)

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

void SupervisedUserService::SetSafeSearchURLReporter(
    std::unique_ptr<SafeSearchURLReporter> reporter) {
  url_reporter_ = std::move(reporter);
}

bool SupervisedUserService::IncludesSyncSessionsType() const {
  return includes_sync_sessions_type_;
}

SupervisedUserService::SupervisedUserService(Profile* profile)
    : includes_sync_sessions_type_(true),
      profile_(profile),
      active_(false),
      delegate_(NULL),
      is_profile_active_(false),
      did_init_(false),
      did_shutdown_(false),
      blacklist_state_(BlacklistLoadState::NOT_LOADED),
#if BUILDFLAG(ENABLE_EXTENSIONS)
      registry_observer_(this),
#endif
      weak_ptr_factory_(this) {
  url_filter_.AddObserver(this);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  registry_observer_.Add(extensions::ExtensionRegistry::Get(profile));
#endif
}

void SupervisedUserService::SetActive(bool active) {
  if (active_ == active)
    return;
  active_ = active;

  if (!delegate_ || !delegate_->SetActive(active_)) {
    if (active_) {
#if !defined(OS_ANDROID)
      ProfileOAuth2TokenService* token_service =
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
      token_service->LoadCredentials(
          supervised_users::kSupervisedUserPseudoEmail);
#else
      NOTREACHED();
#endif
    }
  }

  // Now activate/deactivate anything not handled by the delegate yet.

#if !defined(OS_ANDROID)
  // Re-set the default theme to turn the SU theme on/off.
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile_);
  if (theme_service->UsingDefaultTheme() || theme_service->UsingSystemTheme())
    theme_service->UseDefaultTheme();
#endif

  browser_sync::ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  sync_service->SetEncryptEverythingAllowed(!active_);

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
        base::BindRepeating(&SupervisedUserService::UpdateApprovedExtensions,
                            base::Unretained(this)));
#endif
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
    whitelist_service_->Init();
    UpdateManualHosts();
    UpdateManualURLs();

#if BUILDFLAG(ENABLE_EXTENSIONS)
    UpdateApprovedExtensions();
#endif

#if !defined(OS_ANDROID)
    // TODO(bauerb): Get rid of the platform-specific #ifdef here.
    // http://crbug.com/313377
    BrowserList::AddObserver(this);
#endif
  } else {
    permissions_creators_.clear();
    url_reporter_.reset();

    pref_change_registrar_.Remove(
        prefs::kDefaultSupervisedUserFilteringBehavior);
#if BUILDFLAG(ENABLE_EXTENSIONS)
    pref_change_registrar_.Remove(prefs::kSupervisedUserApprovedExtensions);
#endif
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

bool SupervisedUserService::ProfileIsSupervised() const {
  return profile_->IsSupervised();
}

void SupervisedUserService::OnCustodianInfoChanged() {
  for (SupervisedUserServiceObserver& observer : observer_list_)
    observer.OnCustodianInfoChanged();
}

SupervisedUserSettingsService* SupervisedUserService::GetSettingsService() {
  return SupervisedUserSettingsServiceFactory::GetForProfile(profile_);
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
  SetActive(ProfileIsSupervised());
}

void SupervisedUserService::OnDefaultFilteringBehaviorChanged() {
  int behavior_value = profile_->GetPrefs()->GetInteger(
      prefs::kDefaultSupervisedUserFilteringBehavior);
  SupervisedUserURLFilter::FilteringBehavior behavior =
      SupervisedUserURLFilter::BehaviorFromInt(behavior_value);
  url_filter_.SetDefaultFilteringBehavior(behavior);

  for (SupervisedUserServiceObserver& observer : observer_list_)
    observer.OnURLFilterChanged();
}

void SupervisedUserService::OnSafeSitesSettingChanged() {
  bool use_blacklist = supervised_users::IsSafeSitesBlacklistEnabled(profile_);
  if (use_blacklist != url_filter_.HasBlacklist()) {
    if (use_blacklist && blacklist_state_ == BlacklistLoadState::NOT_LOADED) {
      LoadBlacklist(GetBlacklistPath(), GURL(kBlacklistURL));
    } else if (!use_blacklist ||
               blacklist_state_ == BlacklistLoadState::LOADED) {
      // Either the blacklist was turned off, or it was turned on but has
      // already been loaded previously. Just update the setting.
      UpdateBlacklist();
    }
    // Else: The blacklist was enabled, but the load is already in progress.
    // Do nothing - we'll check the setting again when the load finishes.
  }

  bool use_online_check =
      supervised_users::IsSafeSitesOnlineCheckEnabled(profile_);
  if (use_online_check != url_filter_.HasAsyncURLChecker()) {
    if (use_online_check)
      url_filter_.InitAsyncURLChecker(
          content::BrowserContext::GetDefaultStoragePartition(profile_)
              ->GetURLLoaderFactoryForBrowserProcess());
    else
      url_filter_.ClearAsyncURLChecker();
  }
}

void SupervisedUserService::OnSiteListsChanged(
    const std::vector<scoped_refptr<SupervisedUserSiteList> >& site_lists) {
  whitelists_ = site_lists;
  url_filter_.LoadWhitelists(site_lists);
}

void SupervisedUserService::LoadBlacklist(const base::FilePath& path,
                                          const GURL& url) {
  DCHECK(blacklist_state_ == BlacklistLoadState::NOT_LOADED);
  blacklist_state_ = BlacklistLoadState::LOAD_STARTED;
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&base::PathExists, path),
      base::BindOnce(&SupervisedUserService::OnBlacklistFileChecked,
                     weak_ptr_factory_.GetWeakPtr(), path, url));
}

void SupervisedUserService::OnBlacklistFileChecked(const base::FilePath& path,
                                                   const GURL& url,
                                                   bool file_exists) {
  DCHECK(blacklist_state_ == BlacklistLoadState::LOAD_STARTED);
  if (file_exists) {
    LoadBlacklistFromFile(path);
    return;
  }

  DCHECK(!blacklist_downloader_);

  // Create traffic annotation tag.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("supervised_users_blacklist", R"(
        semantics {
          sender: "Supervised Users"
          description:
            "Downloads a static blacklist consisting of hostname hashes of "
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
  blacklist_downloader_.reset(new FileDownloader(
      url, path, false, std::move(factory),
      base::BindOnce(&SupervisedUserService::OnBlacklistDownloadDone,
                     base::Unretained(this), path),
      traffic_annotation));
}

void SupervisedUserService::LoadBlacklistFromFile(const base::FilePath& path) {
  DCHECK(blacklist_state_ == BlacklistLoadState::LOAD_STARTED);
  blacklist_.ReadFromFile(
      path,
      base::Bind(&SupervisedUserService::OnBlacklistLoaded,
                 base::Unretained(this)));
}

void SupervisedUserService::OnBlacklistDownloadDone(
    const base::FilePath& path,
    FileDownloader::Result result) {
  DCHECK(blacklist_state_ == BlacklistLoadState::LOAD_STARTED);
  if (FileDownloader::IsSuccess(result)) {
    LoadBlacklistFromFile(path);
  } else {
    LOG(WARNING) << "Blacklist download failed";
    // TODO(treib): Retry downloading after some time?
  }
  blacklist_downloader_.reset();
}

void SupervisedUserService::OnBlacklistLoaded() {
  DCHECK(blacklist_state_ == BlacklistLoadState::LOAD_STARTED);
  blacklist_state_ = BlacklistLoadState::LOADED;
  UpdateBlacklist();
}

void SupervisedUserService::UpdateBlacklist() {
  bool use_blacklist = supervised_users::IsSafeSitesBlacklistEnabled(profile_);
  url_filter_.SetBlacklist(use_blacklist ? &blacklist_ : nullptr);
  for (SupervisedUserServiceObserver& observer : observer_list_)
    observer.OnURLFilterChanged();
}

void SupervisedUserService::UpdateManualHosts() {
  const base::DictionaryValue* dict =
      profile_->GetPrefs()->GetDictionary(prefs::kSupervisedUserManualHosts);
  std::map<std::string, bool> host_map;
  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    bool allow = false;
    bool result = it.value().GetAsBoolean(&allow);
    DCHECK(result);
    host_map[it.key()] = allow;
  }
  url_filter_.SetManualHosts(std::move(host_map));

  for (SupervisedUserServiceObserver& observer : observer_list_)
    observer.OnURLFilterChanged();
}

void SupervisedUserService::UpdateManualURLs() {
  const base::DictionaryValue* dict =
      profile_->GetPrefs()->GetDictionary(prefs::kSupervisedUserManualURLs);
  std::map<GURL, bool> url_map;
  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    bool allow = false;
    bool result = it.value().GetAsBoolean(&allow);
    DCHECK(result);
    url_map[GURL(it.key())] = allow;
  }
  url_filter_.SetManualURLs(std::move(url_map));

  for (SupervisedUserServiceObserver& observer : observer_list_)
    observer.OnURLFilterChanged();
}

void SupervisedUserService::OnForceSessionSyncChanged() {
  includes_sync_sessions_type_ =
      profile_->GetPrefs()->GetBoolean(prefs::kForceSessionSync);
  ProfileSyncServiceFactory::GetForProfile(profile_)
      ->ReconfigureDatatypeManager(/*bypass_setup_in_progress_check=*/false);
}

void SupervisedUserService::Shutdown() {
  if (!did_init_)
    return;
  DCHECK(!did_shutdown_);
  did_shutdown_ = true;
  if (ProfileIsSupervised()) {
    base::RecordAction(UserMetricsAction("ManagedUsers_QuitBrowser"));
  }
  SetActive(false);

  browser_sync::ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  // Can be null in tests.
  if (sync_service)
    sync_service->RemovePreferenceProvider(this);
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

  if (extensions::util::WasInstalledByCustodian(extension.id(), profile_))
    return ExtensionState::FORCED;

  if (!base::FeatureList::IsEnabled(
          supervised_users::kSupervisedUserInitiatedExtensionInstall)) {
    return ExtensionState::BLOCKED;
  }

  auto extension_it = approved_extensions_map_.find(extension.id());
  // If the installed version is approved, then the extension is allowed,
  // otherwise, it requires approval.
  if (extension_it != approved_extensions_map_.end() &&
      extension_it->second == extension.version()) {
    return ExtensionState::ALLOWED;
  }
  return ExtensionState::REQUIRE_APPROVAL;
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
  DCHECK(ProfileIsSupervised());
  ExtensionState result = GetExtensionState(*extension);
  bool may_load = result != ExtensionState::BLOCKED;
  if (!may_load && error)
    *error = GetExtensionsLockedMessage();
  return may_load;
}

bool SupervisedUserService::UserMayModifySettings(const Extension* extension,
                                                  base::string16* error) const {
  DCHECK(ProfileIsSupervised());
  ExtensionState result = GetExtensionState(*extension);
  // While the following check allows the supervised user to modify the settings
  // and enable or disable the extension, MustRemainDisabled properly takes care
  // of keeping an extension disabled when required.
  // For custodian-installed extensions, the state is always FORCED, even if
  // it's waiting for an update approval.
  bool may_modify = result != ExtensionState::FORCED;
  if (!may_modify && error)
    *error = GetExtensionsLockedMessage();
  return may_modify;
}

// Note: Having MustRemainInstalled always say "true" for custodian-installed
// extensions does NOT prevent remote uninstalls (which is a bit unexpected, but
// exactly what we want).
bool SupervisedUserService::MustRemainInstalled(const Extension* extension,
                                                base::string16* error) const {
  DCHECK(ProfileIsSupervised());
  ExtensionState result = GetExtensionState(*extension);
  bool may_not_uninstall = result == ExtensionState::FORCED;
  if (may_not_uninstall && error)
    *error = GetExtensionsLockedMessage();
  return may_not_uninstall;
}

bool SupervisedUserService::MustRemainDisabled(
    const Extension* extension,
    extensions::disable_reason::DisableReason* reason,
    base::string16* error) const {
  DCHECK(ProfileIsSupervised());
  ExtensionState state = GetExtensionState(*extension);
  // Only extensions that require approval should be disabled.
  // Blocked extensions should be not loaded at all, and are taken care of
  // at UserMayLoad.
  bool must_remain_disabled = state == ExtensionState::REQUIRE_APPROVAL;

  if (must_remain_disabled) {
    if (error)
      *error = GetExtensionsLockedMessage();
    // If the extension must remain disabled due to permission increase,
    // then the update request has been already sent at update time.
    // We do nothing and we don't add an extra disable reason.
    ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile_);
    if (extension_prefs->HasDisableReason(
            extension->id(),
            extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE)) {
      if (reason)
        *reason = extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE;
      return true;
    }
    if (reason)
      *reason = extensions::disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED;
    if (base::FeatureList::IsEnabled(
            supervised_users::kSupervisedUserInitiatedExtensionInstall)) {
      // If the Extension isn't pending a custodian approval already, send
      // an approval request.
      if (!extension_prefs->HasDisableReason(
              extension->id(), extensions::disable_reason::
                                   DISABLE_CUSTODIAN_APPROVAL_REQUIRED)) {
        // MustRemainDisabled is a const method and hence cannot call
        // AddExtensionInstallRequest directly.
        SupervisedUserService* supervised_user_service =
            SupervisedUserServiceFactory::GetForProfile(profile_);
        supervised_user_service->AddExtensionInstallRequest(
            extension->id(), extension->version());
      }
    }
  }
  return must_remain_disabled;
}

void SupervisedUserService::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    bool is_update) {
  // This callback method is responsible for updating extension state and
  // approved_extensions_map_ upon extension updates.
  if (!is_update)
    return;

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile_);
  const std::string& id = extension->id();
  const base::Version& version = extension->version();

  // If an already approved extension is updated without requiring
  // new permissions, we update the approved_version.
  if (!extension_prefs->HasDisableReason(
          id, extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE) &&
      approved_extensions_map_.count(id) > 0 &&
      approved_extensions_map_[id] < version) {
    approved_extensions_map_[id] = version;

    std::string key = SupervisedUserSettingsService::MakeSplitSettingKey(
        supervised_users::kApprovedExtensions, id);
    std::unique_ptr<base::Value> version_value(
        new base::Value(version.GetString()));
    GetSettingsService()->UpdateSetting(key, std::move(version_value));
  }
  // Upon extension update, the approved version may (or may not) match the
  // installed one. Therefore, a change in extension state might be required.
  ChangeExtensionStateIfNecessary(id);
}

void SupervisedUserService::UpdateApprovedExtensions() {
  const base::DictionaryValue* dict = profile_->GetPrefs()->GetDictionary(
      prefs::kSupervisedUserApprovedExtensions);
  // Keep track of currently approved extensions. We may need to disable them if
  // they are not in the approved map anymore.
  std::set<std::string> extensions_to_be_checked;
  for (const auto& extension : approved_extensions_map_)
    extensions_to_be_checked.insert(extension.first);

  approved_extensions_map_.clear();

  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    std::string version_str;
    bool result = it.value().GetAsString(&version_str);
    DCHECK(result);
    base::Version version(version_str);
    if (version.IsValid()) {
      approved_extensions_map_[it.key()] = version;
      extensions_to_be_checked.insert(it.key());
    } else {
      LOG(WARNING) << "Invalid version number " << version_str;
    }
  }

  for (const auto& extension_id : extensions_to_be_checked) {
    ChangeExtensionStateIfNecessary(extension_id);
  }
}

void SupervisedUserService::ChangeExtensionStateIfNecessary(
    const std::string& extension_id) {
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
    // BLOCKED/FORCED extensions should be already disabled/enabled
    // and we don't need to change their state here.
    case ExtensionState::BLOCKED:
    case ExtensionState::FORCED:
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
      extension_prefs->RemoveDisableReason(
          extension_id,
          extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE);
      // If not disabled for other reasons, enable it.
      if (extension_prefs->GetDisableReasons(extension_id) ==
          extensions::disable_reason::DISABLE_NONE) {
        service->EnableExtension(extension_id);
      }
      break;
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

syncer::ModelTypeSet SupervisedUserService::GetPreferredDataTypes() const {
  if (!ProfileIsSupervised())
    return syncer::ModelTypeSet();

  syncer::ModelTypeSet result;
  if (IncludesSyncSessionsType())
    result.Put(syncer::SESSIONS);
  result.Put(syncer::EXTENSIONS);
  result.Put(syncer::EXTENSION_SETTINGS);
  result.Put(syncer::APPS);
  result.Put(syncer::APP_SETTINGS);
  result.Put(syncer::APP_NOTIFICATIONS);
  result.Put(syncer::APP_LIST);
  return result;
}

#if !defined(OS_ANDROID)
void SupervisedUserService::OnBrowserSetLastActive(Browser* browser) {
  bool profile_became_active = profile_->IsSameProfile(browser->profile());
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
