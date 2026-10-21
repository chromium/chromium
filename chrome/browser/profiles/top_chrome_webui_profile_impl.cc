// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/top_chrome_webui_profile_impl.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/browser/developer_tools_availability.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/pref_service_syncable_factory.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/common/content_features.h"
#include "url/gurl.h"

namespace {

// Creates an independent, in-memory PrefService for TopChromeWebUIProfile.
std::unique_ptr<sync_preferences::PrefServiceSyncable>
CreateTopChromeWebUIPrefService() {
  sync_preferences::PrefServiceSyncableFactory factory;
  factory.set_user_prefs(base::MakeRefCounted<InMemoryPrefStore>());
  factory.SetAccountPrefStore(base::MakeRefCounted<InMemoryPrefStore>());

  auto pref_registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();
  // User-specific preferences must not be stored or shared in this Top Chrome
  // WebUI profile. ContentBrowserClient prefs are registered because they
  // are generic technical flags required to initialize the StoragePartition and
  // renderer processes hosting Top Chrome WebUIs.
  ChromeContentBrowserClient::RegisterProfilePrefs(pref_registry.get());
  // DevTools are disallowed unless debugging, since the renderers of this
  // profile are shared by the windows of all profiles, and DevTools could see
  // the data of all of them.
  pref_registry->RegisterIntegerPref(
      prefs::kDevToolsAvailability,
      static_cast<int>(
          base::FeatureList::IsEnabled(features::kDebugTopChromeWebUI)
              ? policy::DeveloperToolsAvailability::kAllowed
              : policy::DeveloperToolsAvailability::kDisallowed));
  pref_registry->RegisterListPref(prefs::kDeveloperToolsAvailabilityAllowlist);
  return factory.CreateSyncable(pref_registry);
}

}  // namespace

// Off the record so that content keeps its storage in memory.
// Profile::IsOffTheRecord() is derived from the presence of an OTRProfileID,
// so a unique ID is passed only for that purpose. Unlike other OTR profiles,
// this profile has no original profile: GetOriginalProfile() returns itself.
TopChromeWebUIProfileImpl::TopChromeWebUIProfileImpl()
    : TopChromeWebUIProfileImpl(
          OTRProfileID::CreateUnique("TopChromeWebUI::Profile")) {}

TopChromeWebUIProfileImpl::TopChromeWebUIProfileImpl(
    const OTRProfileID& otr_profile_id)
    : Profile(&otr_profile_id), start_time_(base::Time::Now()) {
  TRACE_EVENT0("browser,startup", "TopChromeWebUIProfileImpl::ctor");

  // No keyed service is created for this profile unless its factory explicitly
  // allows it.
  BrowserContextDependencyManager::GetInstance()
      ->DisableBrowserContextServicesByDefault(this);

  prefs_ = CreateTopChromeWebUIPrefService();
  user_prefs::UserPrefs::Set(this, prefs_.get());
  profile_metrics::SetBrowserProfileType(
      this, profile_metrics::BrowserProfileType::kTopChromeWebUIProfile);
}

TopChromeWebUIProfileImpl::~TopChromeWebUIProfileImpl() {
  MaybeSendDestroyedNotification();

  // No service is created for this profile, but the context still has to be
  // unregistered from the dependency manager.
  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      this);

  ShutdownStoragePartitions();
}

std::string TopChromeWebUIProfileImpl::GetProfileUserName() const {
  return std::string();
}

Profile* TopChromeWebUIProfileImpl::GetOffTheRecordProfile(
    const OTRProfileID& otr_profile_id,
    bool create_if_needed) {
  return nullptr;
}

std::vector<Profile*> TopChromeWebUIProfileImpl::GetAllOffTheRecordProfiles() {
  return {};
}

void TopChromeWebUIProfileImpl::DestroyOffTheRecordProfile(
    Profile* otr_profile) {}

bool TopChromeWebUIProfileImpl::HasOffTheRecordProfile(
    const OTRProfileID& otr_profile_id) {
  return false;
}

bool TopChromeWebUIProfileImpl::HasAnyOffTheRecordProfile() {
  return false;
}

Profile* TopChromeWebUIProfileImpl::GetOriginalProfile() {
  return this;
}

const Profile* TopChromeWebUIProfileImpl::GetOriginalProfile() const {
  return this;
}

bool TopChromeWebUIProfileImpl::IsChild() const {
  return false;
}

ExtensionSpecialStoragePolicy*
TopChromeWebUIProfileImpl::GetExtensionSpecialStoragePolicy() {
  return nullptr;
}

PrefService* TopChromeWebUIProfileImpl::GetPrefs() {
  return prefs_.get();
}

const PrefService* TopChromeWebUIProfileImpl::GetPrefs() const {
  return prefs_.get();
}

policy::SchemaRegistryService*
TopChromeWebUIProfileImpl::GetPolicySchemaRegistryService() {
  return nullptr;
}

#if BUILDFLAG(IS_CHROMEOS)
policy::UserCloudPolicyManagerAsh*
TopChromeWebUIProfileImpl::GetUserCloudPolicyManagerAsh() {
  NOTREACHED();
}
#else
policy::UserCloudPolicyManager*
TopChromeWebUIProfileImpl::GetUserCloudPolicyManager() {
  NOTREACHED();
}

policy::ProfileCloudPolicyManager*
TopChromeWebUIProfileImpl::GetProfileCloudPolicyManager() {
  NOTREACHED();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

policy::CloudPolicyManager* TopChromeWebUIProfileImpl::GetCloudPolicyManager() {
  NOTREACHED();
}

bool TopChromeWebUIProfileImpl::IsSameOrParent(Profile* profile) {
  return profile == this;
}

base::Time TopChromeWebUIProfileImpl::GetStartTime() const {
  return start_time_;
}

ProfileKey* TopChromeWebUIProfileImpl::GetProfileKey() const {
  // This profile has no SimpleKeyedServices, so it has no key.
  return nullptr;
}

policy::ProfilePolicyConnector*
TopChromeWebUIProfileImpl::GetProfilePolicyConnector() {
  return nullptr;
}

const policy::ProfilePolicyConnector*
TopChromeWebUIProfileImpl::GetProfilePolicyConnector() const {
  return nullptr;
}

base::FilePath TopChromeWebUIProfileImpl::last_selected_directory() {
  return base::FilePath();
}

void TopChromeWebUIProfileImpl::set_last_selected_directory(
    const base::FilePath& path) {}

bool TopChromeWebUIProfileImpl::WasCreatedByVersionOrLater(
    const std::string& version) {
  return true;
}

#if BUILDFLAG(IS_CHROMEOS)
void TopChromeWebUIProfileImpl::ChangeAppLocale(const std::string& locale,
                                                AppLocaleChangedVia) {}

void TopChromeWebUIProfileImpl::OnLogin() {}

void TopChromeWebUIProfileImpl::InitChromeOSPreferences() {}
#endif  // BUILDFLAG(IS_CHROMEOS)

bool TopChromeWebUIProfileImpl::IsNewProfile() const {
  return false;
}

GURL TopChromeWebUIProfileImpl::GetHomePage() {
  return GURL();
}

void TopChromeWebUIProfileImpl::SetCreationTimeForTesting(
    base::Time creation_time) {
  start_time_ = creation_time;
}

base::FilePath TopChromeWebUIProfileImpl::GetPath() const {
  // This profile keeps no data on disk.
  return base::FilePath();
}

base::Time TopChromeWebUIProfileImpl::GetCreationTime() const {
  return start_time_;
}

std::unique_ptr<content::ZoomLevelDelegate>
TopChromeWebUIProfileImpl::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return nullptr;
}

scoped_refptr<base::SequencedTaskRunner>
TopChromeWebUIProfileImpl::GetIOTaskRunner() {
  return base::SequencedTaskRunner::GetCurrentDefault();
}

content::DownloadManagerDelegate*
TopChromeWebUIProfileImpl::GetDownloadManagerDelegate() {
  return nullptr;
}

content::BrowserPluginGuestManager*
TopChromeWebUIProfileImpl::GetGuestManager() {
  return nullptr;
}

storage::SpecialStoragePolicy*
TopChromeWebUIProfileImpl::GetSpecialStoragePolicy() {
  return nullptr;
}

content::PlatformNotificationService*
TopChromeWebUIProfileImpl::GetPlatformNotificationService() {
  return nullptr;
}

content::PushMessagingService*
TopChromeWebUIProfileImpl::GetPushMessagingService() {
  return nullptr;
}

content::StorageNotificationService*
TopChromeWebUIProfileImpl::GetStorageNotificationService() {
  return nullptr;
}

content::SSLHostStateDelegate*
TopChromeWebUIProfileImpl::GetSSLHostStateDelegate() {
  return nullptr;
}

content::PermissionControllerDelegate*
TopChromeWebUIProfileImpl::GetPermissionControllerDelegate() {
  return nullptr;
}

content::ClientHintsControllerDelegate*
TopChromeWebUIProfileImpl::GetClientHintsControllerDelegate() {
  return nullptr;
}

content::BackgroundFetchDelegate*
TopChromeWebUIProfileImpl::GetBackgroundFetchDelegate() {
  return nullptr;
}

content::BackgroundSyncController*
TopChromeWebUIProfileImpl::GetBackgroundSyncController() {
  return nullptr;
}

content::BrowsingDataRemoverDelegate*
TopChromeWebUIProfileImpl::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

content::ReduceAcceptLanguageControllerDelegate*
TopChromeWebUIProfileImpl::GetReduceAcceptLanguageControllerDelegate() {
  return nullptr;
}

std::unique_ptr<media::VideoDecodePerfHistory>
TopChromeWebUIProfileImpl::CreateVideoDecodePerfHistory() {
  return nullptr;
}

content::FileSystemAccessPermissionContext*
TopChromeWebUIProfileImpl::GetFileSystemAccessPermissionContext() {
  return nullptr;
}

void TopChromeWebUIProfileImpl::RecordPrimaryMainFrameNavigation() {}

content::FederatedIdentityPermissionContextDelegate*
TopChromeWebUIProfileImpl::GetFederatedIdentityPermissionContext() {
  return nullptr;
}

content::FederatedIdentityApiPermissionContextDelegate*
TopChromeWebUIProfileImpl::GetFederatedIdentityApiPermissionContext() {
  return nullptr;
}

content::FederatedIdentityAutoReauthnPermissionContextDelegate*
TopChromeWebUIProfileImpl::GetFederatedIdentityAutoReauthnPermissionContext() {
  return nullptr;
}

#if BUILDFLAG(IS_WIN)
void TopChromeWebUIProfileImpl::AckCrashForTracking() {}
#endif

bool TopChromeWebUIProfileImpl::IsSignedIn() {
  return false;
}
