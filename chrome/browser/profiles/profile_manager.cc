// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_manager.h"

#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/deferred_sequenced_task_runner.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/trace_event.h"
#include "base/util/values/values_util.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/accessibility_labels_service.h"
#include "chrome/browser/accessibility/accessibility_labels_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/account_manager/child_account_type_changed_user_data.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/lite_video/lite_video_keyed_service.h"
#include "chrome/browser/lite_video/lite_video_keyed_service_factory.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/permissions/adaptive_quiet_notification_permission_ui_enabler.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/bookmark_model_loaded_observer.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_id/account_id.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search_engines/default_search_manager.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/stop_source.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_service_factory.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/metrics/android_profile_session_durations_service_factory.h"
#else
#include "chrome/browser/accessibility/caption_controller.h"
#include "chrome/browser/accessibility/caption_controller_factory.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/account_manager/account_manager_policy_controller_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/session/arc_supervision_transition.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#endif

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
#include "chrome/browser/profiles/profile_statistics.h"
#include "chrome/browser/profiles/profile_statistics_factory.h"
#endif

#if defined(OS_WIN) && BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/signin_util_win.h"
#endif  // defined(OS_WIN) && BUILDFLAG(ENABLE_DICE_SUPPORT)

using base::UserMetricsAction;
using content::BrowserThread;

namespace {

// Profile deletion can pass through two stages:
enum class ProfileDeletionStage {
  // At SCHEDULING stage some actions are made before profile deletion,
  // where one of them is the closure of browser windows. Deletion is cancelled
  // if the user choose explicitly not to close any of the tabs.
  SCHEDULING,
  // At MARKED stage profile can be safely removed from disk.
  MARKED
};
using ProfileDeletionMap = std::map<base::FilePath, ProfileDeletionStage>;
ProfileDeletionMap& ProfilesToDelete() {
  static base::NoDestructor<ProfileDeletionMap> profiles_to_delete;
  return *profiles_to_delete;
}

int64_t ComputeFilesSize(const base::FilePath& directory,
                         const base::FilePath::StringType& pattern) {
  int64_t running_size = 0;
  base::FileEnumerator iter(directory, false, base::FileEnumerator::FILES,
                            pattern);
  while (!iter.Next().empty())
    running_size += iter.GetInfo().GetSize();
  return running_size;
}

// Simple task to log the size of the current profile.
void ProfileSizeTask(const base::FilePath& path, int enabled_app_count) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  const int64_t kBytesInOneMB = 1024 * 1024;

  int64_t size = ComputeFilesSize(path, FILE_PATH_LITERAL("*"));
  int size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.TotalSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("History"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.HistorySize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("History*"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.TotalHistorySize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Cookies"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.CookiesSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Bookmarks"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.BookmarksSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Favicons"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.FaviconsSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Top Sites"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.TopSitesSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Visited Links"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.VisitedLinksSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Web Data"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.WebDataSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Extension*"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.ExtensionSize", size_MB);

  // Count number of enabled apps in this profile, if we know.
  if (enabled_app_count != -1)
    UMA_HISTOGRAM_COUNTS_10000("Profile.AppCount", enabled_app_count);
}

#if !defined(OS_ANDROID)
// Schedule a profile for deletion if it isn't already scheduled.
// Returns whether the profile has been newly scheduled.
bool ScheduleProfileDirectoryForDeletion(const base::FilePath& path) {
  if (base::Contains(ProfilesToDelete(), path))
    return false;
  ProfilesToDelete()[path] = ProfileDeletionStage::SCHEDULING;
  return true;
}

void MarkProfileDirectoryForDeletion(const base::FilePath& path) {
  DCHECK(!base::Contains(ProfilesToDelete(), path) ||
         ProfilesToDelete()[path] == ProfileDeletionStage::SCHEDULING);
  ProfilesToDelete()[path] = ProfileDeletionStage::MARKED;
  // Remember that this profile was deleted and files should have been deleted
  // on shutdown. In case of a crash remaining files are removed on next start.
  ListPrefUpdate deleted_profiles(g_browser_process->local_state(),
                                  prefs::kProfilesDeleted);
  deleted_profiles->Append(util::FilePathToValue(path));
}

// Cancel a scheduling deletion, so ScheduleProfileDirectoryForDeletion can be
// called again successfully.
void CancelProfileDeletion(const base::FilePath& path) {
  DCHECK(!base::Contains(ProfilesToDelete(), path) ||
         ProfilesToDelete()[path] == ProfileDeletionStage::SCHEDULING);
  ProfilesToDelete().erase(path);
  ProfileMetrics::LogProfileDeleteUser(ProfileMetrics::DELETE_PROFILE_ABORTED);
}
#endif

bool IsProfileDirectoryMarkedForDeletion(const base::FilePath& profile_path) {
  auto it = ProfilesToDelete().find(profile_path);
  return it != ProfilesToDelete().end() &&
         it->second == ProfileDeletionStage::MARKED;
}

// Physically remove deleted profile directories from disk.
void NukeProfileFromDisk(const base::FilePath& profile_path) {
  // Delete both the profile directory and its corresponding cache.
  base::FilePath cache_path;
  chrome::GetUserCacheDirectory(profile_path, &cache_path);
  base::DeletePathRecursively(profile_path);
  base::DeletePathRecursively(cache_path);
}

// Called after a deleted profile was checked and cleaned up.
void ProfileCleanedUp(const base::Value* profile_path_value) {
  ListPrefUpdate deleted_profiles(g_browser_process->local_state(),
                                  prefs::kProfilesDeleted);
  deleted_profiles->Remove(*profile_path_value, nullptr);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)

// Returns the number of installed (and enabled) apps, excluding any component
// apps.
size_t GetEnabledAppCount(Profile* profile) {
  size_t installed_apps = 0u;
  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  for (extensions::ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end();
       ++iter) {
    if ((*iter)->is_app() &&
        (*iter)->location() != extensions::Manifest::COMPONENT) {
      ++installed_apps;
    }
  }
  return installed_apps;
}

#endif  // ENABLE_EXTENSIONS

// Once a profile is loaded through LoadProfile this method is executed.
// It will then run |client_callback| with the right profile or null if it was
// unable to load it.
// It might get called more than once with different values of
// |status| but only once the profile is fully initialized will
// |client_callback| be run.
void OnProfileLoaded(ProfileManager::ProfileLoadedCallback client_callback,
                     bool incognito,
                     Profile* profile,
                     Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_CREATED) {
    // This is an intermediate state where the profile has been created, but is
    // not yet initialized. Ignore this and wait for the next state change.
    return;
  }
  if (status != Profile::CREATE_STATUS_INITIALIZED) {
    LOG(WARNING) << "Profile not loaded correctly";
    std::move(client_callback).Run(nullptr);
    return;
  }
  DCHECK(profile);
  std::move(client_callback)
      .Run(incognito ? profile->GetPrimaryOTRProfile() : profile);
}

#if !defined(OS_ANDROID)
// Helper function for ScheduleForcedEphemeralProfileForDeletion.
bool IsProfileEphemeral(ProfileAttributesStorage* storage,
                        const base::FilePath& profile_dir) {
  ProfileAttributesEntry* entry = nullptr;
  return storage->GetProfileAttributesWithPath(profile_dir, &entry) &&
         entry->IsEphemeral();
}
#endif

// Helper function that deletes entries from the kProfilesLastActive pref list.
// It is called when every ephemeral profile is handled.
void RemoveFromLastActiveProfilesPrefList(base::FilePath path) {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  ListPrefUpdate update(local_state, prefs::kProfilesLastActive);
  base::ListValue* profile_list = update.Get();
  base::Value entry_value = base::Value(path.BaseName().MaybeAsASCII());
  profile_list->EraseListValue(entry_value);
}

#if defined(OS_CHROMEOS)
bool IsLoggedIn() {
  return user_manager::UserManager::IsInitialized() &&
         user_manager::UserManager::Get()->IsUserLoggedIn();
}
#endif

}  // namespace

ProfileManager::ProfileManager(const base::FilePath& user_data_dir)
    : user_data_dir_(user_data_dir) {
  registrar_.Add(this, chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED,
                 content::NotificationService::AllSources());

  if (ProfileShortcutManager::IsFeatureEnabled() && !user_data_dir_.empty())
    profile_shortcut_manager_ = ProfileShortcutManager::Create(this);
}

ProfileManager::~ProfileManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
// static
void ProfileManager::ShutdownSessionServices() {
  ProfileManager* pm = g_browser_process->profile_manager();
  if (!pm)  // Is nullptr when running unit tests.
    return;
  std::vector<Profile*> profiles(pm->GetLoadedProfiles());
  for (size_t i = 0; i < profiles.size(); ++i)
    SessionServiceFactory::ShutdownForProfile(profiles[i]);
}
#endif

// static
void ProfileManager::NukeDeletedProfilesFromDisk() {
  for (const auto& item : ProfilesToDelete()) {
    if (item.second == ProfileDeletionStage::MARKED)
      NukeProfileFromDisk(item.first);
  }
  ProfilesToDelete().clear();
}

// static
Profile* ProfileManager::GetLastUsedProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetLastUsedProfile(profile_manager->user_data_dir_);
}

// static
Profile* ProfileManager::GetLastUsedProfileAllowedByPolicy() {
  Profile* profile = GetLastUsedProfile();
  if (!profile)
    return nullptr;
  if (IsOffTheRecordModeForced(profile))
    return profile->GetPrimaryOTRProfile();
  return profile;
}

// static
bool ProfileManager::IsOffTheRecordModeForced(Profile* profile) {
  return profile->IsGuestSession() ||
         profile->IsSystemProfile() ||
         IncognitoModePrefs::GetAvailability(profile->GetPrefs()) ==
             IncognitoModePrefs::FORCED;
}

// static
std::vector<Profile*> ProfileManager::GetLastOpenedProfiles() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetLastOpenedProfiles(
      profile_manager->user_data_dir_);
}

// static
Profile* ProfileManager::GetPrimaryUserProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)  // Can be null in unit tests.
    return nullptr;
#if defined(OS_CHROMEOS)
  if (!IsLoggedIn()) {
    return profile_manager->GetActiveUserOrOffTheRecordProfileFromPath(
        profile_manager->user_data_dir());
  }

  user_manager::UserManager* manager = user_manager::UserManager::Get();
  const user_manager::User* user = manager->GetPrimaryUser();
  if (!user)  // Can be null in unit tests.
    return nullptr;

  // Note: The ProfileHelper will take care of guest profiles.
  return chromeos::ProfileHelper::Get()->GetProfileByUserUnsafe(user);
#else
  return profile_manager->GetActiveUserOrOffTheRecordProfileFromPath(
      profile_manager->user_data_dir());
#endif
}

// static
Profile* ProfileManager::GetActiveUserProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
#if defined(OS_CHROMEOS)
  if (!profile_manager)
    return nullptr;

  if (IsLoggedIn()) {
    user_manager::UserManager* manager = user_manager::UserManager::Get();
    const user_manager::User* user = manager->GetActiveUser();
    // To avoid an endless loop (crbug.com/334098) we have to additionally check
    // if the profile of the user was already created. If the profile was not
    // yet created we load the profile using the profile directly.
    // TODO: This should be cleaned up with the new profile manager.
    if (user && user->is_profile_created())
      return chromeos::ProfileHelper::Get()->GetProfileByUserUnsafe(user);
  }
#endif
  Profile* profile =
      profile_manager->GetActiveUserOrOffTheRecordProfileFromPath(
          profile_manager->user_data_dir());
  // |profile| could be null if the user doesn't have a profile yet and the path
  // is on a read-only volume (preventing Chrome from making a new one).
  // However, most callers of this function immediately dereference the result
  // which would lead to crashes in a variety of call sites. Assert here to
  // figure out how common this is. http://crbug.com/383019
  CHECK(profile) << profile_manager->user_data_dir().AsUTF8Unsafe();
  return profile;
}

// static
Profile* ProfileManager::CreateInitialProfile() {
  ProfileManager* const profile_manager = g_browser_process->profile_manager();
  Profile* profile =
      profile_manager->GetProfile(profile_manager->user_data_dir().Append(
          profile_manager->GetInitialProfileDir()));

  if (profile_manager->ShouldGoOffTheRecord(profile))
    return profile->GetPrimaryOTRProfile();
  return profile;
}

void ProfileManager::AddObserver(ProfileManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void ProfileManager::RemoveObserver(ProfileManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

Profile* ProfileManager::GetProfile(const base::FilePath& profile_dir) {
  TRACE_EVENT0("browser", "ProfileManager::GetProfile");

  // If the profile is already loaded (e.g., chrome.exe launched twice), just
  // return it.
  Profile* profile = GetProfileByPath(profile_dir);
  if (profile)
    return profile;
  return CreateAndInitializeProfile(profile_dir);
}

size_t ProfileManager::GetNumberOfProfiles() {
  return GetProfileAttributesStorage().GetNumberOfProfiles();
}

bool ProfileManager::LoadProfile(const std::string& profile_name,
                                 bool incognito,
                                 ProfileLoadedCallback callback) {
  const base::FilePath profile_path = user_data_dir().AppendASCII(profile_name);
  return LoadProfileByPath(profile_path, incognito, std::move(callback));
}

bool ProfileManager::LoadProfileByPath(const base::FilePath& profile_path,
                                       bool incognito,
                                       ProfileLoadedCallback callback) {
  ProfileAttributesEntry* entry = nullptr;
  if (!GetProfileAttributesStorage().GetProfileAttributesWithPath(profile_path,
                                                                  &entry)) {
    std::move(callback).Run(nullptr);
    LOG(ERROR) << "Loading a profile path that does not exist";
    return false;
  }
  CreateProfileAsync(
      profile_path,
      base::BindRepeating(&OnProfileLoaded,
                          // OnProfileLoaded may be called multiple times, but
                          // |callback| will be called only once.
                          base::AdaptCallbackForRepeating(std::move(callback)),
                          incognito),
      base::string16() /* name */, std::string() /* icon_url */);
  return true;
}

void ProfileManager::CreateProfileAsync(const base::FilePath& profile_path,
                                        const CreateCallback& callback,
                                        const base::string16& name,
                                        const std::string& icon_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT1("browser,startup",
               "ProfileManager::CreateProfileAsync",
               "profile_path",
               profile_path.AsUTF8Unsafe());

  bool is_allowed_path = IsAllowedProfilePath(profile_path) ||
                         base::CommandLine::ForCurrentProcess()->HasSwitch(
                             switches::kAllowProfilesOutsideUserDir);

  // Make sure the path is correct and this profile is not pending deletion.
  if (!is_allowed_path || IsProfileDirectoryMarkedForDeletion(profile_path)) {
    if (!is_allowed_path) {
      LOG(ERROR) << "Cannot create profile at path "
                 << profile_path.AsUTF8Unsafe();
    }
    if (!callback.is_null())
      callback.Run(nullptr, Profile::CREATE_STATUS_LOCAL_FAIL);
    return;
  }

  // Create the profile if needed and collect its ProfileInfo.
  auto iter = profiles_info_.find(profile_path);
  ProfileInfo* info = nullptr;

  if (iter != profiles_info_.end()) {
    info = iter->second.get();
  } else {
    // Initiate asynchronous creation process.
    info = RegisterProfile(CreateProfileAsyncHelper(profile_path, this), false);
    size_t icon_index;
    DCHECK(base::IsStringASCII(icon_url));
    if (profiles::IsDefaultAvatarIconUrl(icon_url, &icon_index)) {
      // add profile to cache with user selected name and avatar
      GetProfileAttributesStorage().AddProfile(
          profile_path, name, std::string(), base::string16(),
          /*is_consented_primary_account=*/false, icon_index,
          /*supervised_user_id=*/std::string(), EmptyAccountId());
    }
  }

  // Call or enqueue the callback.
  if (!callback.is_null()) {
    if (iter != profiles_info_.end() && info->created) {
      Profile* profile = info->profile.get();
      // If this was the guest profile, apply settings and go OffTheRecord.
      // The system profile also needs characteristics of being off the record,
      // such as having no extensions, not writing to disk, etc.
      if (profile->IsGuestSession() || profile->IsSystemProfile()) {
        SetNonPersonalProfilePrefs(profile);
        profile = profile->GetPrimaryOTRProfile();
      }
      // Profile has already been created. Run callback immediately.
      callback.Run(profile, Profile::CREATE_STATUS_INITIALIZED);
    } else {
      // Profile is either already in the process of being created, or new.
      // Add callback to the list.
      info->callbacks.push_back(callback);
    }
  }
}

bool ProfileManager::IsValidProfile(const void* profile) {
  for (auto iter = profiles_info_.begin(); iter != profiles_info_.end();
       ++iter) {
    if (iter->second->created) {
      Profile* candidate = iter->second->profile.get();
      if (candidate == profile)
        return true;
      std::vector<Profile*> otr_profiles =
          candidate->GetAllOffTheRecordProfiles();
      if (base::Contains(otr_profiles, profile))
        return true;
    }
  }
  return false;
}

base::FilePath ProfileManager::GetInitialProfileDir() {
#if defined(OS_CHROMEOS)
  if (IsLoggedIn())
    return chromeos::ProfileHelper::Get()->GetActiveUserProfileDir();
#endif
  base::FilePath relative_profile_dir;
  // TODO(mirandac): should not automatically be default profile.
  return relative_profile_dir.AppendASCII(chrome::kInitialProfile);
}

Profile* ProfileManager::GetLastUsedProfile(
    const base::FilePath& user_data_dir) {
#if defined(OS_CHROMEOS)
  // Use default login profile if user has not logged in yet.
  if (!IsLoggedIn())
    return GetActiveUserOrOffTheRecordProfileFromPath(user_data_dir);

  // CrOS multi-profiles implementation is different so GetLastUsedProfile
  // has custom implementation too.
  base::FilePath profile_dir;
  // In case of multi-profiles we ignore "last used profile" preference
  // since it may refer to profile that has been in use in previous session.
  // That profile dir may not be mounted in this session so instead return
  // active profile from current session.
  profile_dir = chromeos::ProfileHelper::Get()->GetActiveUserProfileDir();

  base::FilePath profile_path(user_data_dir);
  Profile* profile = GetProfileByPath(profile_path.Append(profile_dir));

  // Accessing a user profile before it is loaded may lead to policy exploit.
  // See http://crbug.com/689206.
  LOG_IF(FATAL, !profile) << "Calling GetLastUsedProfile() before profile "
                          << "initialization is completed.";

  return profile->IsGuestSession() ? profile->GetPrimaryOTRProfile() : profile;
#else
  return GetProfile(GetLastUsedProfileDir(user_data_dir));
#endif
}

base::FilePath ProfileManager::GetLastUsedProfileDir(
    const base::FilePath& user_data_dir) {
  return user_data_dir.AppendASCII(GetLastUsedProfileName());
}

std::string ProfileManager::GetLastUsedProfileName() {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  const std::string last_used_profile_name =
      local_state->GetString(prefs::kProfileLastUsed);
  // Make sure the system profile can't be the one marked as the last one used
  // since it shouldn't get a browser.
  if (!last_used_profile_name.empty() &&
      last_used_profile_name !=
          base::FilePath(chrome::kSystemProfileDir).AsUTF8Unsafe()) {
    return last_used_profile_name;
  }

  return chrome::kInitialProfile;
}

std::vector<Profile*> ProfileManager::GetLastOpenedProfiles(
    const base::FilePath& user_data_dir) {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  std::vector<Profile*> to_return;
  if (local_state->HasPrefPath(prefs::kProfilesLastActive) &&
      local_state->GetList(prefs::kProfilesLastActive)) {
    // Make a copy because the list might change in the calls to GetProfile.
    std::unique_ptr<base::ListValue> profile_list(
        local_state->GetList(prefs::kProfilesLastActive)->DeepCopy());
    base::ListValue::const_iterator it;
    for (it = profile_list->begin(); it != profile_list->end(); ++it) {
      std::string profile_path;
      if (!it->GetAsString(&profile_path) || profile_path.empty() ||
          profile_path ==
              base::FilePath(chrome::kSystemProfileDir).AsUTF8Unsafe()) {
        LOG(WARNING) << "Invalid entry in " << prefs::kProfilesLastActive;
        continue;
      }
      Profile* profile = GetProfile(user_data_dir.AppendASCII(profile_path));
      if (profile) {
        // crbug.com/823338 -> CHECK that the profiles aren't guest or
        // incognito, causing a crash during session restore.
        CHECK(!profile->IsGuestSession())
            << "Guest profiles shouldn't have been saved as active profiles";
        CHECK(!profile->IsOffTheRecord())
            << "OTR profiles shouldn't have been saved as active profiles";
        to_return.push_back(profile);
      }
    }
  }
  return to_return;
}

std::vector<Profile*> ProfileManager::GetLoadedProfiles() const {
  std::vector<Profile*> profiles;
  for (auto iter = profiles_info_.begin(); iter != profiles_info_.end();
       ++iter) {
    if (iter->second->created)
      profiles.push_back(iter->second->profile.get());
  }
  return profiles;
}

Profile* ProfileManager::GetProfileByPathInternal(
    const base::FilePath& path) const {
  TRACE_EVENT0("browser", "ProfileManager::GetProfileByPathInternal");
  ProfileInfo* profile_info = GetProfileInfoByPath(path);
  return profile_info ? profile_info->profile.get() : nullptr;
}

bool ProfileManager::IsAllowedProfilePath(const base::FilePath& path) const {
  return path.DirName() == user_data_dir();
}

Profile* ProfileManager::GetProfileByPath(const base::FilePath& path) const {
  TRACE_EVENT0("browser", "ProfileManager::GetProfileByPath");
  ProfileInfo* profile_info = GetProfileInfoByPath(path);
  return (profile_info && profile_info->created) ? profile_info->profile.get()
                                                 : nullptr;
}

// static
Profile* ProfileManager::GetProfileFromProfileKey(ProfileKey* profile_key) {
  Profile* profile = g_browser_process->profile_manager()->GetProfileByPath(
      profile_key->GetPath());
  if (profile->GetProfileKey() == profile_key)
    return profile;

  for (Profile* otr : profile->GetAllOffTheRecordProfiles()) {
    if (otr->GetProfileKey() == profile_key)
      return otr;
  }

  NOTREACHED() << "An invalid profile key is passed.";
  return nullptr;
}

// static
base::FilePath ProfileManager::CreateMultiProfileAsync(
    const base::string16& name,
    const std::string& icon_url,
    const CreateCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();

  profile_manager->CreateProfileAsync(new_path, callback, name, icon_url);
  return new_path;
}

// static
base::FilePath ProfileManager::GetGuestProfilePath() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath guest_path = profile_manager->user_data_dir();
  return guest_path.Append(chrome::kGuestProfileDir);
}

// static
base::FilePath ProfileManager::GetSystemProfilePath() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath system_path = profile_manager->user_data_dir();
  return system_path.Append(chrome::kSystemProfileDir);
}

base::FilePath ProfileManager::GenerateNextProfileDirectoryPath() {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  DCHECK(profiles::IsMultipleProfilesEnabled());

  // Create the next profile in the next available directory slot.
  int next_directory = local_state->GetInteger(prefs::kProfilesNumCreated);
  std::string profile_name = chrome::kMultiProfileDirPrefix;
  profile_name.append(base::NumberToString(next_directory));
  base::FilePath new_path = user_data_dir_;
#if defined(OS_WIN)
  new_path = new_path.Append(base::ASCIIToUTF16(profile_name));
#else
  new_path = new_path.Append(profile_name);
#endif
  local_state->SetInteger(prefs::kProfilesNumCreated, ++next_directory);
  return new_path;
}

ProfileInfoCache& ProfileManager::GetProfileInfoCache() {
  TRACE_EVENT0("browser", "ProfileManager::GetProfileInfoCache");
  if (!profile_info_cache_) {
    profile_info_cache_.reset(new ProfileInfoCache(
        g_browser_process->local_state(), user_data_dir_));
  }
  return *profile_info_cache_.get();
}

ProfileAttributesStorage& ProfileManager::GetProfileAttributesStorage() {
  return GetProfileInfoCache();
}

ProfileShortcutManager* ProfileManager::profile_shortcut_manager() {
  return profile_shortcut_manager_.get();
}

#if !defined(OS_ANDROID)
void ProfileManager::MaybeScheduleProfileForDeletion(
    const base::FilePath& profile_dir,
    ProfileLoadedCallback callback,
    ProfileMetrics::ProfileDelete deletion_source) {
  if (!ScheduleProfileDirectoryForDeletion(profile_dir))
    return;

  ProfileAttributesStorage& storage = GetProfileAttributesStorage();
  ProfileAttributesEntry* entry;
  if (storage.GetProfileAttributesWithPath(profile_dir, &entry)) {
    storage.RecordDeletedProfileState(entry);
  }
  ProfileMetrics::LogProfileDeleteUser(deletion_source);

  ScheduleProfileForDeletion(profile_dir, std::move(callback));
}

void ProfileManager::ScheduleProfileForDeletion(
    const base::FilePath& profile_dir,
    ProfileLoadedCallback callback) {
  DCHECK(profiles::IsMultipleProfilesEnabled());
  DCHECK(!IsProfileDirectoryMarkedForDeletion(profile_dir));

  Profile* profile = GetProfileByPath(profile_dir);
  if (profile) {
    // Cancel all in-progress downloads before deleting the profile to prevent a
    // "Do you want to exit Google Chrome and cancel the downloads?" prompt
    // (crbug.com/336725).
    DownloadCoreService* service =
        DownloadCoreServiceFactory::GetForBrowserContext(profile);
    service->CancelDownloads();
    DCHECK_EQ(0, service->NonMaliciousDownloadCount());

    // Close all browser windows before deleting the profile. If the user
    // cancels the closing of any tab in an OnBeforeUnload event, profile
    // deletion is also cancelled. (crbug.com/289390)
    BrowserList::CloseAllBrowsersWithProfile(
        profile,
        base::Bind(&ProfileManager::EnsureActiveProfileExistsBeforeDeletion,
                   base::Unretained(this), base::Passed(std::move(callback))),
        base::Bind(&CancelProfileDeletion), false);
  } else {
    EnsureActiveProfileExistsBeforeDeletion(std::move(callback), profile_dir);
  }
}
#endif  // !defined(OS_ANDROID)

void ProfileManager::AutoloadProfiles() {
  // If running in the background is disabled for the browser, do not autoload
  // any profiles.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  if (!local_state->HasPrefPath(prefs::kBackgroundModeEnabled) ||
      !local_state->GetBoolean(prefs::kBackgroundModeEnabled)) {
    return;
  }

  std::vector<ProfileAttributesEntry*> entries =
      GetProfileAttributesStorage().GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    if (entry->GetBackgroundStatus()) {
      // If status is true, that profile is running background apps. By calling
      // GetProfile, we automatically cause the profile to be loaded which will
      // register it with the BackgroundModeManager.
      GetProfile(entry->GetPath());
    }
  }
}

void ProfileManager::CleanUpEphemeralProfiles() {
  const std::string last_used_profile = GetLastUsedProfileName();
  bool last_active_profile_deleted = false;
  base::FilePath new_profile_path;
  std::vector<base::FilePath> profiles_to_delete;
  ProfileAttributesStorage& storage = GetProfileAttributesStorage();
  std::vector<ProfileAttributesEntry*> entries =
      storage.GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    base::FilePath profile_path = entry->GetPath();
    if (entry->IsEphemeral()) {
      profiles_to_delete.push_back(profile_path);
      RemoveFromLastActiveProfilesPrefList(profile_path);
      if (profile_path.BaseName().MaybeAsASCII() == last_used_profile)
        last_active_profile_deleted = true;
    } else if (new_profile_path.empty()) {
      new_profile_path = profile_path;
    }
  }

  // If the last active profile was ephemeral or all profiles are deleted due to
  // ephemeral, set a new one.
  if (last_active_profile_deleted ||
      (entries.size() == profiles_to_delete.size() &&
       !profiles_to_delete.empty())) {
    if (new_profile_path.empty())
      new_profile_path = GenerateNextProfileDirectoryPath();

    profiles::SetLastUsedProfile(new_profile_path.BaseName().MaybeAsASCII());
  }

  // This uses a separate loop, because deleting the profile from the
  // ProfileInfoCache will modify indices.
  for (const base::FilePath& profile_path : profiles_to_delete) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&NukeProfileFromDisk, profile_path));

    storage.RemoveProfile(profile_path);
  }
}

void ProfileManager::CleanUpDeletedProfiles() {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  const base::ListValue* deleted_profiles =
      local_state->GetList(prefs::kProfilesDeleted);
  DCHECK(deleted_profiles);

  for (const base::Value& value : *deleted_profiles) {
    base::Optional<base::FilePath> profile_path = util::ValueToFilePath(value);
    // Although it should never happen, make sure this is a valid path in the
    // user_data_dir, so we don't accidentally delete something else.
    if (profile_path && IsAllowedProfilePath(*profile_path)) {
      if (base::PathExists(*profile_path)) {
        LOG(WARNING) << "Files of a deleted profile still exist after restart. "
                        "Cleaning up now.";
        base::ThreadPool::PostTaskAndReply(
            FROM_HERE,
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
            base::BindOnce(&NukeProfileFromDisk, *profile_path),
            base::BindOnce(&ProfileCleanedUp, &value));
      } else {
        // Everything is fine, the profile was removed on shutdown.
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(&ProfileCleanedUp, &value));
      }
    } else {
      LOG(ERROR) << "Found invalid profile path in deleted_profiles: "
                 << profile_path->AsUTF8Unsafe();
      NOTREACHED();
    }
  }
}

void ProfileManager::InitProfileUserPrefs(Profile* profile) {
  TRACE_EVENT0("browser", "ProfileManager::InitProfileUserPrefs");
  ProfileAttributesStorage& storage = GetProfileAttributesStorage();

  if (!IsAllowedProfilePath(profile->GetPath())) {
    LOG(WARNING) << "Failed to initialize prefs for a profile at invalid path: "
                 << profile->GetPath().AsUTF8Unsafe();
    return;
  }

#if defined(OS_CHROMEOS)
  // User object may already have changed user type, so we apply that
  // type to profile.
  // If profile type has changed, remove ProfileInfoCache entry for it to
  // make sure it is fully re-initialized later.
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (user) {
    const bool user_is_child =
        (user->GetType() == user_manager::USER_TYPE_CHILD);
    const bool profile_is_child = profile->IsChild();
    const bool profile_is_new = profile->IsNewProfile();
    if (!profile_is_new && profile_is_child != user_is_child) {
      ProfileAttributesEntry* entry;
      if (storage.GetProfileAttributesWithPath(profile->GetPath(), &entry)) {
        LOG(WARNING) << "Profile child status has changed.";
        storage.RemoveProfile(profile->GetPath());
      }
      arc::ArcSupervisionTransition supervisionTransition;
      if (!profile->GetPrefs()->GetBoolean(arc::prefs::kArcSignedIn)) {
        // No transition is necessary if user never enabled ARC.
        supervisionTransition = arc::ArcSupervisionTransition::NO_TRANSITION;
      } else {
        // Notify ARC about user type change via prefs if user enabled ARC.
        supervisionTransition =
            user_is_child ? arc::ArcSupervisionTransition::REGULAR_TO_CHILD
                          : arc::ArcSupervisionTransition::CHILD_TO_REGULAR;
      }
      profile->GetPrefs()->SetInteger(arc::prefs::kArcSupervisionTransition,
                                      static_cast<int>(supervisionTransition));
      chromeos::ChildAccountTypeChangedUserData::GetForProfile(profile)
          ->SetValue(true);
    } else {
      chromeos::ChildAccountTypeChangedUserData::GetForProfile(profile)
          ->SetValue(false);
    }

    if (user_is_child) {
      profile->GetPrefs()->SetString(prefs::kSupervisedUserId,
                                     supervised_users::kChildAccountSUID);
    } else {
      profile->GetPrefs()->ClearPref(prefs::kSupervisedUserId);
    }
  }
#endif

  size_t avatar_index;
  std::string profile_name;
  std::string supervised_user_id;
  if (profile->IsGuestSession()) {
    profile_name = l10n_util::GetStringUTF8(IDS_PROFILES_GUEST_PROFILE_NAME);
    avatar_index = 0;
  } else {
    ProfileAttributesEntry* entry;
    bool has_entry = storage.GetProfileAttributesWithPath(profile->GetPath(),
                                                          &entry);
    // If the profile attributes storage has an entry for this profile, use the
    // data in the profile attributes storage.
    if (has_entry) {
      avatar_index = entry->GetAvatarIconIndex();
      profile_name = base::UTF16ToUTF8(entry->GetLocalProfileName());
      supervised_user_id = entry->GetSupervisedUserId();
    } else if (profile->GetPath() ==
                   profiles::GetDefaultProfileDir(user_data_dir())) {
      avatar_index = profiles::GetPlaceholderAvatarIndex();
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
      profile_name =
          base::UTF16ToUTF8(storage.ChooseNameForNewProfile(avatar_index));
#else
      profile_name = l10n_util::GetStringUTF8(IDS_DEFAULT_PROFILE_NAME);
#endif
    } else {
      avatar_index = storage.ChooseAvatarIconIndexForNewProfile();
      profile_name =
          base::UTF16ToUTF8(storage.ChooseNameForNewProfile(avatar_index));
    }
  }

  if (!profile->GetPrefs()->HasPrefPath(prefs::kProfileAvatarIndex))
    profile->GetPrefs()->SetInteger(prefs::kProfileAvatarIndex, avatar_index);

  if (!profile->GetPrefs()->HasPrefPath(prefs::kProfileName)) {
    profile->GetPrefs()->SetString(prefs::kProfileName, profile_name);
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool force_supervised_user_id =
#if defined(OS_CHROMEOS)
      g_browser_process->platform_part()
              ->profile_helper()
              ->GetSigninProfileDir() != profile->GetPath() &&
      g_browser_process->platform_part()
              ->profile_helper()
              ->GetLockScreenAppProfilePath() != profile->GetPath() &&
#endif
      command_line->HasSwitch(switches::kSupervisedUserId);

  if (force_supervised_user_id) {
    supervised_user_id =
        command_line->GetSwitchValueASCII(switches::kSupervisedUserId);
  }
  if (force_supervised_user_id ||
      !profile->GetPrefs()->HasPrefPath(prefs::kSupervisedUserId)) {
    profile->GetPrefs()->SetString(prefs::kSupervisedUserId,
                                   supervised_user_id);
  }
#if !defined(OS_ANDROID)
  // TODO(pmonette): Fix IsNewProfile() to handle the case where the profile is
  // new even if the "Preferences" file already existed. (For example: The
  // master_preferences file is dumped into the default profile on first run,
  // before profile creation.)
  if (profile->IsNewProfile() || first_run::IsChromeFirstRun()) {
    profile->GetPrefs()->SetBoolean(prefs::kHasSeenWelcomePage, false);
  }
#endif  // !defined(OS_ANDROID)
}

void ProfileManager::RegisterTestingProfile(std::unique_ptr<Profile> profile,
                                            bool add_to_storage) {
  Profile* profile_ptr = profile.get();
  RegisterProfile(std::move(profile), true);
  if (add_to_storage) {
    InitProfileUserPrefs(profile_ptr);
    AddProfileToStorage(profile_ptr);
  }
}

void ProfileManager::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  bool save_active_profiles = false;
  switch (type) {
    case chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST: {
      // Ignore any browsers closing from now on.
      closing_all_browsers_ = true;
      save_active_profiles = true;
      break;
    }
    case chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED: {
      // This will cancel the shutdown process, so the active profiles are
      // tracked again. Also, as the active profiles may have changed (i.e. if
      // some windows were closed) we save the current list of active profiles
      // again.
      closing_all_browsers_ = false;
      save_active_profiles = true;
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }

  if (save_active_profiles)
    SaveActiveProfiles();
}

void ProfileManager::OnProfileCreated(Profile* profile,
                                      bool success,
                                      bool is_new_profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto iter = profiles_info_.find(profile->GetPath());
  DCHECK(iter != profiles_info_.end());
  ProfileInfo* info = iter->second.get();

  std::vector<CreateCallback> callbacks;
  info->callbacks.swap(callbacks);

  // Invoke CREATED callback for normal profiles.
  bool go_off_the_record = ShouldGoOffTheRecord(profile);
  if (success && !go_off_the_record)
    RunCallbacks(callbacks, profile, Profile::CREATE_STATUS_CREATED);

  // Perform initialization.
  if (success) {
    DoFinalInit(info, go_off_the_record);
    if (go_off_the_record)
      profile = profile->GetPrimaryOTRProfile();
  } else {
    profile = nullptr;
    profiles_info_.erase(iter);
  }

  if (profile) {
    // If this was the guest or system profile, finish setting its special
    // status.
    if (profile->IsGuestSession() || profile->IsSystemProfile())
      SetNonPersonalProfilePrefs(profile);

    // Invoke CREATED callback for incognito profiles.
    if (go_off_the_record)
      RunCallbacks(callbacks, profile, Profile::CREATE_STATUS_CREATED);
  }

  // Invoke INITIALIZED or FAIL for all profiles.
  RunCallbacks(callbacks, profile,
               profile ? Profile::CREATE_STATUS_INITIALIZED :
                         Profile::CREATE_STATUS_LOCAL_FAIL);
}

std::unique_ptr<Profile> ProfileManager::CreateProfileHelper(
    const base::FilePath& path) {
  TRACE_EVENT0("browser", "ProfileManager::CreateProfileHelper");

  return Profile::CreateProfile(path, nullptr,
                                Profile::CREATE_MODE_SYNCHRONOUS);
}

std::unique_ptr<Profile> ProfileManager::CreateProfileAsyncHelper(
    const base::FilePath& path,
    Delegate* delegate) {
  return Profile::CreateProfile(path, delegate,
                                Profile::CREATE_MODE_ASYNCHRONOUS);
}

void ProfileManager::DoFinalInit(ProfileInfo* profile_info,
                                 bool go_off_the_record) {
  TRACE_EVENT0("browser", "ProfileManager::DoFinalInit");

  Profile* profile = profile_info->profile.get();
  DoFinalInitForServices(profile, go_off_the_record);
  AddProfileToStorage(profile);
  DoFinalInitLogging(profile);

  // Set the |created| flag now so that PROFILE_ADDED handlers can use
  // GetProfileByPath().
  profile_info->created = true;

  for (auto& observer : observers_)
    observer.OnProfileAdded(profile);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_ADDED,
      content::Source<Profile>(profile),
      content::NotificationService::NoDetails());

  // At this point, the user policy service and the child account service
  // had enough time to initialize and should have updated the user signout
  // flag attached to the profile.
  signin_util::EnsureUserSignoutAllowedIsInitializedForProfile(profile);
  signin_util::EnsurePrimaryAccountAllowedForProfile(profile);

#if !defined(OS_ANDROID)
  // The caret browsing command-line switch toggles caret browsing on
  // initially, but the user can still toggle it from there.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCaretBrowsing))
    profile->GetPrefs()->SetBoolean(prefs::kCaretBrowsingEnabled, true);
#endif
}

void ProfileManager::DoFinalInitForServices(Profile* profile,
                                            bool go_off_the_record) {
  if (!do_final_services_init_)
    return;

  TRACE_EVENT0("browser", "ProfileManager::DoFinalInitForServices");

#if BUILDFLAG(ENABLE_EXTENSIONS)
  bool extensions_enabled = !go_off_the_record;
#if defined(OS_CHROMEOS)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableLoginScreenApps) &&
      chromeos::ProfileHelper::IsSigninProfile(profile)) {
    extensions_enabled = true;
  }
  if (chromeos::ProfileHelper::IsLockScreenAppProfile(profile))
    extensions_enabled = true;
#endif
  extensions::ExtensionSystem::Get(profile)->InitForRegularProfile(
      extensions_enabled);

  // Set the block extensions bit on the ExtensionService. There likely are no
  // blockable extensions to block.
  ProfileAttributesEntry* entry;
  bool has_entry = GetProfileAttributesStorage().
                       GetProfileAttributesWithPath(profile->GetPath(), &entry);
  if (has_entry && entry->IsSigninRequired()) {
    extensions::ExtensionSystem::Get(profile)
        ->extension_service()
        ->BlockAllExtensions();
  }

#endif
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // Initialization needs to happen after extension system initialization (for
  // extension::ManagementPolicy) and InitProfileUserPrefs (for setting the
  // initializing the supervised flag if necessary).
  ChildAccountServiceFactory::GetForProfile(profile)->Init();
  SupervisedUserServiceFactory::GetForProfile(profile)->Init();
#endif
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  // If the lock enabled algorithm changed, update this profile's lock status.
  // This depends on services which shouldn't be initialized until
  // DoFinalInitForServices.
  profiles::UpdateIsProfileLockEnabledIfNeeded(profile);
#endif

  // Activate data reduction proxy. This creates a request context and makes a
  // URL request to check if the data reduction proxy server is reachable.
  DataReductionProxyChromeSettingsFactory::GetForBrowserContext(profile)->
      MaybeActivateDataReductionProxy(true);

  auto* proto_db_provider =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetProtoDatabaseProvider();

  // Creates the Optimization Guide Keyed Service and begins loading the
  // hint cache from persistent memory.
  auto* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (optimization_guide_keyed_service) {
    optimization_guide_keyed_service->Initialize(
        g_browser_process->optimization_guide_service(), proto_db_provider,
        profile->GetPath());
  }

  // Create the Previews Service and begin loading opt out history from
  // persistent memory.
  PreviewsServiceFactory::GetForProfile(profile)->Initialize(
      content::GetUIThreadTaskRunner({}), profile->GetPath());

  // Ensure NavigationPredictorKeyedService is started.
  NavigationPredictorKeyedServiceFactory::GetForProfile(profile);

  IdentityManagerFactory::GetForProfile(profile)->OnNetworkInitialized();
  AccountReconcilorFactory::GetForProfile(profile);

  // Initialization needs to happen after the browser context is available
  // because ProfileSyncService needs the URL context getter.
  UnifiedConsentServiceFactory::GetForProfile(profile);

#if defined(OS_ANDROID)
  AndroidProfileSessionDurationsServiceFactory::GetForProfile(profile);
#else
  captions::CaptionControllerFactory::GetForProfile(profile)->Init();
#endif

#if defined(OS_WIN) && BUILDFLAG(ENABLE_DICE_SUPPORT)
  signin_util::SigninWithCredentialProviderIfPossible(profile);
#endif

  AccessibilityLabelsServiceFactory::GetForProfile(profile)->Init();

#if defined(OS_CHROMEOS)
  chromeos::AccountManagerPolicyControllerFactory::GetForBrowserContext(
      profile);
#endif

  // Creates the LiteVideo Keyed Service and begins loading the
  // hint cache and user blocklist.
  auto* lite_video_keyed_service =
      LiteVideoKeyedServiceFactory::GetForProfile(profile);
  if (lite_video_keyed_service)
    lite_video_keyed_service->Initialize(profile->GetPath());

  // TODO(crbug.com/1031477): Remove once getting this created with the browser
  // context does not change dependency initialization order to cause crashes.
  AdaptiveQuietNotificationPermissionUiEnabler::GetForProfile(profile);
}

void ProfileManager::DoFinalInitLogging(Profile* profile) {
  if (!do_final_services_init_)
    return;

  TRACE_EVENT0("browser", "ProfileManager::DoFinalInitLogging");
  // Count number of extensions in this profile.
  int enabled_app_count = -1;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  enabled_app_count = GetEnabledAppCount(profile);
#endif

  // Log the profile size after a reasonable startup delay.
  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ProfileSizeTask, profile->GetPath(), enabled_app_count),
      base::TimeDelta::FromSeconds(112));
}

ProfileManager::ProfileInfo::ProfileInfo(std::unique_ptr<Profile> profile,
                                         bool created)
    : profile(std::move(profile)), created(created) {}

ProfileManager::ProfileInfo::~ProfileInfo() {
  ProfileDestroyer::DestroyProfileWhenAppropriate(profile.release());
}

Profile* ProfileManager::GetActiveUserOrOffTheRecordProfileFromPath(
    const base::FilePath& user_data_dir) {
#if defined(OS_CHROMEOS)
  base::FilePath default_profile_dir(user_data_dir);
  if (!IsLoggedIn()) {
    default_profile_dir = profiles::GetDefaultProfileDir(user_data_dir);
    Profile* profile = GetProfile(default_profile_dir);
    // For cros, return the OTR profile so we never accidentally keep
    // user data in an unencrypted profile. But doing this makes
    // many of the browser and ui tests fail. We do return the OTR profile
    // if the login-profile switch is passed so that we can test this.
    if (ShouldGoOffTheRecord(profile))
      return profile->GetPrimaryOTRProfile();
    DCHECK(!user_manager::UserManager::Get()->IsLoggedInAsGuest());
    return profile;
  }

  default_profile_dir = default_profile_dir.Append(GetInitialProfileDir());
  ProfileInfo* profile_info = GetProfileInfoByPath(default_profile_dir);
  // Fallback to default off-the-record profile, if user profile has not started
  // loading or has not fully loaded yet.
  if (!profile_info || !profile_info->created)
    default_profile_dir = profiles::GetDefaultProfileDir(user_data_dir);

  Profile* profile = GetProfile(default_profile_dir);
  // Some unit tests didn't initialize the UserManager.
  if (user_manager::UserManager::IsInitialized() &&
      user_manager::UserManager::Get()->IsLoggedInAsGuest())
    return profile->GetPrimaryOTRProfile();
  return profile;
#else
  base::FilePath default_profile_dir(user_data_dir);
  default_profile_dir = default_profile_dir.Append(GetInitialProfileDir());
  return GetProfile(default_profile_dir);
#endif
}

bool ProfileManager::AddProfile(std::unique_ptr<Profile> profile) {
  TRACE_EVENT0("browser", "ProfileManager::AddProfile");

  DCHECK(profile);

  // Make sure that we're not loading a profile with the same ID as a profile
  // that's already loaded.
  if (GetProfileByPathInternal(profile->GetPath())) {
    NOTREACHED() << "Attempted to add profile with the same path ("
                 << profile->GetPath().value()
                 << ") as an already-loaded profile.";
    return false;
  }

  ProfileInfo* profile_info = RegisterProfile(std::move(profile), true);
  InitProfileUserPrefs(profile_info->profile.get());
  DoFinalInit(profile_info, ShouldGoOffTheRecord(profile_info->profile.get()));
  return true;
}

Profile* ProfileManager::CreateAndInitializeProfile(
    const base::FilePath& profile_dir) {
  TRACE_EVENT0("browser", "ProfileManager::CreateAndInitializeProfile");

  if (!IsAllowedProfilePath(profile_dir) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowProfilesOutsideUserDir)) {
    LOG(ERROR) << "Cannot create profile at path "
               << profile_dir.AsUTF8Unsafe();
    return nullptr;
  }

  // CHECK that we are not trying to load the same profile twice, to prevent
  // profile corruption. Note that this check also covers the case when we have
  // already started loading the profile but it is not fully initialized yet,
  // which would make Bad Things happen if we returned it.
  CHECK(!GetProfileByPathInternal(profile_dir));
  std::unique_ptr<Profile> profile = CreateProfileHelper(profile_dir);
  if (!profile)
    return nullptr;

  Profile* profile_ptr = profile.get();
  bool result = AddProfile(std::move(profile));
  DCHECK(result);
  return profile_ptr;
}

#if !defined(OS_ANDROID)
void ProfileManager::EnsureActiveProfileExistsBeforeDeletion(
    ProfileLoadedCallback callback,
    const base::FilePath& profile_dir) {
  // In case we delete non-active profile and current profile is valid, proceed.
  const base::FilePath last_used_profile_path =
      GetLastUsedProfileDir(user_data_dir_);
  const base::FilePath guest_profile_path = GetGuestProfilePath();
  Profile* last_used_profile = GetProfileByPath(last_used_profile_path);
  if (last_used_profile_path != profile_dir &&
      last_used_profile_path != guest_profile_path && last_used_profile &&
      !last_used_profile->IsLegacySupervised()) {
    FinishDeletingProfile(profile_dir, last_used_profile_path);
    return;
  }

  // Search for an active browser and use its profile as active if possible.
  for (Browser* browser : *BrowserList::GetInstance()) {
    Profile* profile = browser->profile();
    base::FilePath cur_path = profile->GetPath();
    if (cur_path != profile_dir &&
        cur_path != guest_profile_path &&
        !profile->IsLegacySupervised() &&
        !IsProfileDirectoryMarkedForDeletion(cur_path)) {
      OnNewActiveProfileLoaded(profile_dir, cur_path, std::move(callback),
                               profile, Profile::CREATE_STATUS_INITIALIZED);
      return;
    }
  }

  // There no valid browsers to fallback, search for any existing valid profile.
  ProfileAttributesStorage& storage = GetProfileAttributesStorage();
  base::FilePath fallback_profile_path;
  std::vector<ProfileAttributesEntry*> entries =
      storage.GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    base::FilePath cur_path = entry->GetPath();
    // Make sure that this profile is not pending deletion, and is not
    // legacy-supervised.
    if (cur_path != profile_dir &&
        cur_path != guest_profile_path &&
        !entry->IsLegacySupervised() &&
        !IsProfileDirectoryMarkedForDeletion(cur_path)) {
      fallback_profile_path = cur_path;
      break;
    }
  }

  // If we're deleting the last (non-legacy-supervised) profile, then create a
  // new profile in its place. Load existing profile otherwise.
  std::string new_avatar_url;
  base::string16 new_profile_name;
  if (fallback_profile_path.empty()) {
    fallback_profile_path = GenerateNextProfileDirectoryPath();
#if !defined(OS_CHROMEOS)
    int avatar_index = profiles::GetPlaceholderAvatarIndex();
    new_avatar_url = profiles::GetDefaultAvatarIconUrl(avatar_index);
    new_profile_name = storage.ChooseNameForNewProfile(avatar_index);
#endif
    // A new profile about to be created.
    ProfileMetrics::LogProfileAddNewUser(
        ProfileMetrics::ADD_NEW_USER_LAST_DELETED);
  }

  // Create and/or load fallback profile.
  CreateProfileAsync(
      fallback_profile_path,
      base::BindRepeating(
          &ProfileManager::OnNewActiveProfileLoaded, base::Unretained(this),
          profile_dir, fallback_profile_path,
          // OnNewActiveProfileLoaded may be called several times, but
          // only once with CREATE_STATUS_INITIALIZED.
          base::AdaptCallbackForRepeating(std::move(callback))),
      new_profile_name, new_avatar_url);
}

void ProfileManager::OnLoadProfileForProfileDeletion(
    const base::FilePath& profile_dir,
    Profile* profile) {
  ProfileAttributesStorage& storage = GetProfileAttributesStorage();
  // TODO(sail): Due to bug 88586 we don't delete the profile instance. Once we
  // start deleting the profile instance we need to close background apps too.
  if (profile) {
    // TODO(estade): Migrate additional code in this block to observe
    // ProfileManager instead of handling shutdown here.
    for (auto& observer : observers_)
      observer.OnProfileMarkedForPermanentDeletion(profile);

    // Disable sync for doomed profile.
    if (ProfileSyncServiceFactory::HasSyncService(profile)) {
      syncer::SyncService* sync_service =
          ProfileSyncServiceFactory::GetForProfile(profile);
      // Ensure data is cleared even if sync was already off.
      sync_service->StopAndClear();
    }

    ProfileAttributesEntry* entry;
    bool has_entry = storage.GetProfileAttributesWithPath(profile_dir, &entry);
    DCHECK(has_entry);
    ProfileMetrics::LogProfileDelete(entry->IsAuthenticated());
    // Some platforms store passwords in keychains. They should be removed.
    scoped_refptr<password_manager::PasswordStore> password_store =
        PasswordStoreFactory::GetForProfile(profile,
                                            ServiceAccessType::EXPLICIT_ACCESS)
            .get();
    if (password_store.get()) {
      password_store->RemoveLoginsCreatedBetween(
          base::Time(), base::Time::Max(), base::Closure());
    }

    // The Profile Data doesn't get wiped until Chrome closes. Since we promised
    // that the user's data would be removed, do so immediately.
    profiles::RemoveBrowsingDataForProfile(profile_dir);

    // Clean-up pref data that won't be cleaned up by deleting the profile dir.
    profile->GetPrefs()->OnStoreDeletionFromDisk();
  } else {
    // We failed to load the profile, but it's safe to delete a not yet loaded
    // Profile from disk.
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&NukeProfileFromDisk, profile_dir));
  }

  storage.RemoveProfile(profile_dir);
}

void ProfileManager::FinishDeletingProfile(
    const base::FilePath& profile_dir,
    const base::FilePath& new_active_profile_dir) {
  // Update the last used profile pref before closing browser windows. This
  // way the correct last used profile is set for any notification observers.
  profiles::SetLastUsedProfile(
      new_active_profile_dir.BaseName().MaybeAsASCII());

  // Attempt to load the profile before deleting it to properly clean up
  // profile-specific data stored outside the profile directory.
  LoadProfileByPath(
      profile_dir, false,
      base::BindOnce(&ProfileManager::OnLoadProfileForProfileDeletion,
                     base::Unretained(this), profile_dir));

  // Prevents CreateProfileAsync from re-creating the profile.
  MarkProfileDirectoryForDeletion(profile_dir);
}
#endif  // !defined(OS_ANDROID)

ProfileManager::ProfileInfo* ProfileManager::RegisterProfile(
    std::unique_ptr<Profile> profile,
    bool created) {
  TRACE_EVENT0("browser", "ProfileManager::RegisterProfile");
  base::FilePath path = profile->GetPath();
  auto info = std::make_unique<ProfileInfo>(std::move(profile), created);
  ProfileInfo* info_raw = info.get();
  profiles_info_.insert(std::make_pair(path, std::move(info)));
  return info_raw;
}

ProfileManager::ProfileInfo* ProfileManager::GetProfileInfoByPath(
    const base::FilePath& path) const {
  auto it = profiles_info_.find(path);
  return it != profiles_info_.end() ? it->second.get() : nullptr;
}

void ProfileManager::AddProfileToStorage(Profile* profile) {
  TRACE_EVENT0("browser", "ProfileManager::AddProfileToCache");
  if (profile->IsGuestSession() || profile->IsSystemProfile())
    return;
  if (!IsAllowedProfilePath(profile->GetPath())) {
    LOG(WARNING) << "Failed to add to storage a profile at invalid path: "
                 << profile->GetPath().AsUTF8Unsafe();
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  bool is_consented_primary_account = identity_manager->HasPrimaryAccount();
  CoreAccountInfo account_info = identity_manager->GetPrimaryAccountInfo(
      signin::ConsentLevel::kNotRequired);

  base::string16 username = base::UTF8ToUTF16(account_info.email);

  ProfileAttributesStorage& storage = GetProfileAttributesStorage();
  // |entry| and |has_entry| below are put inside a pair of brackets for
  // scoping, to avoid potential clashes of variable names.
  {
    ProfileAttributesEntry* entry;
    bool has_entry = storage.GetProfileAttributesWithPath(profile->GetPath(),
                                                          &entry);
    if (has_entry) {
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
      bool was_authenticated_status = entry->IsAuthenticated();
#endif
      // The ProfileAttributesStorage's info must match the Identity Manager.
      entry->SetAuthInfo(account_info.gaia, username,
                         is_consented_primary_account);

      entry->SetSignedInWithCredentialProvider(profile->GetPrefs()->GetBoolean(
          prefs::kSignedInWithCredentialProvider));

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
      // Sign out if force-sign-in policy is enabled and profile is not signed
      // in.
      VLOG(1) << "ForceSigninCheck: " << signin_util::IsForceSigninEnabled()
              << ", " << was_authenticated_status << ", "
              << !entry->IsAuthenticated();
      if (signin_util::IsForceSigninEnabled() && was_authenticated_status &&
          !entry->IsAuthenticated()) {
        auto* account_mutator = identity_manager->GetPrimaryAccountMutator();

        // GetPrimaryAccountMutator() returns nullptr on ChromeOS only.
        DCHECK(account_mutator);
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE,
            base::BindOnce(
                base::IgnoreResult(
                    &signin::PrimaryAccountMutator::ClearPrimaryAccount),
                base::Unretained(account_mutator),
                signin::PrimaryAccountMutator::ClearAccountsAction::kRemoveAll,
                signin_metrics::AUTHENTICATION_FAILED_WITH_FORCE_SIGNIN,
                signin_metrics::SignoutDelete::IGNORE_METRIC));
      }
#endif
      return;
    }
  }

  // Profile name and avatar are set by InitProfileUserPrefs and stored in the
  // profile. Use those values to setup the entry in profile attributes storage.
  base::string16 profile_name =
      base::UTF8ToUTF16(profile->GetPrefs()->GetString(prefs::kProfileName));

  size_t icon_index =
      profile->GetPrefs()->GetInteger(prefs::kProfileAvatarIndex);

  std::string supervised_user_id =
      profile->GetPrefs()->GetString(prefs::kSupervisedUserId);

  AccountId account_id(EmptyAccountId());
#if defined(OS_CHROMEOS)
  user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (user)
    account_id = user->GetAccountId();
#endif

  storage.AddProfile(profile->GetPath(), profile_name, account_info.gaia,
                     username, is_consented_primary_account, icon_index,
                     supervised_user_id, account_id);

  ProfileAttributesEntry* entry;
  bool has_entry =
      storage.GetProfileAttributesWithPath(profile->GetPath(), &entry);
  DCHECK(has_entry);

  if (profile->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles))
    entry->SetIsEphemeral(true);

  entry->SetSignedInWithCredentialProvider(
      profile->GetPrefs()->GetBoolean(prefs::kSignedInWithCredentialProvider));
}

void ProfileManager::SetNonPersonalProfilePrefs(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(prefs::kSigninAllowed, false);
  prefs->SetBoolean(bookmarks::prefs::kEditBookmarksEnabled, false);
  prefs->SetBoolean(bookmarks::prefs::kShowBookmarkBar, false);
  prefs->ClearPref(DefaultSearchManager::kDefaultSearchProviderDataPrefName);
}

bool ProfileManager::ShouldGoOffTheRecord(Profile* profile) {
#if defined(OS_CHROMEOS)
  if (chromeos::ProfileHelper::IsSigninProfile(profile) ||
      chromeos::ProfileHelper::IsLockScreenAppProfile(profile)) {
    return true;
  }
#endif
  return profile->IsGuestSession() || profile->IsSystemProfile();
}

void ProfileManager::RunCallbacks(const std::vector<CreateCallback>& callbacks,
                                  Profile* profile,
                                  Profile::CreateStatus status) {
  for (size_t i = 0; i < callbacks.size(); ++i)
    callbacks[i].Run(profile, status);
}

void ProfileManager::SaveActiveProfiles() {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  ListPrefUpdate update(local_state, prefs::kProfilesLastActive);
  base::ListValue* profile_list = update.Get();

  profile_list->Clear();

  // crbug.com/120112 -> several non-off-the-record profiles might have the same
  // GetPath().BaseName(). In that case, we cannot restore both
  // profiles. Include each base name only once in the last active profile
  // list.
  std::set<std::string> profile_paths;
  std::vector<Profile*>::const_iterator it;
  for (it = active_profiles_.begin(); it != active_profiles_.end(); ++it) {
    // crbug.com/823338 -> CHECK that the profiles aren't guest or incognito,
    // causing a crash during session restore.
    CHECK(!(*it)->IsGuestSession())
        << "Guest profiles shouldn't be saved as active profiles";
    CHECK(!(*it)->IsOffTheRecord())
        << "OTR profiles shouldn't be saved as active profiles";
    std::string profile_path = (*it)->GetPath().BaseName().MaybeAsASCII();
    // Some profiles might become ephemeral after they are created.
    // Don't persist the System Profile as one of the last actives, it should
    // never get a browser.
    if (!(*it)->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles) &&
        profile_paths.find(profile_path) == profile_paths.end() &&
        profile_path !=
            base::FilePath(chrome::kSystemProfileDir).AsUTF8Unsafe()) {
      profile_paths.insert(profile_path);
      profile_list->AppendString(profile_path);
    }
  }
}

#if !defined(OS_ANDROID)
void ProfileManager::OnBrowserOpened(Browser* browser) {
  DCHECK(browser);
  Profile* profile = browser->profile();
  DCHECK(profile);
  bool is_ephemeral =
      profile->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles);
  if (!profile->IsOffTheRecord() && !is_ephemeral &&
      ++browser_counts_[profile] == 1) {
    active_profiles_.push_back(profile);
    SaveActiveProfiles();
  }
  // If browsers are opening, we can't be closing all the browsers. This
  // can happen if the application was exited, but background mode or
  // packaged apps prevented the process from shutting down, and then
  // a new browser window was opened.
  closing_all_browsers_ = false;
}

void ProfileManager::OnBrowserClosed(Browser* browser) {
  Profile* profile = browser->profile();
  DCHECK(profile);
  if (!profile->IsOffTheRecord() && --browser_counts_[profile] == 0) {
    active_profiles_.erase(
        std::find(active_profiles_.begin(), active_profiles_.end(), profile));
    if (!closing_all_browsers_)
      SaveActiveProfiles();
  }

  Profile* original_profile = profile->GetOriginalProfile();
  // Do nothing if the closed window is not the last window of the same profile.
  for (auto* browser_iter : *BrowserList::GetInstance()) {
    if (browser_iter->profile()->GetOriginalProfile() == original_profile)
      return;
  }

  base::FilePath path = profile->GetPath();
  if (IsProfileDirectoryMarkedForDeletion(path)) {
    // Do nothing if the profile is already being deleted.
  } else if (profile->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles)) {
    // Delete if the profile is an ephemeral profile.
    ScheduleForcedEphemeralProfileForDeletion(path);
  }
}

void ProfileManager::UpdateLastUser(Profile* last_active) {
  // The profile may incorrectly become "active" during its destruction, caused
  // by the UI teardown. See https://crbug.com/1073451
  if (IsProfileDirectoryMarkedForDeletion(last_active->GetPath()))
    return;

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  // Only keep track of profiles that we are managing; tests may create others.
  // Also never consider the SystemProfile as "active".
  if (profiles_info_.find(last_active->GetPath()) != profiles_info_.end() &&
      !last_active->IsSystemProfile()) {
    std::string profile_path_base =
        last_active->GetPath().BaseName().MaybeAsASCII();
    if (profile_path_base != GetLastUsedProfileName())
      profiles::SetLastUsedProfile(profile_path_base);

    ProfileAttributesEntry* entry;
    if (GetProfileAttributesStorage().
            GetProfileAttributesWithPath(last_active->GetPath(), &entry)) {
      entry->SetActiveTimeToNow();
    }
  }
}

ProfileManager::BrowserListObserver::BrowserListObserver(
    ProfileManager* manager)
    : profile_manager_(manager) {
  BrowserList::AddObserver(this);
}

ProfileManager::BrowserListObserver::~BrowserListObserver() {
  BrowserList::RemoveObserver(this);
}

void ProfileManager::BrowserListObserver::OnBrowserAdded(Browser* browser) {
  profile_manager_->OnBrowserOpened(browser);
}

void ProfileManager::BrowserListObserver::OnBrowserRemoved(
    Browser* browser) {
  profile_manager_->OnBrowserClosed(browser);
}

void ProfileManager::BrowserListObserver::OnBrowserSetLastActive(
    Browser* browser) {
  // If all browsers are being closed (e.g. the user is in the process of
  // shutting down), this event will be fired after each browser is
  // closed. This does not represent a user intention to change the active
  // browser so is not handled here.
  if (profile_manager_->closing_all_browsers_)
    return;

  Profile* last_active = browser->profile();

  // Don't remember ephemeral profiles as last because they are not going to
  // persist after restart.
  if (last_active->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles))
    return;

  profile_manager_->UpdateLastUser(last_active);
}

void ProfileManager::OnNewActiveProfileLoaded(
    const base::FilePath& profile_to_delete_path,
    const base::FilePath& new_active_profile_path,
    ProfileLoadedCallback callback,
    Profile* loaded_profile,
    Profile::CreateStatus status) {
  DCHECK(status != Profile::CREATE_STATUS_LOCAL_FAIL &&
         status != Profile::CREATE_STATUS_REMOTE_FAIL);

  // Only run the code if the profile initialization has finished completely.
  if (status != Profile::CREATE_STATUS_INITIALIZED)
    return;

  if (IsProfileDirectoryMarkedForDeletion(new_active_profile_path)) {
    // If the profile we tried to load as the next active profile has been
    // deleted, then retry deleting this profile to redo the logic to load
    // the next available profile.
    EnsureActiveProfileExistsBeforeDeletion(std::move(callback),
                                            profile_to_delete_path);
    return;
  }

  FinishDeletingProfile(profile_to_delete_path, new_active_profile_path);
  std::move(callback).Run(loaded_profile);
}

void ProfileManager::ScheduleForcedEphemeralProfileForDeletion(
    const base::FilePath& profile_dir) {
  DCHECK_EQ(0u, chrome::GetBrowserCount(GetProfileByPath(profile_dir)));
  DCHECK(IsProfileEphemeral(&GetProfileAttributesStorage(), profile_dir));

  // Search for latest active profile, already loaded preferably.
  bool found_entry_loaded = false;
  ProfileAttributesEntry* found_entry = nullptr;
  ProfileAttributesStorage& storage = GetProfileAttributesStorage();
  for (ProfileAttributesEntry* entry : storage.GetAllProfilesAttributes()) {
    // Skip all profiles forbidden to rollback.
    base::FilePath entry_path = entry->GetPath();
    if (entry_path == profile_dir ||
        entry_path == GetGuestProfilePath() ||
        entry->IsLegacySupervised() ||
        IsProfileDirectoryMarkedForDeletion(entry_path))
      continue;
    // Check if |entry| preferable over |found_entry|.
    bool entry_loaded = !!GetProfileByPath(entry_path);
    if (!found_entry || (!found_entry_loaded && entry_loaded) ||
        found_entry->GetActiveTime() < entry->GetActiveTime()) {
      found_entry = entry;
      found_entry_loaded = entry_loaded;
    }
  }

  RemoveFromLastActiveProfilesPrefList(profile_dir);

  const base::FilePath new_active_profile_dir =
      found_entry ? found_entry->GetPath() : GenerateNextProfileDirectoryPath();
  FinishDeletingProfile(profile_dir, new_active_profile_dir);
}
#endif  // !defined(OS_ANDROID)

ProfileManagerWithoutInit::ProfileManagerWithoutInit(
    const base::FilePath& user_data_dir) : ProfileManager(user_data_dir) {
  set_do_final_services_init(false);
}
