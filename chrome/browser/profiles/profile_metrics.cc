// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_metrics.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/profile_metrics/counts.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "content/public/browser/browser_thread.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser_finder.h"
#endif

namespace {

const int kMaximumDaysOfDisuse = 4 * 7;  // Should be integral number of weeks.

#if !defined(OS_ANDROID)
size_t number_of_profile_switches_ = 0;
#endif

// Enum for tracking the state of profiles being switched to.
enum ProfileOpenState {
  // Profile being switched to is already opened and has browsers opened.
  PROFILE_OPENED = 0,
  // Profile being switched to is already opened but has no browsers opened.
  PROFILE_OPENED_NO_BROWSER,
  // Profile being switched to is not opened.
  PROFILE_UNOPENED
};

#if !defined(OS_ANDROID)
ProfileOpenState GetProfileOpenState(
    ProfileManager* manager,
    const base::FilePath& path) {
  Profile* profile_switched_to = manager->GetProfileByPath(path);
  if (!profile_switched_to)
    return PROFILE_UNOPENED;

  if (chrome::GetBrowserCount(profile_switched_to) > 0)
    return PROFILE_OPENED;

  return PROFILE_OPENED_NO_BROWSER;
}
#endif

ProfileMetrics::ProfileType GetProfileType(
    const base::FilePath& profile_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ProfileMetrics::ProfileType metric = ProfileMetrics::SECONDARY;
  ProfileManager* manager = g_browser_process->profile_manager();
  base::FilePath user_data_dir;
  // In unittests, we do not always have a profile_manager so check.
  if (manager) {
    user_data_dir = manager->user_data_dir();
  }
  if (profile_path == user_data_dir.AppendASCII(chrome::kInitialProfile)) {
    metric = ProfileMetrics::ORIGINAL;
  }
  return metric;
}

void LogLockedProfileInformation(ProfileManager* manager) {
  base::Time now = base::Time::Now();
  const int kMinutesInProfileValidDuration =
      base::TimeDelta::FromDays(28).InMinutes();
  std::vector<ProfileAttributesEntry*> entries =
      manager->GetProfileAttributesStorage().GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    // Find when locked profiles were locked
    if (entry->IsSigninRequired()) {
      base::TimeDelta time_since_lock = now - entry->GetActiveTime();
      // Specifying 100 buckets for the histogram to get a higher level of
      // granularity in the reported data, given the large number of possible
      // values (kMinutesInProfileValidDuration > 40,000).
      UMA_HISTOGRAM_CUSTOM_COUNTS("Profile.LockedProfilesDuration",
                                  time_since_lock.InMinutes(),
                                  1,
                                  kMinutesInProfileValidDuration,
                                  100);
    }
  }
}

bool HasProfileBeenActiveSince(const ProfileAttributesEntry* entry,
                               const base::Time& active_limit) {
#if !defined(OS_ANDROID)
  // TODO(mlerman): iOS and Android should set an ActiveTime in the
  // ProfileAttributesStorage. (see ProfileManager::OnBrowserSetLastActive)
  if (entry->GetActiveTime() < active_limit)
    return false;
#endif
  return true;
}

}  // namespace

// This enum is used for histograms. Do not change existing values. Append new
// values at the end.
enum ProfileAvatar {
  AVATAR_GENERIC = 0,  // The names for avatar icons
  AVATAR_GENERIC_AQUA = 1,
  AVATAR_GENERIC_BLUE = 2,
  AVATAR_GENERIC_GREEN = 3,
  AVATAR_GENERIC_ORANGE = 4,
  AVATAR_GENERIC_PURPLE = 5,
  AVATAR_GENERIC_RED = 6,
  AVATAR_GENERIC_YELLOW = 7,
  AVATAR_SECRET_AGENT = 8,
  AVATAR_SUPERHERO = 9,
  AVATAR_VOLLEYBALL = 10,
  AVATAR_BUSINESSMAN = 11,
  AVATAR_NINJA = 12,
  AVATAR_ALIEN = 13,
  AVATAR_AWESOME = 14,
  AVATAR_FLOWER = 15,
  AVATAR_PIZZA = 16,
  AVATAR_SOCCER = 17,
  AVATAR_BURGER = 18,
  AVATAR_CAT = 19,
  AVATAR_CUPCAKE = 20,
  AVATAR_DOG = 21,
  AVATAR_HORSE = 22,
  AVATAR_MARGARITA = 23,
  AVATAR_NOTE = 24,
  AVATAR_SUN_CLOUD = 25,
  AVATAR_PLACEHOLDER = 26,
  AVATAR_UNKNOWN = 27,
  AVATAR_GAIA = 28,
  // Modern avatars:
  AVATAR_ORIGAMI_CAT = 29,
  AVATAR_ORIGAMI_CORGI = 30,
  AVATAR_ORIGAMI_DRAGON = 31,
  AVATAR_ORIGAMI_ELEPHANT = 32,
  AVATAR_ORIGAMI_FOX = 33,
  AVATAR_ORIGAMI_MONKEY = 34,
  AVATAR_ORIGAMI_PANDA = 35,
  AVATAR_ORIGAMI_PENGUIN = 36,
  AVATAR_ORIGAMI_PINKBUTTERFLY = 37,
  AVATAR_ORIGAMI_RABBIT = 38,
  AVATAR_ORIGAMI_UNICORN = 39,
  AVATAR_ILLUSTRATION_BASKETBALL = 40,
  AVATAR_ILLUSTRATION_BIKE = 41,
  AVATAR_ILLUSTRATION_BIRD = 42,
  AVATAR_ILLUSTRATION_CHEESE = 43,
  AVATAR_ILLUSTRATION_FOOTBALL = 44,
  AVATAR_ILLUSTRATION_RAMEN = 45,
  AVATAR_ILLUSTRATION_SUNGLASSES = 46,
  AVATAR_ILLUSTRATION_SUSHI = 47,
  AVATAR_ILLUSTRATION_TAMAGOTCHI = 48,
  AVATAR_ILLUSTRATION_VINYL = 49,
  AVATAR_ABSTRACT_AVOCADO = 50,
  AVATAR_ABSTRACT_CAPPUCCINO = 51,
  AVATAR_ABSTRACT_ICECREAM = 52,
  AVATAR_ABSTRACT_ICEWATER = 53,
  AVATAR_ABSTRACT_MELON = 54,
  AVATAR_ABSTRACT_ONIGIRI = 55,
  AVATAR_ABSTRACT_PIZZA = 56,
  AVATAR_ABSTRACT_SANDWICH = 57,
  NUM_PROFILE_AVATAR_METRICS
};

bool ProfileMetrics::CountProfileInformation(ProfileManager* manager,
                                             profile_metrics::Counts* counts) {
  ProfileAttributesStorage& storage = manager->GetProfileAttributesStorage();
  size_t number_of_profiles = storage.GetNumberOfProfiles();
  counts->total = number_of_profiles;

  // Ignore other metrics if we have no profiles.
  if (!number_of_profiles)
    return false;

  // Maximum age for "active" profile is 4 weeks.
  base::Time oldest = base::Time::Now() -
      base::TimeDelta::FromDays(kMaximumDaysOfDisuse);

  std::vector<ProfileAttributesEntry*> entries =
      storage.GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    if (!HasProfileBeenActiveSince(entry, oldest)) {
      counts->unused++;
    } else {
      counts->active++;
      if (!entry->IsUsingDefaultName())
        counts->named++;
      if (entry->IsSupervised())
        counts->supervised++;
      if (entry->IsAuthenticated()) {
        counts->signedin++;
        if (entry->IsUsingGAIAPicture())
          counts->gaia_icon++;
        if (entry->IsAuthError())
          counts->auth_errors++;
      }
    }
  }
  return true;
}

profile_metrics::BrowserProfileType ProfileMetrics::GetBrowserProfileType(
    Profile* profile) {
  if (profile->IsSystemProfile())
    return profile_metrics::BrowserProfileType::kSystem;
  if (profile->IsGuestSession())
    return profile_metrics::BrowserProfileType::kGuest;
  // A regular profile can be in a guest session or a system profile. Hence it
  // should be checked after them.
  if (profile->IsRegularProfile())
    return profile_metrics::BrowserProfileType::kRegular;
  if (profile->IsIncognitoProfile()) {
    return profile->IsIndependentOffTheRecordProfile()
               ? profile_metrics::BrowserProfileType::
                     kIndependentIncognitoProfile
               : profile_metrics::BrowserProfileType::kIncognito;
  }

  NOTREACHED();
  return profile_metrics::BrowserProfileType::kMaxValue;
}

#if !defined(OS_ANDROID)
void ProfileMetrics::LogNumberOfProfileSwitches() {
  UMA_HISTOGRAM_COUNTS_100("Profile.NumberOfSwitches",
                           number_of_profile_switches_);
}
#endif

void ProfileMetrics::LogNumberOfProfiles(ProfileManager* manager) {
  profile_metrics::Counts counts;
  bool success = CountProfileInformation(manager, &counts);

  profile_metrics::LogProfileMetricsCounts(counts);

  // Ignore other metrics if we have no profiles.
  if (success)
    LogLockedProfileInformation(manager);
}

void ProfileMetrics::LogProfileAddNewUser(ProfileAdd metric) {
  DCHECK(metric < NUM_PROFILE_ADD_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Profile.AddNewUser", metric,
                            NUM_PROFILE_ADD_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Profile.NetUserCount", ADD_NEW_USER,
                            NUM_PROFILE_NET_METRICS);
}

void ProfileMetrics::LogProfileAvatarSelection(size_t icon_index) {
  ProfileAvatar icon_name = AVATAR_UNKNOWN;
  switch (icon_index) {
    case 0:
      icon_name = AVATAR_GENERIC;
      break;
    case 1:
      icon_name = AVATAR_GENERIC_AQUA;
      break;
    case 2:
      icon_name = AVATAR_GENERIC_BLUE;
      break;
    case 3:
      icon_name = AVATAR_GENERIC_GREEN;
      break;
    case 4:
      icon_name = AVATAR_GENERIC_ORANGE;
      break;
    case 5:
      icon_name = AVATAR_GENERIC_PURPLE;
      break;
    case 6:
      icon_name = AVATAR_GENERIC_RED;
      break;
    case 7:
      icon_name = AVATAR_GENERIC_YELLOW;
      break;
    case 8:
      icon_name = AVATAR_SECRET_AGENT;
      break;
    case 9:
      icon_name = AVATAR_SUPERHERO;
      break;
    case 10:
      icon_name = AVATAR_VOLLEYBALL;
      break;
    case 11:
      icon_name = AVATAR_BUSINESSMAN;
      break;
    case 12:
      icon_name = AVATAR_NINJA;
      break;
    case 13:
      icon_name = AVATAR_ALIEN;
      break;
    case 14:
      icon_name = AVATAR_AWESOME;
      break;
    case 15:
      icon_name = AVATAR_FLOWER;
      break;
    case 16:
      icon_name = AVATAR_PIZZA;
      break;
    case 17:
      icon_name = AVATAR_SOCCER;
      break;
    case 18:
      icon_name = AVATAR_BURGER;
      break;
    case 19:
      icon_name = AVATAR_CAT;
      break;
    case 20:
      icon_name = AVATAR_CUPCAKE;
      break;
    case 21:
      icon_name = AVATAR_DOG;
      break;
    case 22:
      icon_name = AVATAR_HORSE;
      break;
    case 23:
      icon_name = AVATAR_MARGARITA;
      break;
    case 24:
      icon_name = AVATAR_NOTE;
      break;
    case 25:
      icon_name = AVATAR_SUN_CLOUD;
      break;
    case 26:
      icon_name = AVATAR_PLACEHOLDER;
      break;
    // Modern avatars:
    case 27:
      icon_name = AVATAR_ORIGAMI_CAT;
      break;
    case 28:
      icon_name = AVATAR_ORIGAMI_CORGI;
      break;
    case 29:
      icon_name = AVATAR_ORIGAMI_DRAGON;
      break;
    case 30:
      icon_name = AVATAR_ORIGAMI_ELEPHANT;
      break;
    case 31:
      icon_name = AVATAR_ORIGAMI_FOX;
      break;
    case 32:
      icon_name = AVATAR_ORIGAMI_MONKEY;
      break;
    case 33:
      icon_name = AVATAR_ORIGAMI_PANDA;
      break;
    case 34:
      icon_name = AVATAR_ORIGAMI_PENGUIN;
      break;
    case 35:
      icon_name = AVATAR_ORIGAMI_PINKBUTTERFLY;
      break;
    case 36:
      icon_name = AVATAR_ORIGAMI_RABBIT;
      break;
    case 37:
      icon_name = AVATAR_ORIGAMI_UNICORN;
      break;
    case 38:
      icon_name = AVATAR_ILLUSTRATION_BASKETBALL;
      break;
    case 39:
      icon_name = AVATAR_ILLUSTRATION_BIKE;
      break;
    case 40:
      icon_name = AVATAR_ILLUSTRATION_BIRD;
      break;
    case 41:
      icon_name = AVATAR_ILLUSTRATION_CHEESE;
      break;
    case 42:
      icon_name = AVATAR_ILLUSTRATION_FOOTBALL;
      break;
    case 43:
      icon_name = AVATAR_ILLUSTRATION_RAMEN;
      break;
    case 44:
      icon_name = AVATAR_ILLUSTRATION_SUNGLASSES;
      break;
    case 45:
      icon_name = AVATAR_ILLUSTRATION_SUSHI;
      break;
    case 46:
      icon_name = AVATAR_ILLUSTRATION_TAMAGOTCHI;
      break;
    case 47:
      icon_name = AVATAR_ILLUSTRATION_VINYL;
      break;
    case 48:
      icon_name = AVATAR_ABSTRACT_AVOCADO;
      break;
    case 49:
      icon_name = AVATAR_ABSTRACT_CAPPUCCINO;
      break;
    case 50:
      icon_name = AVATAR_ABSTRACT_ICECREAM;
      break;
    case 51:
      icon_name = AVATAR_ABSTRACT_ICEWATER;
      break;
    case 52:
      icon_name = AVATAR_ABSTRACT_MELON;
      break;
    case 53:
      icon_name = AVATAR_ABSTRACT_ONIGIRI;
      break;
    case 54:
      icon_name = AVATAR_ABSTRACT_PIZZA;
      break;
    case 55:
      icon_name = AVATAR_ABSTRACT_SANDWICH;
      break;
    case SIZE_MAX:
      icon_name = AVATAR_GAIA;
      break;
    default:
      NOTREACHED();
  }
  UMA_HISTOGRAM_ENUMERATION("Profile.Avatar", icon_name,
                            NUM_PROFILE_AVATAR_METRICS);
}

void ProfileMetrics::LogProfileDeleteUser(ProfileDelete metric) {
  DCHECK(metric < NUM_DELETE_PROFILE_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Profile.DeleteProfileAction", metric,
                            NUM_DELETE_PROFILE_METRICS);
  if (metric != DELETE_PROFILE_USER_MANAGER_SHOW_WARNING &&
      metric != DELETE_PROFILE_SETTINGS_SHOW_WARNING &&
      metric != DELETE_PROFILE_ABORTED) {
    // If a user was actually deleted, update the net user count.
    UMA_HISTOGRAM_ENUMERATION("Profile.NetUserCount", PROFILE_DELETED,
                              NUM_PROFILE_NET_METRICS);
  }
}

void ProfileMetrics::LogProfileOpenMethod(ProfileOpen metric) {
  DCHECK(metric < NUM_PROFILE_OPEN_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Profile.OpenMethod", metric,
                            NUM_PROFILE_OPEN_METRICS);
}

#if !defined(OS_ANDROID)
void ProfileMetrics::LogProfileSwitch(
    ProfileOpen metric,
    ProfileManager* manager,
    const base::FilePath& profile_path) {
  DCHECK(metric < NUM_PROFILE_OPEN_METRICS);
  ProfileOpenState open_state = GetProfileOpenState(manager, profile_path);
  switch (open_state) {
    case PROFILE_OPENED:
      UMA_HISTOGRAM_ENUMERATION(
        "Profile.OpenMethod.ToOpenedProfile",
        metric,
        NUM_PROFILE_OPEN_METRICS);
      break;
    case PROFILE_OPENED_NO_BROWSER:
      UMA_HISTOGRAM_ENUMERATION(
        "Profile.OpenMethod.ToOpenedProfileWithoutBrowser",
        metric,
        NUM_PROFILE_OPEN_METRICS);
      break;
    case PROFILE_UNOPENED:
      UMA_HISTOGRAM_ENUMERATION(
        "Profile.OpenMethod.ToUnopenedProfile",
        metric,
        NUM_PROFILE_OPEN_METRICS);
      break;
    default:
      // There are no other possible values.
      NOTREACHED();
      break;
  }

  ++number_of_profile_switches_;
  // The LogOpenMethod histogram aggregates data from profile switches as well
  // as opening of profile related UI elements.
  LogProfileOpenMethod(metric);
}
#endif

void ProfileMetrics::LogProfileSwitchGaia(ProfileGaia metric) {
  if (metric == GAIA_OPT_IN)
    LogProfileAvatarSelection(SIZE_MAX);
  UMA_HISTOGRAM_ENUMERATION("Profile.SwitchGaiaPhotoSettings",
                            metric,
                            NUM_PROFILE_GAIA_METRICS);
}

void ProfileMetrics::LogProfileSyncInfo(ProfileSync metric) {
  DCHECK(metric < NUM_PROFILE_SYNC_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Profile.SyncCustomize", metric,
                            NUM_PROFILE_SYNC_METRICS);
}

void ProfileMetrics::LogProfileAuthResult(ProfileAuth metric) {
  UMA_HISTOGRAM_ENUMERATION("Profile.AuthResult", metric,
                            NUM_PROFILE_AUTH_METRICS);
}

void ProfileMetrics::LogProfileDesktopMenu(ProfileDesktopMenu metric) {
  UMA_HISTOGRAM_ENUMERATION("Profile.DesktopMenu.NonGAIA", metric,
                            NUM_PROFILE_DESKTOP_MENU_METRICS);
}

void ProfileMetrics::LogProfileDelete(bool profile_was_signed_in) {
  UMA_HISTOGRAM_BOOLEAN("Profile.Delete", profile_was_signed_in);
}

void ProfileMetrics::LogTimeToOpenUserManager(
    const base::TimeDelta& time_to_open) {
  UMA_HISTOGRAM_CUSTOM_TIMES("Profile.TimeToOpenUserManagerUpTo1min",
                             time_to_open, base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(1), 50);
}

#if defined(OS_ANDROID)
void ProfileMetrics::LogProfileAndroidAccountManagementMenu(
    ProfileAndroidAccountManagementMenu metric,
    signin::GAIAServiceType gaia_service) {
  // The first parameter to the histogram needs to be literal, because of the
  // optimized implementation of |UMA_HISTOGRAM_ENUMERATION|. Do not attempt
  // to refactor.
  switch (gaia_service) {
    case signin::GAIA_SERVICE_TYPE_NONE:
      UMA_HISTOGRAM_ENUMERATION(
          "Profile.AndroidAccountManagementMenu.NonGAIA",
          metric,
          NUM_PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_METRICS);
      break;
    case signin::GAIA_SERVICE_TYPE_SIGNOUT:
      UMA_HISTOGRAM_ENUMERATION(
          "Profile.AndroidAccountManagementMenu.GAIASignout",
          metric,
          NUM_PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_METRICS);
      break;
    case signin::GAIA_SERVICE_TYPE_INCOGNITO:
      UMA_HISTOGRAM_ENUMERATION(
          "Profile.AndroidAccountManagementMenu.GAIASignoutIncognito",
          metric,
          NUM_PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_METRICS);
      break;
    case signin::GAIA_SERVICE_TYPE_ADDSESSION:
      UMA_HISTOGRAM_ENUMERATION(
          "Profile.AndroidAccountManagementMenu.GAIAAddSession",
          metric,
          NUM_PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_METRICS);
      break;
    case signin::GAIA_SERVICE_TYPE_SIGNUP:
      UMA_HISTOGRAM_ENUMERATION(
          "Profile.AndroidAccountManagementMenu.GAIASignup",
          metric,
          NUM_PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_METRICS);
      break;
    case signin::GAIA_SERVICE_TYPE_DEFAULT:
      UMA_HISTOGRAM_ENUMERATION(
          "Profile.AndroidAccountManagementMenu.GAIADefault",
          metric,
          NUM_PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_METRICS);
      break;
  }
}
#endif  // defined(OS_ANDROID)

void ProfileMetrics::LogProfileLaunch(Profile* profile) {
  base::FilePath profile_path = profile->GetPath();
  UMA_HISTOGRAM_ENUMERATION("Profile.LaunchBrowser",
                            GetProfileType(profile_path),
                            NUM_PROFILE_TYPE_METRICS);

  if (profile->IsSupervised()) {
    base::RecordAction(
        base::UserMetricsAction("ManagedMode_NewManagedUserWindow"));
  }
}

void ProfileMetrics::LogProfileUpdate(const base::FilePath& profile_path) {
  UMA_HISTOGRAM_ENUMERATION("Profile.Update",
                            GetProfileType(profile_path),
                            NUM_PROFILE_TYPE_METRICS);
}
