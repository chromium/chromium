// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_metrics.h"

#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/common/chrome_constants.h"
#include "components/profile_metrics/counts.h"

namespace {

// Enum for getting net counts for adding and deleting users.
enum class ProfileNetUserCounts {
  ADD_NEW_USER = 0,  // Total count of add new user
  PROFILE_DELETED,   // User deleted a profile
  kMaxValue = PROFILE_DELETED
};


// Count and return summary information about the profiles currently in the
// `storage`.
profile_metrics::Counts CountProfileInformation(
    ProfileAttributesStorage* storage,
    profile_metrics::ProfileActivityThreshold activity_threshold) {
  profile_metrics::Counts counts;

  size_t number_of_profiles = storage->GetNumberOfProfiles();
  counts.total = number_of_profiles;
  // Ignore other metrics if we have no profiles.
  if (!number_of_profiles) {
    return counts;
  }

  std::vector<ProfileAttributesEntry*> entries =
      storage->GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    if (!ProfileMetrics::IsProfileActive(entry, activity_threshold)) {
      counts.unused++;
    } else {
      counts.active++;
      if (entry->IsSupervised()) {
        counts.supervised++;
      }
      if (entry->IsAuthenticated()) {
        counts.signedin++;
      }
    }
  }

  return counts;
}

#if !BUILDFLAG(IS_ANDROID)
base::TimeDelta GetActivityThresholdDelta(
    profile_metrics::ProfileActivityThreshold activity_threshold) {
  switch (activity_threshold) {
    case profile_metrics::ProfileActivityThreshold::kDuration1Day:
      return base::Days(1);
    case profile_metrics::ProfileActivityThreshold::kDuration7Days:
      return base::Days(7);
    case profile_metrics::ProfileActivityThreshold::kDuration28Days:
      return base::Days(28);
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

void LogProfileAvatar(size_t icon_index, std::string_view histogram_name) {
  ProfileMetrics::ProfileAvatar icon_name = ProfileMetrics::AVATAR_UNKNOWN;
  switch (icon_index) {
    case 0:
      icon_name = ProfileMetrics::AVATAR_GENERIC;
      break;
    case 1:
      icon_name = ProfileMetrics::AVATAR_GENERIC_AQUA;
      break;
    case 2:
      icon_name = ProfileMetrics::AVATAR_GENERIC_BLUE;
      break;
    case 3:
      icon_name = ProfileMetrics::AVATAR_GENERIC_GREEN;
      break;
    case 4:
      icon_name = ProfileMetrics::AVATAR_GENERIC_ORANGE;
      break;
    case 5:
      icon_name = ProfileMetrics::AVATAR_GENERIC_PURPLE;
      break;
    case 6:
      icon_name = ProfileMetrics::AVATAR_GENERIC_RED;
      break;
    case 7:
      icon_name = ProfileMetrics::AVATAR_GENERIC_YELLOW;
      break;
    case 8:
      icon_name = ProfileMetrics::AVATAR_SECRET_AGENT;
      break;
    case 9:
      icon_name = ProfileMetrics::AVATAR_SUPERHERO;
      break;
    case 10:
      icon_name = ProfileMetrics::AVATAR_VOLLEYBALL;
      break;
    case 11:
      icon_name = ProfileMetrics::AVATAR_BUSINESSMAN;
      break;
    case 12:
      icon_name = ProfileMetrics::AVATAR_NINJA;
      break;
    case 13:
      icon_name = ProfileMetrics::AVATAR_ALIEN;
      break;
    case 14:
      icon_name = ProfileMetrics::AVATAR_AWESOME;
      break;
    case 15:
      icon_name = ProfileMetrics::AVATAR_FLOWER;
      break;
    case 16:
      icon_name = ProfileMetrics::AVATAR_PIZZA;
      break;
    case 17:
      icon_name = ProfileMetrics::AVATAR_SOCCER;
      break;
    case 18:
      icon_name = ProfileMetrics::AVATAR_BURGER;
      break;
    case 19:
      icon_name = ProfileMetrics::AVATAR_CAT;
      break;
    case 20:
      icon_name = ProfileMetrics::AVATAR_CUPCAKE;
      break;
    case 21:
      icon_name = ProfileMetrics::AVATAR_DOG;
      break;
    case 22:
      icon_name = ProfileMetrics::AVATAR_HORSE;
      break;
    case 23:
      icon_name = ProfileMetrics::AVATAR_MARGARITA;
      break;
    case 24:
      icon_name = ProfileMetrics::AVATAR_NOTE;
      break;
    case 25:
      icon_name = ProfileMetrics::AVATAR_SUN_CLOUD;
      break;
    case 26:
      icon_name = ProfileMetrics::AVATAR_PLACEHOLDER;
      break;
    // Modern avatars:
    case 27:
      icon_name = ProfileMetrics::AVATAR_ORIGAMI_CAT;
      break;
    case 28:
      icon_name = ProfileMetrics::AVATAR_ORIGAMI_CORGI;
      break;
    case 29:
      icon_name = ProfileMetrics::AVATAR_ORIGAMI_DRAGON;
      break;
    case 30:
      icon_name = ProfileMetrics::AVATAR_ORIGAMI_ELEPHANT;
      break;
    case 31:
      icon_name = ProfileMetrics::AVATAR_ORIGAMI_FOX;
      break;
    case 32:
      icon_name = ProfileMetrics::AVATAR_ORIGAMI_MONKEY;
      break;
    case 33:
      icon_name = ProfileMetrics::AVATAR_ORIGAMI_PANDA;
      break;
    case 34:
      icon_name = ProfileMetrics::AVATAR_ORIGAMI_PENGUIN;
      break;
    case 35:
      icon_name = ProfileMetrics::AVATAR_ORIGAMI_PINKBUTTERFLY;
      break;
    case 36:
      icon_name = ProfileMetrics::AVATAR_ORIGAMI_RABBIT;
      break;
    case 37:
      icon_name = ProfileMetrics::AVATAR_ORIGAMI_UNICORN;
      break;
    case 38:
      icon_name = ProfileMetrics::AVATAR_ILLUSTRATION_BASKETBALL;
      break;
    case 39:
      icon_name = ProfileMetrics::AVATAR_ILLUSTRATION_BIKE;
      break;
    case 40:
      icon_name = ProfileMetrics::AVATAR_ILLUSTRATION_BIRD;
      break;
    case 41:
      icon_name = ProfileMetrics::AVATAR_ILLUSTRATION_CHEESE;
      break;
    case 42:
      icon_name = ProfileMetrics::AVATAR_ILLUSTRATION_FOOTBALL;
      break;
    case 43:
      icon_name = ProfileMetrics::AVATAR_ILLUSTRATION_RAMEN;
      break;
    case 44:
      icon_name = ProfileMetrics::AVATAR_ILLUSTRATION_SUNGLASSES;
      break;
    case 45:
      icon_name = ProfileMetrics::AVATAR_ILLUSTRATION_SUSHI;
      break;
    case 46:
      icon_name = ProfileMetrics::AVATAR_ILLUSTRATION_TAMAGOTCHI;
      break;
    case 47:
      icon_name = ProfileMetrics::AVATAR_ILLUSTRATION_VINYL;
      break;
    case 48:
      icon_name = ProfileMetrics::AVATAR_ABSTRACT_AVOCADO;
      break;
    case 49:
      icon_name = ProfileMetrics::AVATAR_ABSTRACT_CAPPUCCINO;
      break;
    case 50:
      icon_name = ProfileMetrics::AVATAR_ABSTRACT_ICECREAM;
      break;
    case 51:
      icon_name = ProfileMetrics::AVATAR_ABSTRACT_ICEWATER;
      break;
    case 52:
      icon_name = ProfileMetrics::AVATAR_ABSTRACT_MELON;
      break;
    case 53:
      icon_name = ProfileMetrics::AVATAR_ABSTRACT_ONIGIRI;
      break;
    case 54:
      icon_name = ProfileMetrics::AVATAR_ABSTRACT_PIZZA;
      break;
    case 55:
      icon_name = ProfileMetrics::AVATAR_ABSTRACT_SANDWICH;
      break;
    case SIZE_MAX:
      icon_name = ProfileMetrics::AVATAR_GAIA;
      break;
    default:
      NOTREACHED();
  }
  base::UmaHistogramEnumeration(histogram_name, icon_name,
                                ProfileMetrics::NUM_PROFILE_AVATAR_METRICS);
}

}  // namespace

// static
bool ProfileMetrics::IsProfileActive(
    const ProfileAttributesEntry* entry,
    profile_metrics::ProfileActivityThreshold activity_threshold) {
#if !BUILDFLAG(IS_ANDROID)
  // TODO(mlerman): iOS and Android should set an ActiveTime in the
  // ProfileAttributesStorage. (see ProfileManager::OnBrowserSetLastActive)
  if (base::Time::Now() - entry->GetActiveTime() >
      GetActivityThresholdDelta(activity_threshold)) {
    return false;
  }
#endif
  return true;
}

void ProfileMetrics::LogNumberOfProfiles(ProfileAttributesStorage* storage) {
  CHECK(storage);
  profile_metrics::LogTotalNumberOfProfiles(storage->GetNumberOfProfiles());

  for (profile_metrics::ProfileActivityThreshold activity_threshold :
       {profile_metrics::ProfileActivityThreshold::kDuration1Day,
        profile_metrics::ProfileActivityThreshold::kDuration7Days,
        profile_metrics::ProfileActivityThreshold::kDuration28Days}) {
    profile_metrics::Counts counts =
        CountProfileInformation(storage, activity_threshold);
    profile_metrics::LogProfileMetricsCounts(counts, activity_threshold);
  }
}

void ProfileMetrics::LogProfileAddNewUser(ProfileAdd metric) {
  base::UmaHistogramEnumeration("Profile.AddNewUser", metric);
  base::UmaHistogramEnumeration("Profile.NetUserCount",
                                ProfileNetUserCounts::ADD_NEW_USER);
}

// static
void ProfileMetrics::LogProfileAddSignInFlowOutcome(
    ProfileSignedInFlowOutcome outcome) {
  base::UmaHistogramEnumeration("Profile.AddSignInFlowOutcome", outcome);
}

void ProfileMetrics::LogProfileAvatarOnLoad(size_t icon_index) {
  LogProfileAvatar(icon_index, "Profile.AvatarOnLoad");
}

void ProfileMetrics::LogProfileAvatarSelection(size_t icon_index) {
  LogProfileAvatar(icon_index, "Profile.Avatar");
}

void ProfileMetrics::LogProfileDeleteUser(ProfileDelete metric) {
  DCHECK(metric < NUM_DELETE_PROFILE_METRICS);
  base::UmaHistogramEnumeration("Profile.DeleteProfileAction", metric,
                                NUM_DELETE_PROFILE_METRICS);
  if (metric != DELETE_PROFILE_USER_MANAGER_SHOW_WARNING &&
      metric != DELETE_PROFILE_SETTINGS_SHOW_WARNING &&
      metric != DELETE_PROFILE_ABORTED) {
    // If a user was actually deleted, update the net user count.
    base::UmaHistogramEnumeration("Profile.NetUserCount",
                                  ProfileNetUserCounts::PROFILE_DELETED);
  }
}

void ProfileMetrics::LogProfileLaunch(Profile* profile) {
  if (profile->IsChild()) {
    base::RecordAction(
        base::UserMetricsAction("ManagedMode_NewManagedUserWindow"));
  }
}
