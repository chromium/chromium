// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_METRICS_H_
#define CHROME_BROWSER_PROFILES_PROFILE_METRICS_H_

#include <stddef.h>

#include "build/build_config.h"

class Profile;
class ProfileAttributesEntry;
class ProfileAttributesStorage;

namespace profile_metrics {
struct Counts;
}

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

  // Returns whether profile |entry| is considered active for metrics.
  static bool IsProfileActive(const ProfileAttributesEntry* entry);

  // Count and return summary information about the profiles currently in the
  // |storage|. This information is returned in the output variable |counts|.
  static void CountProfileInformation(ProfileAttributesStorage* storage,
                                      profile_metrics::Counts* counts);

  static void LogNumberOfProfiles(ProfileAttributesStorage* storage);
  static void LogProfileAddNewUser(ProfileAdd metric);
  static void LogProfileAddSignInFlowOutcome(
      ProfileSignedInFlowOutcome outcome);
  static void LogProfileAvatarSelection(size_t icon_index);
  static void LogProfileDeleteUser(ProfileDelete metric);
  static void LogProfileLaunch(Profile* profile);

  // Records the count of KeyedService active for the System Profile histogram.
  // Expects only System Profiles.
  static void LogSystemProfileKeyedServicesCount(Profile* profile);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_METRICS_H_
