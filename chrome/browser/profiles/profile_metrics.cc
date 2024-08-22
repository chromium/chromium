// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_metrics.h"

#include <vector>

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "components/keyed_service/core/keyed_service_factory.h"
#include "components/keyed_service/core/refcounted_keyed_service_factory.h"
#include "components/profile_metrics/counts.h"

namespace {

#if !BUILDFLAG(IS_ANDROID)
constexpr base::TimeDelta kProfileActivityThreshold =
    base::Days(28);  // Should be integral number of weeks.
#endif

// Enum for getting net counts for adding and deleting users.
enum class ProfileNetUserCounts {
  ADD_NEW_USER = 0,  // Total count of add new user
  PROFILE_DELETED,   // User deleted a profile
  kMaxValue = PROFILE_DELETED
};

size_t GetTotalKeyedServiceCount(Profile* profile) {
  return KeyedServiceFactory::GetServicesCount(profile) +
         RefcountedKeyedServiceFactory::GetServicesCount(profile);
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

// static
bool ProfileMetrics::IsProfileActive(const ProfileAttributesEntry* entry) {
#if !BUILDFLAG(IS_ANDROID)
  // TODO(mlerman): iOS and Android should set an ActiveTime in the
  // ProfileAttributesStorage. (see ProfileManager::OnBrowserSetLastActive)
  if (base::Time::Now() - entry->GetActiveTime() > kProfileActivityThreshold)
    return false;
#endif
  return true;
}

// static
void ProfileMetrics::CountProfileInformation(ProfileAttributesStorage* storage,
                                             profile_metrics::Counts* counts) {
  size_t number_of_profiles = storage->GetNumberOfProfiles();
  counts->total = number_of_profiles;

  // Ignore other metrics if we have no profiles.
  if (!number_of_profiles)
    return;

  std::vector<ProfileAttributesEntry*> entries =
      storage->GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    if (!IsProfileActive(entry)) {
      counts->unused++;
    } else {
      counts->active++;
      if (entry->IsSupervised())
        counts->supervised++;
      if (entry->IsAuthenticated())
        counts->signedin++;
    }
  }
}

void ProfileMetrics::LogNumberOfProfiles(ProfileAttributesStorage* storage) {
  profile_metrics::Counts counts;
  CountProfileInformation(storage, &counts);
  profile_metrics::LogProfileMetricsCounts(counts);
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
      NOTREACHED_IN_MIGRATION();
  }
  base::UmaHistogramEnumeration("Profile.Avatar", icon_name,
                                NUM_PROFILE_AVATAR_METRICS);
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

void ProfileMetrics::LogSystemProfileKeyedServicesCount(Profile* profile) {
  DCHECK(profile->IsSystemProfile());

  std::string histogram_name = "Profile.KeyedService.Count.SystemProfile";
  histogram_name += profile->IsOffTheRecord() ? "OTR-M-107" : "Original-M-107";
  base::UmaHistogramCounts1000(histogram_name,
                               GetTotalKeyedServiceCount(profile));
}
