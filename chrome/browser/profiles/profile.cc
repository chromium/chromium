// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include <string>

#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/sync/base/sync_prefs.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/buildflags/buildflags.h"

#if defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/chromeos_switches.h"
#endif

#if !defined(OS_ANDROID)
#include "content/public/browser/host_zoom_map.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_pref_store.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/pref_names.h"
#endif

#if DCHECK_IS_ON()

#include <set>
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"

namespace {

base::LazyInstance<base::Lock>::Leaky g_profile_instances_lock =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<std::set<content::BrowserContext*>>::Leaky
    g_profile_instances = LAZY_INSTANCE_INITIALIZER;

}  // namespace

#endif  // DCHECK_IS_ON()

Profile::Profile()
    : restored_last_session_(false),
      sent_destroyed_notification_(false),
      accessibility_pause_level_(0),
      is_guest_profile_(false),
      is_system_profile_(false) {
#if DCHECK_IS_ON()
  base::AutoLock lock(g_profile_instances_lock.Get());
  g_profile_instances.Get().insert(this);
#endif  // DCHECK_IS_ON()

  BrowserContextDependencyManager::GetInstance()->MarkBrowserContextLive(this);
}

Profile::~Profile() {
#if DCHECK_IS_ON()
  base::AutoLock lock(g_profile_instances_lock.Get());
  g_profile_instances.Get().erase(this);
#endif  // DCHECK_IS_ON()
}

// static
Profile* Profile::FromBrowserContext(content::BrowserContext* browser_context) {
  if (!browser_context)
    return nullptr;

  // For code running in a chrome/ environment, it is safe to cast to Profile*
  // because Profile is the only implementation of BrowserContext used. In
  // testing, however, there are several BrowserContext subclasses that are not
  // Profile subclasses, and we can catch them. http://crbug.com/725276
#if DCHECK_IS_ON()
  base::AutoLock lock(g_profile_instances_lock.Get());
  if (!g_profile_instances.Get().count(browser_context)) {
    DCHECK(false)
        << "Non-Profile BrowserContext passed to Profile::FromBrowserContext! "
           "If you have a test linked in chrome/ you need a chrome/ based test "
           "class such as TestingProfile in chrome/test/base/testing_profile.h "
           "or you need to subclass your test class from Profile, not from "
           "BrowserContext.";
  }
#endif  // DCHECK_IS_ON()
  return static_cast<Profile*>(browser_context);
}

// static
Profile* Profile::FromWebUI(content::WebUI* web_ui) {
  return FromBrowserContext(web_ui->GetWebContents()->GetBrowserContext());
}

TestingProfile* Profile::AsTestingProfile() {
  return nullptr;
}

#if !defined(OS_ANDROID)
ChromeZoomLevelPrefs* Profile::GetZoomLevelPrefs() {
  return nullptr;
}
#endif  // !defined(OS_ANDROID)

PrefService* Profile::GetReadOnlyOffTheRecordPrefs() {
  return GetOffTheRecordPrefs();
}

Profile::Delegate::~Delegate() {
}

// static
const char Profile::kProfileKey[] = "__PROFILE__";

// static
void Profile::RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kSearchSuggestEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#if defined(OS_ANDROID)
  registry->RegisterStringPref(
      prefs::kContextualSearchEnabled,
      std::string(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif  // defined(OS_ANDROID)
  registry->RegisterBooleanPref(prefs::kSessionExitedCleanly, true);
  registry->RegisterStringPref(prefs::kSessionExitType, std::string());
  registry->RegisterInt64Pref(prefs::kSiteEngagementLastUpdateTime, 0,
                              PrefRegistry::LOSSY_PREF);
  registry->RegisterBooleanPref(prefs::kSSLErrorOverrideAllowed, true);
  registry->RegisterBooleanPref(prefs::kDisableExtensions, false);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  registry->RegisterBooleanPref(extensions::pref_names::kAlertsInitialized,
                                false);
#endif
  base::FilePath home;
  base::PathService::Get(base::DIR_HOME, &home);
  registry->RegisterStringPref(prefs::kSelectFileLastDirectory,
                               home.MaybeAsASCII());
#if !defined(OS_ANDROID)
  registry->RegisterDictionaryPref(prefs::kPartitionDefaultZoomLevel);
  registry->RegisterDictionaryPref(prefs::kPartitionPerHostZoomLevels);
#endif  // !defined(OS_ANDROID)
  registry->RegisterStringPref(prefs::kDefaultApps, "install");
  registry->RegisterBooleanPref(prefs::kSpeechRecognitionFilterProfanities,
                                true);
  registry->RegisterIntegerPref(prefs::kProfileIconVersion, 0);
  registry->RegisterBooleanPref(prefs::kAllowDinosaurEasterEgg, true);
#if defined(OS_CHROMEOS)
  // TODO(dilmah): For OS_CHROMEOS we maintain kApplicationLocale in both
  // local state and user's profile.  For other platforms we maintain
  // kApplicationLocale only in local state.
  // In the future we may want to maintain kApplicationLocale
  // in user's profile for other platforms as well.
  registry->RegisterStringPref(
      language::prefs::kApplicationLocale, std::string(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterStringPref(prefs::kApplicationLocaleBackup, std::string());
  registry->RegisterStringPref(prefs::kApplicationLocaleAccepted,
                               std::string());
#endif

  registry->RegisterBooleanPref(prefs::kDataSaverEnabled, false);
  data_reduction_proxy::RegisterSyncableProfilePrefs(registry);

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  // Preferences related to the avatar bubble and user manager tutorials.
  registry->RegisterIntegerPref(prefs::kProfileAvatarTutorialShown, 0);
#endif

#if defined(OS_ANDROID)
  registry->RegisterBooleanPref(prefs::kClickedUpdateMenuItem, false);
  registry->RegisterStringPref(prefs::kLatestVersionWhenClickedUpdateMenuItem,
                               std::string());
#endif

  registry->RegisterBooleanPref(
      prefs::kMediaRouterCloudServicesPrefSet,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kMediaRouterEnableCloudServices,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kMediaRouterFirstRunFlowAcknowledged,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kMediaRouterMediaRemotingEnabled, true);
  registry->RegisterListPref(prefs::kMediaRouterTabMirroringSources);

  registry->RegisterDictionaryPref(prefs::kWebShareVisitedTargets);
  registry->RegisterDictionaryPref(prefs::kExcludedSchemes);

  // Instead of registering new prefs here, please create a static method and
  // invoke it from RegisterProfilePrefs() in
  // chrome/browser/prefs/browser_prefs.cc.
}

std::string Profile::GetDebugName() {
  std::string name = GetPath().BaseName().MaybeAsASCII();
  if (name.empty()) {
    name = "UnknownProfile";
  }
  return name;
}

bool Profile::IsGuestSession() const {
#if defined(OS_CHROMEOS)
  static bool is_guest_session =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kGuestSession);
  return is_guest_session;
#else
  return is_guest_profile_;
#endif
}

bool Profile::IsSystemProfile() const {
  return is_system_profile_;
}

bool Profile::ShouldRestoreOldSessionCookies() {
  return false;
}

bool Profile::ShouldPersistSessionCookies() {
  return false;
}

network::mojom::NetworkContextPtr Profile::CreateNetworkContext(
    bool in_memory,
    const base::FilePath& relative_partition_path) {
  return ProfileNetworkContextServiceFactory::GetForContext(this)
      ->CreateNetworkContext(in_memory, relative_partition_path);
}

bool Profile::IsNewProfile() {
  // The profile has been shut down if the prefs were loaded from disk, unless
  // first-run autoimport wrote them and reloaded the pref service.
  // TODO(crbug.com/660346): revisit this when crbug.com/22142 (unifying the
  // profile import code) is fixed.
  return GetOriginalProfile()->GetPrefs()->GetInitializationStatus() ==
      PrefService::INITIALIZATION_STATUS_CREATED_NEW_PREF_STORE;
}

bool Profile::IsSyncAllowed() {
  if (ProfileSyncServiceFactory::HasProfileSyncService(this)) {
    browser_sync::ProfileSyncService* sync_service =
        ProfileSyncServiceFactory::GetForProfile(this);
    return !sync_service->HasDisableReason(
               syncer::SyncService::DISABLE_REASON_PLATFORM_OVERRIDE) &&
           !sync_service->HasDisableReason(
               syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
  }

  // No ProfileSyncService created yet - we don't want to create one, so just
  // infer the accessible state by looking at prefs/command line flags.
  syncer::SyncPrefs prefs(GetPrefs());
  return browser_sync::ProfileSyncService::IsSyncAllowedByFlag() &&
         !prefs.IsManaged();
}

void Profile::MaybeSendDestroyedNotification() {
  if (!sent_destroyed_notification_) {
    sent_destroyed_notification_ = true;

    NotifyWillBeDestroyed(this);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_DESTROYED,
        content::Source<Profile>(this),
        content::NotificationService::NoDetails());
  }
}

PrefStore* Profile::CreateExtensionPrefStore(Profile* profile,
                                             bool incognito_pref_store) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return new ExtensionPrefStore(
      ExtensionPrefValueMapFactory::GetForBrowserContext(profile),
      incognito_pref_store);
#else
  return nullptr;
#endif
}

bool ProfileCompare::operator()(Profile* a, Profile* b) const {
  DCHECK(a && b);
  if (a->IsSameProfile(b))
    return false;
  return a->GetOriginalProfile() < b->GetOriginalProfile();
}

#if !defined(OS_ANDROID)
double Profile::GetDefaultZoomLevelForProfile() {
  return GetDefaultStoragePartition(this)
      ->GetHostZoomMap()
      ->GetDefaultZoomLevel();
}
#endif  // !defined(OS_ANDROID)
