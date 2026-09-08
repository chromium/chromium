// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_METRICS_H_
#define CHROME_BROWSER_PROFILES_PROFILE_METRICS_H_

#include <stddef.h>

#include "build/build_config.h"
#include "components/profile_metrics/counts.h"

class Profile;
class ProfileAttributesEntry;
class ProfileAttributesStorage;

#if BUILDFLAG(IS_ANDROID)
namespace signin {
enum GAIAServiceType : int;
}
#endif  // BUILDFLAG(IS_ANDROID)

class ProfileMetrics {
 public:
  // Enum for counting the ways users were added.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum ProfileAdd {
    // User adds new user from icon menu -- no longer used
    // ADD_NEW_USER_ICON = 0,
    // User adds new user from menu bar -- no longer used
    // ADD_NEW_USER_MENU = 1,
    // User adds new profile from the (old) create-profile dialog
    ADD_NEW_USER_DIALOG = 2,
    // User adds new local profile from Profile Picker
    ADD_NEW_PROFILE_PICKER_LOCAL = 3,
    // Auto-created after deleting last user
    ADD_NEW_USER_LAST_DELETED = 4,
    // Created by the sign-in interception prompt
    ADD_NEW_USER_SIGNIN_INTERCEPTION = 5,
    // Created during the sync flow (to avoid clash with data in the existing
    // profile)
    ADD_NEW_USER_SYNC_FLOW = 6,
    // User adds new signed-in profile from Profile Picker
    ADD_NEW_PROFILE_PICKER_SIGNED_IN = 7,
    kMaxValue = ADD_NEW_PROFILE_PICKER_SIGNED_IN
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ProfileSignedInFlowOutcome {
    kConsumerSync = 0,
    kConsumerSigninOnly = 1,
    kConsumerSyncSettings = 2,
    kEnterpriseSync = 3,
    kEnterpriseSigninOnly = 4,
    // DEPRECATED: kEnterpriseSigninOnlyNotLinked = 5,
    kEnterpriseSyncSettings = 6,
    kEnterpriseSyncDisabled = 7,
    // Includes the case that the account is already syncing in another profile.
    kLoginError = 8,
    kSAML = 9,
    kAbortedBeforeSignIn = 10,
    kAbortedAfterSignIn = 11,
    kAbortedOnEnterpriseWelcome = 12,
    kSkippedAlreadySyncing = 13,
    kSkippedByPolicies = 14,
    kForceSigninSyncNotGranted = 15,
    kMaxValue = kForceSigninSyncNotGranted,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum ProfileDelete {
    // Delete profile from settings page.
    DELETE_PROFILE_SETTINGS = 0,
    // Delete profile from User Manager.
    DELETE_PROFILE_USER_MANAGER = 1,
    // Show the delete profile warning in the User Manager.
    DELETE_PROFILE_USER_MANAGER_SHOW_WARNING = 2,
    // Show the delete profile warning in the Settings page.
    DELETE_PROFILE_SETTINGS_SHOW_WARNING = 3,
    // Aborts profile deletion in an OnBeforeUnload event in any browser tab.
    DELETE_PROFILE_ABORTED = 4,
    // DELETE_PROFILE_DICE_WEB_SIGNOUT = 5,  // No longer used.
    // Delete profile internally when Chrome signout is prohibited and the
    // username is no longer allowed.
    DELETE_PROFILE_PRIMARY_ACCOUNT_NOT_ALLOWED = 6,
    // DELETE_PROFILE_PRIMARY_ACCOUNT_REMOVED_LACROS = 7,  // No longer used.
    // DELETE_PROFILE_SIGNIN_REQUIRED_MIRROR_LACROS = 8,   // No longer used.
    NUM_DELETE_PROFILE_METRICS
  };

  enum ProfileAuth {
    AUTH_UNNECESSARY,     // Profile was not locked
    AUTH_LOCAL,           // Profile was authenticated locally
    AUTH_ONLINE,          // Profile was authenticated on-line
    AUTH_FAILED,          // Profile failed authentication
    AUTH_FAILED_OFFLINE,  // Profile failed authentication and was offline
    NUM_PROFILE_AUTH_METRICS
  };

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

  // Returns whether profile `entry` is considered active for metrics. "Active"
  // is dependent on the `activity_threshold` duration, defaulted to 28 days.
  static bool IsProfileActive(
      const ProfileAttributesEntry* entry,
      profile_metrics::ProfileActivityThreshold activity_threshold =
          profile_metrics::ProfileActivityThreshold::kDuration28Days);

  static void LogNumberOfProfiles(ProfileAttributesStorage* storage);
  static void LogProfileAddNewUser(ProfileAdd metric);
  static void LogProfileAddSignInFlowOutcome(
      ProfileSignedInFlowOutcome outcome);
  static void LogProfileAvatarOnLoad(size_t icon_index);
  static void LogProfileAvatarSelection(size_t icon_index);
  static void LogProfileDeleteUser(ProfileDelete metric);
  static void LogProfileLaunch(Profile* profile);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_METRICS_H_
