// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_manager.h"

#include <stdint.h>

#include <cstddef>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/accessibility/accessibility_labels_service.h"
#include "chrome/browser/accessibility/accessibility_labels_service_factory.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_lifetime_manager.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_lifetime_manager_factory.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/permissions/adaptive_quiet_notification_permission_ui_enabler.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/primary_account_policy_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_id/account_id.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search_engines/default_search_manager.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/sync/base/stop_source.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/session_service_factory.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/live_caption/live_caption_controller.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_management_transition.h"
#include "ash/constants/ash_switches.h"
#include "base/debug/dump_without_crashing.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/account_manager/account_manager_policy_controller_factory.h"
#include "chrome/browser/ash/account_manager/child_account_type_changed_user_data.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/profiles/profile_statistics.h"
#include "chrome/browser/profiles/profile_statistics_factory.h"
#endif

#if BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/signin_util_win.h"
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/account_manager/account_profile_mapper.h"
#include "chrome/browser/ui/startup/first_run_service.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/contact_center_insights/contact_center_insights_extension_manager.h"
#include "chrome/browser/chromeos/extensions/desk_api/desk_api_extension_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

using content::BrowserThread;

namespace {

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

#if BUILDFLAG(ENABLE_EXTENSIONS)

// Returns the number of installed (and enabled) apps, excluding any component
// apps.
size_t GetEnabledAppCount(Profile* profile) {
  if (extensions::ChromeContentBrowserClientExtensionsPart::
          AreExtensionsDisabledForProfile(profile)) {
    return 0u;
  }

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  DCHECK(registry);

  size_t installed_apps = 0u;
  const extensions::ExtensionSet& extensions = registry->enabled_extensions();
  for (extensions::ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end(); ++iter) {
    if ((*iter)->is_app() &&
        (*iter)->location() !=
            extensions::mojom::ManifestLocation::kComponent) {
      ++installed_apps;
    }
  }
  return installed_apps;
}

#endif  // ENABLE_EXTENSIONS

// Once a profile is initialized through LoadProfile this method is executed.
// It will then run |client_callback| with the right profile.
void OnProfileInitialized(ProfileManager::ProfileLoadedCallback client_callback,
                          bool incognito,
                          Profile* profile) {
  if (!profile) {
    LOG(WARNING) << "Profile not loaded correctly";
    std::move(client_callback).Run(nullptr);
    return;
  }

  std::move(client_callback)
      .Run(incognito ? profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)
                     : profile);
}

// Helper function for `ProfileManager` to identify if a profile is ephemeral.
bool IsRegisteredAsEphemeral(ProfileAttributesStorage* storage,
                             const base::FilePath& profile_dir) {
  ProfileAttributesEntry* entry =
      storage->GetProfileAttributesWithPath(profile_dir);
  return entry && entry->IsEphemeral();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsLoggedIn() {
  return user_manager::UserManager::IsInitialized() &&
         user_manager::UserManager::Get()->IsUserLoggedIn();
}
#endif

bool IsForceEphemeralProfilesEnabled(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles);
}

int GetTotalRefCount(const std::map<ProfileKeepAliveOrigin, int>& keep_alives) {
  return std::accumulate(
      keep_alives.begin(), keep_alives.end(), 0,
      [](int acc, const auto& pair) { return acc + pair.second; });
}

// Outputs the state of ProfileInfo::keep_alives, for easier debugging. e.g.,
// a Profile with 3 regular windows open, and one Incognito window open would
// write this string:
//    [kBrowserWindow (3), kOffTheRecordProfile (1)]
std::ostream& operator<<(
    std::ostream& out,
    const std::map<ProfileKeepAliveOrigin, int>& keep_alives) {
  out << "[";
  bool first = true;
  for (const auto& pair : keep_alives) {
    if (pair.second == 0)
      continue;
    if (!first)
      out << ", ";
    out << pair.first << " (" << pair.second << ")";
    first = false;
  }
  out << "]";
  return out;
}

#if BUILDFLAG(IS_CHROMEOS)
void UpdateSupervisedUserPref(Profile* profile, bool is_child) {
  DCHECK(profile);
  if (is_child) {
    profile->GetPrefs()->SetString(prefs::kSupervisedUserId,
                                   supervised_user::kChildAccountSUID);
  } else {
    profile->GetPrefs()->ClearPref(prefs::kSupervisedUserId);
  }
}

absl::optional<bool> IsUserChild(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  return user ? absl::make_optional(user->GetType() ==
                                    user_manager::USER_TYPE_CHILD)
              : absl::nullopt;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->SessionType() ==
         crosapi::mojom::SessionType::kChildSession;
#endif
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void RunCallbacks(std::vector<base::OnceCallback<void(Profile*)>>& callbacks,
                  Profile* profile) {
  for (base::OnceCallback<void(Profile*)>& callback : callbacks)
    std::move(callback).Run(profile);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
void ClearPrimaryAccountForProfile(
    base::WeakPtr<Profile> weak_profile,
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric) {
  Profile* profile = weak_profile.get();
  if (!profile)
    return;

  IdentityManagerFactory::GetForProfile(profile)
      ->GetPrimaryAccountMutator()
      ->ClearPrimaryAccount(signout_source_metric, signout_delete_metric);
}
#endif

}  // namespace

ProfileManager::ProfileManager(const base::FilePath& user_data_dir)
    : user_data_dir_(user_data_dir)
#if !BUILDFLAG(IS_ANDROID)
      ,
      delete_profile_helper_(std::make_unique<DeleteProfileHelper>(*this))
#endif
{
#if !BUILDFLAG(IS_ANDROID)
  closing_all_browsers_subscription_ = chrome::AddClosingAllBrowsersCallback(
      base::BindRepeating(&ProfileManager::OnClosingAllBrowsersChanged,
                          base::Unretained(this)));
#endif

  if (ProfileShortcutManager::IsFeatureEnabled() && !user_data_dir_.empty())
    profile_shortcut_manager_ = ProfileShortcutManager::Create(this);

  zombie_metrics_timer_.Start(FROM_HERE, base::Minutes(30), this,
                              &ProfileManager::RecordZombieMetrics);
}

ProfileManager::~ProfileManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& observer : observers_) {
    observer.OnProfileManagerDestroying();
  }

  base::UmaHistogramBoolean("Profile.DidDestroyProfileBeforeShutdown",
                            could_have_destroyed_profile_);
  if (base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose)) {
    // Ideally, all the keepalives should've been cleared already. Report
    // metrics for incorrect usage of ScopedProfileKeepAlive.
    for (const auto& path_and_profile_info : profiles_info_) {
      const ProfileInfo* profile_info = path_and_profile_info.second.get();

      Profile* profile = profile_info->GetRawProfile();
      if (profile && !profile->IsRegularProfile())
        continue;

      for (const auto& origin_and_count : profile_info->keep_alives) {
        ProfileKeepAliveOrigin origin = origin_and_count.first;
        int count = origin_and_count.second;
        if (count > 0) {
          UMA_HISTOGRAM_ENUMERATION("Profile.KeepAliveLeakAtShutdown", origin);
        }
      }
    }
  }

  profiles_info_.clear();
  ProfileDestroyer::DestroyPendingProfilesForShutdown();
}

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
// static
void ProfileManager::ShutdownSessionServices() {
  ProfileManager* pm = g_browser_process->profile_manager();
  if (!pm)  // Is nullptr when running unit tests.
    return;
  for (auto* profile : pm->GetLoadedProfiles()) {
    // Don't construct SessionServices for every type just to
    // shut them down. If they were never created, just skip.
    if (SessionServiceFactory::GetForProfileIfExisting(profile))
      SessionServiceFactory::ShutdownForProfile(profile);
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
    if (AppSessionServiceFactory::GetForProfileIfExisting(profile))
      AppSessionServiceFactory::ShutdownForProfile(profile);
#endif
  }
}
#endif

// static
Profile* ProfileManager::GetLastUsedProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)  // Can be null in unit tests.
    return nullptr;

#if BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Use default login profile if user has not logged in yet.
  if (!IsLoggedIn())
    return profile_manager->GetActiveUserOrOffTheRecordProfile();

  // CrOS multi-profiles implementation is different so GetLastUsedProfile()
  // has custom implementation too.
  // In case of multi-profiles we ignore "last used profile" preference
  // since it may refer to profile that has been in use in previous session.
  // That profile dir may not be mounted in this session so instead return
  // active profile from current session.
  user_manager::UserManager* manager = user_manager::UserManager::Get();
  // IsLoggedIn check above ensures |user| is non-null.
  const auto* user = manager->GetActiveUser();
  Profile* profile = profile_manager->GetProfileByPath(
      ash::BrowserContextHelper::Get()->GetBrowserContextPathByUserIdHash(
          user->username_hash()));
#else
  // TODO(crbug.com/1363933): Once Lacros is launched pre-login, we should
  // probably do something analogous to the !IsLoggedIn() check above.
  Profile* profile =
      profile_manager->GetProfile(profile_manager->GetLastUsedProfileDir());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Accessing a user profile before it is loaded may lead to policy exploit.
  // See http://crbug.com/689206.
  LOG_IF(FATAL, !profile) << "Calling GetLastUsedProfile() before profile "
                          << "initialization is completed.";

  return profile->IsGuestSession()
             ? profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)
             : profile;
#else
  return profile_manager->GetProfile(profile_manager->GetLastUsedProfileDir());
#endif  // BUILDFLAG(IS_CHROMEOS)
}

// static
Profile* ProfileManager::GetLastUsedProfileIfLoaded() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)  // Can be null in unit tests.
    return nullptr;
  return profile_manager->GetProfileByPath(
      profile_manager->GetLastUsedProfileDir());
}

// static
Profile* ProfileManager::GetLastUsedProfileAllowedByPolicy() {
  return MaybeForceOffTheRecordMode(GetLastUsedProfile());
}

// static
Profile* ProfileManager::MaybeForceOffTheRecordMode(Profile* profile) {
  if (!profile)
    return nullptr;
  if (profile->IsGuestSession() || profile->IsSystemProfile() ||
      IncognitoModePrefs::GetAvailability(profile->GetPrefs()) ==
          IncognitoModePrefs::Availability::kForced) {
    return profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }
  return profile;
}

// static
std::vector<Profile*> ProfileManager::GetLastOpenedProfiles() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(profile_manager);
  DCHECK(local_state);

  std::vector<Profile*> to_return;
  if (local_state->HasPrefPath(prefs::kProfilesLastActive)) {
    // Make a copy because the list might change in the calls to GetProfile.
    const base::Value::List profile_list =
        local_state->GetList(prefs::kProfilesLastActive).Clone();
    for (const auto& entry : profile_list) {
      const std::string* profile_base_name = entry.GetIfString();
      if (!profile_base_name || profile_base_name->empty() ||
          *profile_base_name ==
              base::FilePath(chrome::kSystemProfileDir).AsUTF8Unsafe()) {
        LOG(WARNING) << "Invalid entry in " << prefs::kProfilesLastActive;
        continue;
      }
      Profile* profile =
          profile_manager->GetProfile(profile_manager->user_data_dir().Append(
              base::FilePath::FromUTF8Unsafe(*profile_base_name)));
      if (profile) {
        // crbug.com/823338 -> CHECK that the profiles aren't guest or
        // incognito, causing a crash during session restore.
        CHECK(!profile->IsGuestSession())
            << "Guest profiles shouldn't have been saved as active profiles";
        CHECK(!profile->IsOffTheRecord())
            << "OTR profiles shouldn't have been saved as active profiles";
        // TODO(rsult): If this DCHECK is never hit, turn it into a CHECK.
        DCHECK((!profile->IsSystemProfile()))
            << "System profile shouldn't have been saved as active profiles.";

        to_return.push_back(profile);
      }
    }
  }
  return to_return;
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
// static
Profile* ProfileManager::GetPrimaryUserProfile() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (IsLoggedIn()) {
    user_manager::UserManager* manager = user_manager::UserManager::Get();
    const user_manager::User* user = manager->GetPrimaryUser();
    if (!user)  // Can be null in unit tests.
      return nullptr;

    if (user->is_profile_created()) {
      // Note: The ProfileHelper will take care of guest profiles.
      return ash::ProfileHelper::Get()->GetProfileByUser(user);
    }

    LOG(ERROR) << "ProfileManager::GetPrimaryUserProfile is called when "
                  "|user| is created but |user|'s profile is not yet created. "
                  "It probably means that something is wrong with a calling "
                  "code. Please report in http://crbug.com/361528 if you see "
                  "this message.";

    // Taking metrics to make sure this code path is not used in production.
    // TODO(crbug.com/1325210): Remove the following code, once we made sure
    // they are not used in the production.
    if (base::SysInfo::IsRunningOnChromeOS()) {
      base::UmaHistogramBoolean(
          "Ash.BrowserContext.UnexpectedGetPrimaryUserProfile", true);
      // Also taking the stack trace, so we can identify who's the caller on
      // unexpected cases.
      base::debug::DumpWithoutCrashing();
    }

    Profile* profile = ProfileManager::GetActiveUserProfile();
    if (profile && manager->IsLoggedInAsGuest())
      profile = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    return profile;
  }
#endif

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)  // Can be null in unit tests.
    return nullptr;

  return profile_manager->GetActiveUserOrOffTheRecordProfile();
}

// static
Profile* ProfileManager::GetActiveUserProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
      return ash::ProfileHelper::Get()->GetProfileByUser(user);
  }
#endif
  Profile* profile = profile_manager->GetActiveUserOrOffTheRecordProfile();
  // |profile| could be null if the user doesn't have a profile yet and the path
  // is on a read-only volume (preventing Chrome from making a new one).
  // However, most callers of this function immediately dereference the result
  // which would lead to crashes in a variety of call sites. Assert here to
  // figure out how common this is. http://crbug.com/383019
  CHECK(profile) << profile_manager->user_data_dir().AsUTF8Unsafe();
  return profile;
}
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)
// static
Profile* ProfileManager::CreateInitialProfile() {
  ProfileManager* const profile_manager = g_browser_process->profile_manager();
  Profile* profile =
      profile_manager->GetProfile(profile_manager->user_data_dir().Append(
          profile_manager->GetInitialProfileDir()));

  if (profile_manager->ShouldGoOffTheRecord(profile))
    return profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  return profile;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)

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

bool ProfileManager::LoadProfile(const base::FilePath& profile_base_name,
                                 bool incognito,
                                 ProfileLoadedCallback callback) {
  const base::FilePath profile_path = user_data_dir().Append(profile_base_name);
  return LoadProfileByPath(profile_path, incognito, std::move(callback));
}

bool ProfileManager::LoadProfileByPath(const base::FilePath& profile_path,
                                       bool incognito,
                                       ProfileLoadedCallback callback) {
  ProfileAttributesEntry* entry =
      GetProfileAttributesStorage().GetProfileAttributesWithPath(profile_path);
  if (!entry) {
    std::move(callback).Run(nullptr);
    LOG(ERROR) << "Loading a profile path that does not exist";
    return false;
  }
  CreateProfileAsync(
      profile_path,
      base::BindOnce(&OnProfileInitialized, std::move(callback), incognito));
  return true;
}

void ProfileManager::CreateProfileAsync(
    const base::FilePath& profile_path,
    base::OnceCallback<void(Profile*)> initialized_callback,
    base::OnceCallback<void(Profile*)> created_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT1("browser,startup", "ProfileManager::CreateProfileAsync",
               "profile_path", profile_path.AsUTF8Unsafe());

  if (!CanCreateProfileAtPath(profile_path)) {
    if (!initialized_callback.is_null())
      std::move(initialized_callback).Run(nullptr);
    return;
  }

  // Create the profile if needed and collect its ProfileInfo.
  auto iter = profiles_info_.find(profile_path);
  ProfileInfo* info = nullptr;

  if (iter != profiles_info_.end()) {
    info = iter->second.get();
  } else {
    // Initiate asynchronous creation process.
    info = RegisterOwnedProfile(CreateProfileAsyncHelper(profile_path));
  }

  // Call or enqueue the callback.
  if (!initialized_callback.is_null() || !created_callback.is_null()) {
    if (iter != profiles_info_.end() && info->GetCreatedProfile()) {
      Profile* profile = info->GetCreatedProfile();
      // If this was the Guest profile, apply settings and go OffTheRecord.
      // The system profile also needs characteristics of being off the record,
      // such as having no extensions, not writing to disk, etc.
      if (profile->IsGuestSession() || profile->IsSystemProfile()) {
        SetNonPersonalProfilePrefs(profile);
        profile = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
      }

      // Profile has already been created. Run callback immediately.
      if (!initialized_callback.is_null())
        std::move(initialized_callback).Run(profile);
    } else {
      // Profile is either already in the process of being created, or new.
      // Add callback to the list.
      if (!initialized_callback.is_null())
        info->init_callbacks.push_back(std::move(initialized_callback));
      if (!created_callback.is_null())
        info->created_callbacks.push_back(std::move(created_callback));
    }
  }
}

bool ProfileManager::IsValidProfile(const void* profile) {
  for (auto iter = profiles_info_.begin(); iter != profiles_info_.end();
       ++iter) {
    Profile* candidate = iter->second->GetCreatedProfile();
    if (!candidate)
      continue;
    if (candidate == profile)
      return true;
    std::vector<Profile*> otr_profiles =
        candidate->GetAllOffTheRecordProfiles();
    if (base::Contains(otr_profiles, profile))
      return true;
  }
  return false;
}

base::FilePath ProfileManager::GetInitialProfileDir() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (IsLoggedIn()) {
    user_manager::UserManager* manager = user_manager::UserManager::Get();
    // IsLoggedIn check above ensures |user| is non-null.
    const auto* user = manager->GetActiveUser();
    return base::FilePath(
        ash::BrowserContextHelper::GetUserBrowserContextDirName(
            user->username_hash()));
  }
#endif
  base::FilePath relative_profile_dir;
  // TODO(mirandac): should not automatically be default profile.
  return relative_profile_dir.AppendASCII(chrome::kInitialProfile);
}

base::FilePath ProfileManager::GetLastUsedProfileDir() {
  return user_data_dir_.Append(GetLastUsedProfileBaseName());
}

// static
base::FilePath ProfileManager::GetLastUsedProfileBaseName() {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  base::FilePath last_used_profile_base_name =
      local_state->GetFilePath(prefs::kProfileLastUsed);
  // Make sure the system profile can't be the one marked as the last one used
  // since it shouldn't get a browser.
  if (!last_used_profile_base_name.empty() &&
      last_used_profile_base_name.value() != chrome::kSystemProfileDir) {
    return last_used_profile_base_name;
  }

  return base::FilePath::FromASCII(chrome::kInitialProfile);
}

base::FilePath ProfileManager::GetProfileDirForEmail(const std::string& email) {
  for (const auto* entry :
       GetProfileAttributesStorage().GetAllProfilesAttributes()) {
    if (gaia::AreEmailsSame(base::UTF16ToUTF8(entry->GetUserName()), email))
      return entry->GetPath();
  }
  return base::FilePath();
}

std::vector<Profile*> ProfileManager::GetLoadedProfiles() const {
  std::vector<Profile*> profiles;
  for (auto iter = profiles_info_.begin(); iter != profiles_info_.end();
       ++iter) {
    Profile* profile = iter->second->GetCreatedProfile();
    if (profile)
      profiles.push_back(profile);
  }
  return profiles;
}

Profile* ProfileManager::GetProfileByPathInternal(
    const base::FilePath& path) const {
  TRACE_EVENT0("browser", "ProfileManager::GetProfileByPathInternal");
  ProfileInfo* profile_info = GetProfileInfoByPath(path);
  return profile_info ? profile_info->GetRawProfile() : nullptr;
}

bool ProfileManager::IsAllowedProfilePath(const base::FilePath& path) const {
  return path.DirName() == user_data_dir();
}

bool ProfileManager::CanCreateProfileAtPath(const base::FilePath& path) const {
  bool is_allowed_path = IsAllowedProfilePath(path) ||
                         base::CommandLine::ForCurrentProcess()->HasSwitch(
                             switches::kAllowProfilesOutsideUserDir);

  if (!is_allowed_path) {
    LOG(ERROR) << "Cannot create profile at path " << path.AsUTF8Unsafe();
    return false;
  }

#if !BUILDFLAG(IS_ANDROID)
  if (IsProfileDirectoryMarkedForDeletion(path))
    return false;
#endif

  return true;
}

Profile* ProfileManager::GetProfileByPath(const base::FilePath& path) const {
  TRACE_EVENT0("browser", "ProfileManager::GetProfileByPath");
  ProfileInfo* profile_info = GetProfileInfoByPath(path);
  return profile_info ? profile_info->GetCreatedProfile() : nullptr;
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

std::map<ProfileKeepAliveOrigin, int> ProfileManager::GetKeepAlivesByPath(
    const base::FilePath& path) {
  ProfileInfo* profile_info = GetProfileInfoByPath(path);
  return profile_info ? profile_info->keep_alives
                      : std::map<ProfileKeepAliveOrigin, int>();
}

#if !BUILDFLAG(IS_ANDROID)
// static
void ProfileManager::CreateMultiProfileAsync(
    const std::u16string& name,
    size_t icon_index,
    bool is_hidden,
    base::OnceCallback<void(Profile*)> initialized_callback,
    base::OnceCallback<void(Profile*)> created_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!name.empty());
  DCHECK(profiles::IsDefaultAvatarIconIndex(icon_index));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  base::FilePath new_path;
  ProfileAttributesEntry* entry = nullptr;
  // In some crashes, it can happen that next profile dir (backed up by local
  // pref) is out-of-sync with profiles dir. Keep generating next profile path
  // until that path is _not_ present in `storage`.
  do {
    new_path = profile_manager->GenerateNextProfileDirectoryPath();
    // The generated path should be unused and free to use.
    DCHECK_EQ(profile_manager->GetProfileByPath(new_path), nullptr);
    DCHECK(profile_manager->CanCreateProfileAtPath(new_path));

    entry = storage.GetProfileAttributesWithPath(new_path);
  } while (entry != nullptr);

  // Add a storage entry early here to set up a new profile with user selected
  // name and avatar.
  // These parameters will be used to initialize profile's prefs in
  // InitProfileUserPrefs(). AddProfileToStorage() will set any missing
  // attributes after prefs are loaded.
  // TODO(alexilin): consider using the user data to supply these parameters
  // to profile.
  ProfileAttributesInitParams init_params;
  init_params.profile_path = new_path;
  init_params.profile_name = name;
  init_params.icon_index = icon_index;
  init_params.is_ephemeral = is_hidden;
  init_params.is_omitted = is_hidden;
  storage.AddProfile(std::move(init_params));

  if (!base::FeatureList::IsEnabled(
          features::kNukeProfileBeforeCreateMultiAsync)) {
    profile_manager->CreateProfileAsync(
        new_path, std::move(initialized_callback), std::move(created_callback));
    return;
  }

  // As another check, make sure the generated path is not present in the file
  // system (there could be orphan profile dirs).
  // TODO(crbug.com/1277948): There can be a theoretical race condition with a
  // direct CreateProfileAsync() call that can create the directory before
  // adding an entry to ProfileAttributesStorage. Creating a new
  // ProfileAttributesEntry consistently before writing the profile folder to
  // disk would resolve this.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&NukeProfileFromDisk, new_path,
                     base::BindOnce(&ProfileManager::CreateProfileAsync,
                                    profile_manager->weak_factory_.GetWeakPtr(),
                                    new_path, std::move(initialized_callback),
                                    std::move(created_callback))));
}
#endif  // !BUILDFLAG(IS_ANDROID)

// static
base::FilePath ProfileManager::GetGuestProfilePath() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath guest_path = profile_manager->user_data_dir();
  return guest_path.Append(chrome::kGuestProfileDir);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
// static
base::FilePath ProfileManager::GetSystemProfilePath() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath system_path = profile_manager->user_data_dir();
  return system_path.Append(chrome::kSystemProfileDir);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// static
base::FilePath ProfileManager::GetPrimaryUserProfilePath() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->user_data_dir().Append(
      profile_manager->GetInitialProfileDir());
}
#endif

base::FilePath ProfileManager::GenerateNextProfileDirectoryPath() {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  DCHECK(profiles::IsMultipleProfilesEnabled());

  // Create the next profile in the next available directory slot.
  int next_directory = local_state->GetInteger(prefs::kProfilesNumCreated);
  std::string profile_name = chrome::kMultiProfileDirPrefix;
  profile_name.append(base::NumberToString(next_directory));
  base::FilePath new_path = user_data_dir_;
#if BUILDFLAG(IS_WIN)
  new_path = new_path.Append(base::ASCIIToWide(profile_name));
#else
  new_path = new_path.Append(profile_name);
#endif
  local_state->SetInteger(prefs::kProfilesNumCreated, ++next_directory);
  return new_path;
}

ProfileAttributesStorage& ProfileManager::GetProfileAttributesStorage() {
  TRACE_EVENT0("browser", "ProfileManager::GetProfileAttributesStorage");
  if (!profile_attributes_storage_) {
    profile_attributes_storage_ = std::make_unique<ProfileAttributesStorage>(
        g_browser_process->local_state(), user_data_dir_);
  }
  return *profile_attributes_storage_.get();
}

ProfileShortcutManager* ProfileManager::profile_shortcut_manager() {
  return profile_shortcut_manager_.get();
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
AccountProfileMapper* ProfileManager::GetAccountProfileMapper() {
  if (!account_profile_mapper_) {
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        GetAccountManagerFacade(/*profile_path=*/std::string()),
        &GetProfileAttributesStorage(), g_browser_process->local_state());
  }
  return account_profile_mapper_.get();
}
#endif

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

void ProfileManager::InitProfileUserPrefs(Profile* profile) {
  TRACE_EVENT0("browser", "ProfileManager::InitProfileUserPrefs");
  ProfileAttributesStorage& storage = GetProfileAttributesStorage();

  if (!IsAllowedProfilePath(profile->GetPath())) {
    LOG(WARNING) << "Failed to initialize prefs for a profile at invalid path: "
                 << profile->GetPath().AsUTF8Unsafe();
    return;
  }

  // User type can change during online sign in on Chrome OS. Propagate the
  // change to the profile in both Ash and LaCrOS and remove stored profile
  // attributes so they can be re-initialized later.
#if BUILDFLAG(IS_CHROMEOS)
  const absl::optional<bool> user_is_child = IsUserChild(profile);
  const bool profile_is_new = profile->IsNewProfile();
  const bool profile_is_child = profile->IsChild();
  const bool did_supervised_status_change =
      !profile_is_new && user_is_child.has_value() &&
      profile_is_child != user_is_child.value();

  if (user_is_child.has_value()) {
    if (did_supervised_status_change) {
      ProfileAttributesEntry* entry =
          storage.GetProfileAttributesWithPath(profile->GetPath());
      if (entry)
        storage.RemoveProfile(profile->GetPath());
    }
    UpdateSupervisedUserPref(profile, user_is_child.value());
  }
#endif

  // Additionally to propagation of the user type change to profile on Chrome
  // OS, Ash needs to propagate it to ARC++ and update secondary accounts.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (user_is_child.has_value()) {
    const bool profile_is_managed = !profile->IsOffTheRecord() &&
                                    arc::policy_util::IsAccountManaged(profile);

    if (did_supervised_status_change) {
      ash::ChildAccountTypeChangedUserData::GetForProfile(profile)->SetValue(
          true);
    } else {
      ash::ChildAccountTypeChangedUserData::GetForProfile(profile)->SetValue(
          false);
    }

    // Notify ARC about transition via prefs if needed.
    if (!profile_is_new) {
      const bool arc_is_managed =
          profile->GetPrefs()->GetBoolean(arc::prefs::kArcIsManaged);
      const bool arc_is_managed_set =
          profile->GetPrefs()->HasPrefPath(arc::prefs::kArcIsManaged);

      const bool arc_signed_in =
          profile->GetPrefs()->GetBoolean(arc::prefs::kArcSignedIn);

      arc::ArcManagementTransition transition;
      if (!arc_signed_in) {
        // No transition is necessary if user never enabled ARC.
        transition = arc::ArcManagementTransition::NO_TRANSITION;
      } else if (profile_is_child != user_is_child.value()) {
        transition = user_is_child.value()
                         ? arc::ArcManagementTransition::REGULAR_TO_CHILD
                         : arc::ArcManagementTransition::CHILD_TO_REGULAR;
      } else if (profile_is_managed && arc_is_managed_set && !arc_is_managed) {
        transition = arc::ArcManagementTransition::UNMANAGED_TO_MANAGED;
      } else {
        // User state has not changed.
        transition = arc::ArcManagementTransition::NO_TRANSITION;
      }

      profile->GetPrefs()->SetInteger(arc::prefs::kArcManagementTransition,
                                      static_cast<int>(transition));
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  size_t avatar_index;
  std::string profile_name;
  std::string supervised_user_id;
  if (profile->IsGuestSession()) {
    profile_name = l10n_util::GetStringUTF8(IDS_PROFILES_GUEST_PROFILE_NAME);
    avatar_index = 0;
  } else {
    ProfileAttributesEntry* entry =
        storage.GetProfileAttributesWithPath(profile->GetPath());
    // If the profile attributes storage has an entry for this profile, use the
    // data in the profile attributes storage.
    if (entry) {
      avatar_index = entry->GetAvatarIconIndex();
      profile_name = base::UTF16ToUTF8(entry->GetLocalProfileName());
      supervised_user_id = entry->GetSupervisedUserId();
    } else {
      avatar_index = profiles::GetPlaceholderAvatarIndex();
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
      profile_name =
          base::UTF16ToUTF8(storage.ChooseNameForNewProfile(avatar_index));
#else
      profile_name = l10n_util::GetStringUTF8(IDS_DEFAULT_PROFILE_NAME);
#endif
    }
  }

  // TODO(https://crbug.com/1194607): investigate whether these prefs are
  // actually useful.
  if (!profile->GetPrefs()->HasPrefPath(prefs::kProfileAvatarIndex))
    profile->GetPrefs()->SetInteger(prefs::kProfileAvatarIndex, avatar_index);

  if (!profile->GetPrefs()->HasPrefPath(prefs::kProfileName)) {
    profile->GetPrefs()->SetString(prefs::kProfileName, profile_name);
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool force_supervised_user_id =
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#if !BUILDFLAG(IS_ANDROID)
  if (profile->IsNewProfile()) {
    profile->GetPrefs()->SetBoolean(prefs::kHasSeenWelcomePage, false);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ProfileManager::RegisterTestingProfile(std::unique_ptr<Profile> profile,
                                            bool add_to_storage) {
  ProfileInfo* profile_info = RegisterOwnedProfile(std::move(profile));
  profile_info->MarkProfileAsCreated(profile_info->GetRawProfile());
  if (add_to_storage) {
    InitProfileUserPrefs(profile_info->GetCreatedProfile());
    AddProfileToStorage(profile_info->GetCreatedProfile());
  }
}

std::unique_ptr<Profile> ProfileManager::CreateProfileHelper(
    const base::FilePath& path) {
  TRACE_EVENT0("browser", "ProfileManager::CreateProfileHelper");

  return Profile::CreateProfile(path, this, Profile::CREATE_MODE_SYNCHRONOUS);
}

std::unique_ptr<Profile> ProfileManager::CreateProfileAsyncHelper(
    const base::FilePath& path) {
  return Profile::CreateProfile(path, this, Profile::CREATE_MODE_ASYNCHRONOUS);
}

bool ProfileManager::HasKeepAliveForTesting(const Profile* profile,
                                            ProfileKeepAliveOrigin origin) {
  DCHECK(profile);
  ProfileInfo* info = GetProfileInfoByPath(profile->GetPath());
  DCHECK(info);
  return info->keep_alives[origin] > 0;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void ProfileManager::SetAccountProfileMapperForTests(
    std::unique_ptr<AccountProfileMapper> mapper) {
  DCHECK(!account_profile_mapper_)
      << "AccountProfileMapper must be set before the first usage";
  account_profile_mapper_ = std::move(mapper);
}
#endif

void ProfileManager::DisableProfileMetricsForTesting() {
  zombie_metrics_timer_.Stop();
}

size_t ProfileManager::GetZombieProfileCount() const {
  size_t zombie_count = 0;
  for (const base::FilePath& dir : ever_loaded_profiles_) {
    const ProfileInfo* info = GetProfileInfoByPath(dir);
    if (!info || GetTotalRefCount(info->keep_alives) == 0)
      zombie_count++;
  }
  return zombie_count;
}

void ProfileManager::RecordZombieMetrics() {
  size_t zombie_count = GetZombieProfileCount();
  base::UmaHistogramCounts100("Profile.LiveProfileCount",
                              ever_loaded_profiles_.size() - zombie_count);
  base::UmaHistogramCounts100("Profile.ZombieProfileCount", zombie_count);
}

void ProfileManager::AddKeepAlive(const Profile* profile,
                                  ProfileKeepAliveOrigin origin) {
  DCHECK_NE(ProfileKeepAliveOrigin::kWaitingForFirstBrowserWindow, origin);

  CHECK(profile);
  CHECK(!profile->IsOffTheRecord());

  ProfileInfo* info = GetProfileInfoByPath(profile->GetPath());
  if (!info) {
    // Can be null in unit tests, when the Profile was not created via
    // ProfileManager.
    VLOG(1) << "AddKeepAlive(" << profile->GetDebugName() << ", " << origin
            << ") called before the Profile was added to the ProfileManager. "
            << "The keepalive was not added. This may cause a crash during "
            << "teardown. (except in unit tests, where Profiles may not be "
            << "registered with the ProfileManager)";
    return;
  }

  if (base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose)) {
    DCHECK_NE(0, GetTotalRefCount(info->keep_alives))
        << "AddKeepAlive() on a soon-to-be-deleted Profile is not allowed";
  }

  info->keep_alives[origin]++;

  VLOG(1) << "AddKeepAlive(" << profile->GetDebugName() << ", " << origin
          << "). keep_alives=" << info->keep_alives;

  if (origin == ProfileKeepAliveOrigin::kBrowserWindow ||
      (origin == ProfileKeepAliveOrigin::kProfilePickerView &&
       base::FeatureList::IsEnabled(features::kDestroySystemProfiles))) {
    ClearFirstBrowserWindowKeepAlive(profile);
  }
}

void ProfileManager::RemoveKeepAlive(const Profile* profile,
                                     ProfileKeepAliveOrigin origin) {
  DCHECK_NE(ProfileKeepAliveOrigin::kWaitingForFirstBrowserWindow, origin);

  CHECK(profile);
  CHECK(!profile->IsOffTheRecord());

  ProfileInfo* info = GetProfileInfoByPath(profile->GetPath());
  if (!info) {
    // Can be null in unit tests, when the Profile was not created via
    // ProfileManager.
    VLOG(1) << "RemoveKeepAlive(" << profile->GetDebugName() << ", " << origin
            << ") called before the Profile was added to the "
            << "ProfileManager. The keepalive was not removed. (this is OK in "
            << "unit tests, wheres Profile may not be registered with the "
            << "ProfileManager)";
    return;
  }

  DCHECK(base::Contains(info->keep_alives, origin));
  info->keep_alives[origin]--;
  DCHECK_LE(0, info->keep_alives[origin]);

  VLOG(1) << "RemoveKeepAlive(" << profile->GetDebugName() << ", " << origin
          << "). keep_alives=" << info->keep_alives;

  UnloadProfileIfNoKeepAlive(info);
}

void ProfileManager::ClearFirstBrowserWindowKeepAlive(const Profile* profile) {
  CHECK(profile);
  CHECK(!profile->IsOffTheRecord());

  ProfileInfo* info = GetProfileInfoByPath(profile->GetPath());
  DCHECK(info);

  int& waiting_for_first_browser_window =
      info->keep_alives[ProfileKeepAliveOrigin::kWaitingForFirstBrowserWindow];

  if (waiting_for_first_browser_window == 0)
    return;

  waiting_for_first_browser_window = 0;

  VLOG(1) << "ClearFirstBrowserWindowKeepAlive(" << profile->GetDebugName()
          << "). keep_alives=" << info->keep_alives;

  UnloadProfileIfNoKeepAlive(info);
}

void ProfileManager::NotifyOnProfileMarkedForPermanentDeletion(
    Profile* profile) {
  for (auto& observer : observers_)
    observer.OnProfileMarkedForPermanentDeletion(profile);
}

void ProfileManager::UnloadProfileIfNoKeepAlive(const ProfileInfo* info) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  if (GetTotalRefCount(info->keep_alives) != 0)
    return;

  could_have_destroyed_profile_ = true;

  // When DestroyProfileOnBrowserClose is disabled: record memory metrics, but
  // don't actually unload the Profile.
  if (!base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose))
    return;

  if (!info->GetCreatedProfile()) {
    NOTREACHED() << "Attempted to unload profile "
                 << info->GetRawProfile()->GetDebugName()
                 << " before it was loaded. This is not valid.";
  }

  VLOG(1) << "Unloading profile " << info->GetCreatedProfile()->GetDebugName();
  UnloadProfile(info->GetCreatedProfile()->GetPath());
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
}

void ProfileManager::DoFinalInit(ProfileInfo* profile_info,
                                 bool go_off_the_record) {
  TRACE_EVENT0("browser", "ProfileManager::DoFinalInit");

  Profile* profile = profile_info->GetRawProfile();
  DoFinalInitForServices(profile, go_off_the_record);
  AddProfileToStorage(profile);
  DoFinalInitLogging(profile);

  // Set the |created| flag now so that PROFILE_ADDED handlers can use
  // GetProfileByPath().
  //
  // TODO(nicolaso): De-spaghettify MarkProfileAsCreated() by only calling it
  // here, and nowhere else.
  profile_info->MarkProfileAsCreated(profile);

  for (auto& observer : observers_)
    observer.OnProfileAdded(profile);

  if (PrimaryAccountPolicyManager* primary_account_policy_manager =
          PrimaryAccountPolicyManagerFactory::GetForProfile(profile)) {
    primary_account_policy_manager->Initialize();
  }

#if !BUILDFLAG(IS_ANDROID)
  // The caret browsing command-line switch toggles caret browsing on
  // initially, but the user can still toggle it from there.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCaretBrowsing)) {
    profile->GetPrefs()->SetBoolean(prefs::kCaretBrowsingEnabled, true);
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  // Delete browsing data specified by the ClearBrowsingDataOnExitList policy
  // if they were not properly deleted on the last browser shutdown.
  auto* browsing_data_lifetime_manager =
      ChromeBrowsingDataLifetimeManagerFactory::GetForProfile(profile);
  if (browsing_data_lifetime_manager && !profile->IsOffTheRecord() &&
      profile->GetPrefs()->GetBoolean(
          browsing_data::prefs::kClearBrowsingDataOnExitDeletionPending)) {
    browsing_data_lifetime_manager->ClearBrowsingDataForOnExitPolicy(
        /*keep_browser_alive=*/false);
  }
}

void ProfileManager::DoFinalInitForServices(Profile* profile,
                                            bool go_off_the_record) {
  if (!do_final_services_init_ ||
      AreKeyedServicesDisabledForProfileByDefault(profile)) {
    return;
  }

  TRACE_EVENT0("browser", "ProfileManager::DoFinalInitForServices");

#if BUILDFLAG(ENABLE_EXTENSIONS)
  bool extensions_enabled = !go_off_the_record;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableLoginScreenApps) &&
      ash::ProfileHelper::IsSigninProfile(profile)) {
    extensions_enabled = true;
  }
  if (ash::ProfileHelper::IsLockScreenAppProfile(profile))
    extensions_enabled = true;
#endif
  extensions::ExtensionSystem::Get(profile)->InitForRegularProfile(
      extensions_enabled);

  // Set the block extensions bit on the ExtensionService. There likely are no
  // blockable extensions to block.
  ProfileAttributesEntry* entry =
      GetProfileAttributesStorage().GetProfileAttributesWithPath(
          profile->GetPath());
  if (entry && entry->IsSigninRequired()) {
    extensions::ExtensionSystem::Get(profile)
        ->extension_service()
        ->BlockAllExtensions();
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Ensure that the `ContactCenterInsightsExtensionManager` is instantiated
  // after other systems are set up and only when extensions are enabled for the
  // given profile. This is done in `ProfileManager` so we can repurpose the
  // same pre-conditional checks that are being used with other extension
  // components and we can maintain said order.
  if (extensions_enabled) {
    ::chromeos::ContactCenterInsightsExtensionManager::GetForProfile(profile);

    ::chromeos::DeskApiExtensionManager::GetForProfile(profile);
  }
#endif

#endif
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // Initialization needs to happen after extension system initialization (for
  // extension::ManagementPolicy) and InitProfileUserPrefs (for setting the
  // initializing the supervised flag if necessary).
  ChildAccountServiceFactory::GetForProfile(profile)->Init();
  SupervisedUserServiceFactory::GetForProfile(profile)->Init();
#endif

  // Ensure NavigationPredictorKeyedService is started.
  NavigationPredictorKeyedServiceFactory::GetForProfile(profile);

  IdentityManagerFactory::GetForProfile(profile)->OnNetworkInitialized();
  AccountReconcilorFactory::GetForProfile(profile);

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  BoundSessionCookieRefreshServiceFactory::GetForProfile(profile);
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

  // Initialization needs to happen after the browser context is available
  // because SyncService needs the URL context getter.
  UnifiedConsentServiceFactory::GetForProfile(profile);

#if BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_DICE_SUPPORT)
  signin_util::SigninWithCredentialProviderIfPossible(profile);
#endif

  auto* accessibility_service =
      AccessibilityLabelsServiceFactory::GetForProfile(profile);
  if (accessibility_service)
    accessibility_service->Init();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::AccountManagerPolicyControllerFactory::GetForBrowserContext(profile);
#endif

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
      base::Seconds(112));
}

ProfileManager::ProfileInfo::ProfileInfo() {
  // The profile should have a refcount >=1 until AddKeepAlive() is called.
  keep_alives[ProfileKeepAliveOrigin::kWaitingForFirstBrowserWindow] = 1;
}

ProfileManager::ProfileInfo::~ProfileInfo() {
  // Regardless of sync or async creation, we always take ownership right after
  // Profile::CreateProfile(). So we should always own the Profile by this
  // point.
  DCHECK(owned_profile_);
  DCHECK_EQ(owned_profile_.get(), unowned_profile_);
  unowned_profile_ = nullptr;
  ProfileDestroyer::DestroyOriginalProfileWhenAppropriate(
      std::move(owned_profile_));
}

// static
std::unique_ptr<ProfileManager::ProfileInfo>
ProfileManager::ProfileInfo::FromUnownedProfile(Profile* profile) {
  // ProfileInfo's constructor is private, can't make_unique().
  std::unique_ptr<ProfileInfo> info(new ProfileInfo());
  info->unowned_profile_ = profile;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Lacros primary profile should not be destroyed, this KeepAlive will be
  // set and not expected to be removed at any point. On Secondary profiles, we
  // do not set it to allow the profiles to be destroyed.
  if (base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose) &&
      Profile::IsMainProfilePath(info->unowned_profile_->GetPath())) {
    info->keep_alives[ProfileKeepAliveOrigin::kLacrosMainProfile] = 1;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  return info;
}

void ProfileManager::ProfileInfo::TakeOwnershipOfProfile(
    std::unique_ptr<Profile> profile) {
  DCHECK_EQ(unowned_profile_, profile.get());
  DCHECK(!owned_profile_);
  owned_profile_ = std::move(profile);
}

void ProfileManager::ProfileInfo::MarkProfileAsCreated(Profile* profile) {
  DCHECK_EQ(GetRawProfile(), profile);
  created_ = true;
}

Profile* ProfileManager::ProfileInfo::GetCreatedProfile() const {
  return created_ ? GetRawProfile() : nullptr;
}

Profile* ProfileManager::ProfileInfo::GetRawProfile() const {
  DCHECK(owned_profile_ == nullptr || owned_profile_.get() == unowned_profile_);
  return unowned_profile_;
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
Profile* ProfileManager::GetActiveUserOrOffTheRecordProfile() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!IsLoggedIn()) {
    base::FilePath default_profile_dir =
        profiles::GetDefaultProfileDir(user_data_dir_);
    Profile* profile = GetProfile(default_profile_dir);
    // For cros, return the OTR profile so we never accidentally keep
    // user data in an unencrypted profile. But doing this makes
    // many of the browser and ui tests fail. We do return the OTR profile
    // if the login-profile switch is passed so that we can test this.
    if (ShouldGoOffTheRecord(profile))
      return profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    DCHECK(!user_manager::UserManager::Get()->IsLoggedInAsGuest());
    return profile;
  }

  base::FilePath default_profile_dir =
      user_data_dir_.Append(GetInitialProfileDir());
  ProfileInfo* profile_info = GetProfileInfoByPath(default_profile_dir);
  // Fallback to default off-the-record profile, if user profile has not started
  // loading or has not fully loaded yet.
  if (!profile_info || !profile_info->GetCreatedProfile())
    default_profile_dir = profiles::GetDefaultProfileDir(user_data_dir_);

  Profile* profile = GetProfile(default_profile_dir);
  // Some unit tests didn't initialize the UserManager.
  if (user_manager::UserManager::IsInitialized() &&
      user_manager::UserManager::Get()->IsLoggedInAsGuest())
    return profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  return profile;
#else
  base::FilePath default_profile_dir =
      user_data_dir_.Append(GetInitialProfileDir());
  return GetProfile(default_profile_dir);
#endif
}
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

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

  ProfileInfo* profile_info = RegisterOwnedProfile(std::move(profile));
  profile_info->MarkProfileAsCreated(profile_info->GetRawProfile());

  InitProfileUserPrefs(profile_info->GetCreatedProfile());
  DoFinalInit(profile_info,
              ShouldGoOffTheRecord(profile_info->GetCreatedProfile()));
  return true;
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
void ProfileManager::UnloadProfile(const base::FilePath& profile_dir) {
  TRACE_EVENT0("browser", "ProfileManager::UnloadProfile");

  DCHECK(base::Contains(profiles_info_, profile_dir));

  bool ephemeral =
      IsRegisteredAsEphemeral(&GetProfileAttributesStorage(), profile_dir);
  bool marked_for_deletion = IsProfileDirectoryMarkedForDeletion(profile_dir);

  // Remove from |profiles_info_|, eventually causing the Profile object's
  // destruction.
  profiles_info_.erase(profile_dir);

  if (!ephemeral && !marked_for_deletion)
    return;

  // If the profile is ephemeral or deleted via ScheduleProfileForDeletion(),
  // also do some cleanup.

  // TODO(crbug.com/88586): There could still be pending tasks that write to
  // disk, and don't need the Profile. If they run after
  // NukeProfileFromDisk(), they may still leave files behind.
  //
  // TODO(crbug.com/1191455): This can also fail if an object is holding a lock
  // to a file in the profile directory. This happens flakily, e.g. with the
  // LevelDB for GCMStore. The locked files don't get deleted properly.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&NukeProfileFromDisk, profile_dir, base::OnceClosure()));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

Profile* ProfileManager::CreateAndInitializeProfile(
    const base::FilePath& profile_dir) {
  TRACE_EVENT0("browser", "ProfileManager::CreateAndInitializeProfile");

  if (!CanCreateProfileAtPath(profile_dir)) {
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

  // Place the unique_ptr inside ProfileInfo, which was added by
  // OnProfileCreationStarted().
  ProfileInfo* info = GetProfileInfoByPath(profile->GetPath());
  DCHECK(info);
  info->TakeOwnershipOfProfile(std::move(profile));
  info->MarkProfileAsCreated(info->GetRawProfile());
  Profile* profile_ptr = info->GetCreatedProfile();

  if (profile_ptr->IsGuestSession() || profile_ptr->IsSystemProfile())
    SetNonPersonalProfilePrefs(profile_ptr);

  bool go_off_the_record = ShouldGoOffTheRecord(profile_ptr);
  DoFinalInit(info, go_off_the_record);
  return profile_ptr;
}

void ProfileManager::OnProfileCreationFinished(Profile* profile,
                                               Profile::CreateMode create_mode,
                                               bool success,
                                               bool is_new_profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto iter = profiles_info_.find(profile->GetPath());
  DCHECK(iter != profiles_info_.end());
  ProfileInfo* info = iter->second.get();

  if (create_mode == Profile::CREATE_MODE_SYNCHRONOUS) {
    // Already initialized in OnProfileCreationStarted().
    // TODO(nicolaso): Figure out why this would break browser tests:
    //     DCHECK_EQ(profile, profiles_info_->GetCreatedProfile());
    return;
  }

  std::vector<base::OnceCallback<void(Profile*)>> created_callbacks;
  info->created_callbacks.swap(created_callbacks);

  std::vector<base::OnceCallback<void(Profile*)>> init_callbacks;
  info->init_callbacks.swap(init_callbacks);

  // Invoke CREATED callback for normal profiles.
  bool go_off_the_record = ShouldGoOffTheRecord(profile);
  if (success && !go_off_the_record)
    RunCallbacks(created_callbacks, profile);

  // Perform initialization.
  if (success) {
    DoFinalInit(info, go_off_the_record);
    if (go_off_the_record)
      profile = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
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
      RunCallbacks(created_callbacks, profile);
  }

  // Invoke INITIALIZED for all profiles.
  // Profile might be null, meaning that the creation failed.
  RunCallbacks(init_callbacks, profile);
}

void ProfileManager::OnProfileCreationStarted(Profile* profile,
                                              Profile::CreateMode create_mode) {
  if (create_mode == Profile::CREATE_MODE_ASYNCHRONOUS) {
    // Profile will be registered later, in CreateProfileAsync().
    return;
  }

  if (profiles_info_.find(profile->GetPath()) != profiles_info_.end())
    return;

  // Make sure the Profile is in |profiles_info_| early enough during Profile
  // initialization.
  RegisterUnownedProfile(profile);
}

#if !BUILDFLAG(IS_ANDROID)

absl::optional<base::FilePath> ProfileManager::FindLastActiveProfile(
    base::RepeatingCallback<bool(ProfileAttributesEntry*)> predicate) {
  bool found_entry_loaded = false;
  ProfileAttributesEntry* found_entry = nullptr;
  ProfileAttributesStorage& storage = GetProfileAttributesStorage();
  for (ProfileAttributesEntry* entry : storage.GetAllProfilesAttributes()) {
    // Skip all profiles forbidden to rollback.
    base::FilePath entry_path = entry->GetPath();
    if (!predicate.Run(entry) || entry_path == GetGuestProfilePath() ||
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
  return found_entry ? absl::optional<base::FilePath>(found_entry->GetPath())
                     : absl::nullopt;
}

// static
void ProfileManager::CleanUpGuestProfile() {
// ChromeOS handles guest data independently.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  Profile* profile =
      profile_manager->GetProfileByPath(profile_manager->GetGuestProfilePath());
  if (profile) {
    // Clear all browsing data once a Guest Session completes. The Guest
    // profile has BrowserContextKeyedServices that the ProfileDestroyer
    // can't delete it properly.
    profiles::RemoveBrowsingDataForProfile(GetGuestProfilePath());
  }
#endif  //! BUILDFLAG(IS_CHROMEOS_ASH)
}

DeleteProfileHelper& ProfileManager::GetDeleteProfileHelper() {
  return *delete_profile_helper_;
}

#endif  // !BUILDFLAG(IS_ANDROID)

ProfileManager::ProfileInfo* ProfileManager::RegisterOwnedProfile(
    std::unique_ptr<Profile> profile) {
  TRACE_EVENT0("browser", "ProfileManager::RegisterOwnedProfile");
  Profile* profile_ptr = profile.get();
  auto info = ProfileInfo::FromUnownedProfile(profile_ptr);
  info->TakeOwnershipOfProfile(std::move(profile));
  ProfileInfo* info_raw = info.get();
  profiles_info_.insert(
      std::make_pair(profile_ptr->GetPath(), std::move(info)));
  if (profile_ptr->IsRegularProfile())
    ever_loaded_profiles_.insert(profile_ptr->GetPath());
  return info_raw;
}

ProfileManager::ProfileInfo* ProfileManager::RegisterUnownedProfile(
    Profile* profile) {
  TRACE_EVENT0("browser", "ProfileManager::RegisterUnownedProfile");
  base::FilePath path = profile->GetPath();
  auto info = ProfileInfo::FromUnownedProfile(profile);
  ProfileInfo* info_raw = info.get();
  profiles_info_.insert(std::make_pair(path, std::move(info)));
  if (profile->IsRegularProfile())
    ever_loaded_profiles_.insert(path);
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
  bool is_consented_primary_account =
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync);
  CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  std::u16string username = base::UTF8ToUTF16(account_info.email);

  ProfileAttributesStorage& storage = GetProfileAttributesStorage();
  // |entry| below is put inside a pair of brackets for scoping, to avoid
  // potential clashes of variable names.
  {
    ProfileAttributesEntry* entry =
        storage.GetProfileAttributesWithPath(profile->GetPath());
    if (entry) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
      bool could_be_managed_status = entry->CanBeManaged();
#endif
      // The ProfileAttributesStorage's info must match the Identity Manager.
      entry->SetAuthInfo(account_info.gaia, username,
                         is_consented_primary_account);

      entry->SetSignedInWithCredentialProvider(profile->GetPrefs()->GetBoolean(
          prefs::kSignedInWithCredentialProvider));

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
      // Sign out if force-sign-in policy is enabled and profile is not signed
      // in.
      VLOG(1) << "ForceSigninCheck: " << signin_util::IsForceSigninEnabled()
              << ", " << could_be_managed_status << ", "
              << !entry->CanBeManaged();
      if (signin_util::IsForceSigninEnabled() && could_be_managed_status &&
          !entry->CanBeManaged()) {
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE,
            base::BindOnce(&ClearPrimaryAccountForProfile,
                           profile->GetWeakPtr(),
                           signin_metrics::ProfileSignout::
                               kAuthenticationFailedWithForceSignin,
                           signin_metrics::SignoutDelete::kIgnoreMetric));
      }
#endif
      return;
    }
  }

  ProfileAttributesInitParams init_params;
  init_params.profile_path = profile->GetPath();

  // Profile name and avatar are set by InitProfileUserPrefs and stored in the
  // profile. Use those values to setup the entry in profile attributes storage.
  init_params.profile_name =
      base::UTF8ToUTF16(profile->GetPrefs()->GetString(prefs::kProfileName));

  init_params.icon_index =
      profile->GetPrefs()->GetInteger(prefs::kProfileAvatarIndex);

  init_params.supervised_user_id =
      profile->GetPrefs()->GetString(prefs::kSupervisedUserId);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (user)
    init_params.account_id = user->GetAccountId();
#endif

  init_params.gaia_id = account_info.gaia;
  init_params.user_name = username;
  init_params.is_consented_primary_account = is_consented_primary_account;

  init_params.is_ephemeral = IsForceEphemeralProfilesEnabled(profile);
  init_params.is_signed_in_with_credential_provider =
      profile->GetPrefs()->GetBoolean(prefs::kSignedInWithCredentialProvider);

  storage.AddProfile(std::move(init_params));
}

void ProfileManager::SetNonPersonalProfilePrefs(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(prefs::kSigninAllowed, false);
  prefs->SetBoolean(bookmarks::prefs::kEditBookmarksEnabled, false);
  prefs->SetBoolean(bookmarks::prefs::kShowBookmarkBar, false);
  prefs->ClearPref(DefaultSearchManager::kDefaultSearchProviderDataPrefName);
}

bool ProfileManager::ShouldGoOffTheRecord(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!ash::ProfileHelper::IsUserProfile(profile)) {
    return true;
  }
#endif
  return profile->IsGuestSession() || profile->IsSystemProfile();
}

void ProfileManager::SaveActiveProfiles() {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  ScopedListPrefUpdate update(local_state, prefs::kProfilesLastActive);
  base::Value::List& profile_list = update.Get();

  profile_list.clear();

  // crbug.com/120112 -> several non-off-the-record profiles might have the same
  // GetBaseName(). In that case, we cannot restore both
  // profiles. Include each base name only once in the last active profile
  // list.
  std::set<base::FilePath> profile_paths;
  std::vector<Profile*>::const_iterator it;
  for (it = active_profiles_.begin(); it != active_profiles_.end(); ++it) {
    // crbug.com/823338 -> CHECK that the profiles aren't guest or incognito,
    // causing a crash during session restore.
    CHECK((!(*it)->IsGuestSession()))
        << "Guest profiles shouldn't be saved as active profiles";
    CHECK(!(*it)->IsOffTheRecord())
        << "OTR profiles shouldn't be saved as active profiles";
    // TODO(rsult): If this DCHECK is never hit, turn it into a CHECK and remove
    // the test on `chrome::kSystemProfileDir` below.
    DCHECK((!(*it)->IsSystemProfile()))
        << "System profile shouldn't be saved as active profile";
    base::FilePath profile_path = (*it)->GetBaseName();
    // Some profiles might become ephemeral after they are created.
    // Don't persist the System Profile as one of the last actives, it should
    // never get a browser.
    if (!IsRegisteredAsEphemeral(&GetProfileAttributesStorage(),
                                 (*it)->GetPath()) &&
        profile_paths.find(profile_path) == profile_paths.end() &&
        profile_path != base::FilePath(chrome::kSystemProfileDir)) {
      profile_paths.insert(profile_path);
      profile_list.Append(profile_path.AsUTF8Unsafe());
    }
  }
}

#if !BUILDFLAG(IS_ANDROID)
void ProfileManager::OnBrowserOpened(Browser* browser) {
  DCHECK(browser);
  Profile* profile = browser->profile();
  DCHECK(profile);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // No browser should be opened before the FRE is finished.
  DCHECK(!ShouldOpenFirstRun(profile));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  if (!profile->IsOffTheRecord() &&
      !IsRegisteredAsEphemeral(&GetProfileAttributesStorage(),
                               profile->GetPath()) &&
      !browser->is_type_app() && ++browser_counts_[profile] == 1) {
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
  if (!profile->IsOffTheRecord() && !browser->is_type_app() &&
      --browser_counts_[profile] == 0) {
    active_profiles_.erase(base::ranges::find(active_profiles_, profile));
    if (!closing_all_browsers_)
      SaveActiveProfiles();
  }

  Profile* original_profile = profile->GetOriginalProfile();
  // Do nothing if the closed window is not the last window of the same profile.
  for (auto* browser_iter : *BrowserList::GetInstance()) {
    if (browser_iter->profile()->GetOriginalProfile() == original_profile)
      return;
  }

  if (profile->IsGuestSession()) {
    auto duration = base::Time::Now() - profile->GetCreationTime();
    base::UmaHistogramCustomCounts("Profile.Guest.OTR.Lifetime",
                                   duration.InMinutes(), 1,
                                   base::Days(28).InMinutes(), 100);

    CleanUpGuestProfile();
  }

  base::FilePath path = profile->GetPath();
  ProfileInfo* info = GetProfileInfoByPath(path);
  if (IsProfileDirectoryMarkedForDeletion(path)) {
    // Do nothing if the profile is already being deleted.
  } else if (IsRegisteredAsEphemeral(&GetProfileAttributesStorage(), path) &&
             info->keep_alives[ProfileKeepAliveOrigin::kProfileCreationFlow] ==
                 0) {
    // Delete if the profile is an ephemeral profile and it is not in the
    // profile creation flow.
    // TODO(crbug.com/1369535): Delete the profile when there is no keep alive.
    delete_profile_helper_->ScheduleEphemeralProfileForDeletion(path);
  } else if (!profile->IsOffTheRecord()) {
    auto* browsing_data_lifetime_manager =
        ChromeBrowsingDataLifetimeManagerFactory::GetForProfile(
            original_profile);
    if (browsing_data_lifetime_manager) {
      // Delete browsing data set by the ClearBrowsingDataOnExitList policy.
      browsing_data_lifetime_manager->ClearBrowsingDataForOnExitPolicy(
          /*keep_browser_alive=*/true);
    }
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
    base::FilePath profile_path_base = last_active->GetBaseName();
    if (profile_path_base != GetLastUsedProfileBaseName())
      profiles::SetLastUsedProfile(profile_path_base);

    ProfileAttributesEntry* entry =
        GetProfileAttributesStorage().GetProfileAttributesWithPath(
            last_active->GetPath());
    if (entry) {
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

void ProfileManager::BrowserListObserver::OnBrowserRemoved(Browser* browser) {
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
  if (IsRegisteredAsEphemeral(&profile_manager_->GetProfileAttributesStorage(),
                              last_active->GetPath()))
    return;

  profile_manager_->UpdateLastUser(last_active);
}

void ProfileManager::OnClosingAllBrowsersChanged(bool closing) {
  // Save active profiles when the browser begins shutting down, or if shutdown
  // is cancelled. The active profiles won't be changed during the shutdown
  // process as windows are closed.
  closing_all_browsers_ = closing;
  SaveActiveProfiles();
}
#endif  // !BUILDFLAG(IS_ANDROID)

ProfileManagerWithoutInit::ProfileManagerWithoutInit(
    const base::FilePath& user_data_dir)
    : ProfileManager(user_data_dir) {
  set_do_final_services_init(false);
}
