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

namespace base {
class FilePath;
}

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
    kMaxValue = kSkippedByPolicies,
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
    // Delete profile internally when a profile cannot exist without a primary
    // account and this account gets removed.
    DELETE_PROFILE_PRIMARY_ACCOUNT_REMOVED_LACROS = 7,
    // Delete profile internally at startup, if a Lacros profile using Mirror is
    // not signed in (as it is not supported yet).
    DELETE_PROFILE_SIGNIN_REQUIRED_MIRROR_LACROS = 8,
    NUM_DELETE_PROFILE_METRICS
  };

  // The options for sync are logged after the user has changed their sync
  // setting. See people_handler.h.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum ProfileSync {
    SYNC_CUSTOMIZE = 0,           // User decided to customize sync
    SYNC_CHOOSE,                  // User chose what to sync
    SYNC_CREATED_NEW_PASSPHRASE,  // User created a passphrase to encrypt data
    SYNC_ENTERED_EXISTING_PASSPHRASE,  // User entered an existing passphrase
    NUM_PROFILE_SYNC_METRICS
  };

  enum ProfileGaia {
    GAIA_OPT_IN = 0,          // User changed to GAIA photo as avatar
    GAIA_OPT_OUT,             // User changed to not use GAIA photo as avatar
    NUM_PROFILE_GAIA_METRICS
  };

  enum ProfileAuth {
    AUTH_UNNECESSARY,     // Profile was not locked
    AUTH_LOCAL,           // Profile was authenticated locally
    AUTH_ONLINE,          // Profile was authenticated on-line
    AUTH_FAILED,          // Profile failed authentication
    AUTH_FAILED_OFFLINE,  // Profile failed authentication and was offline
    NUM_PROFILE_AUTH_METRICS
  };

#if BUILDFLAG(IS_ANDROID)
  // Enum for tracking user interactions with the account management menu
  // on Android.
  //
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.profiles
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: ProfileAccountManagementMetrics
  // GENERATED_JAVA_PREFIX_TO_STRIP: PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_
  enum ProfileAndroidAccountManagementMenu {
    // User arrived at the Account management screen.
    PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_VIEW = 0,
    // User arrived at the Account management screen, and clicked Add account.
    PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_ADD_ACCOUNT = 1,
    // User arrived at the Account management screen, and clicked Go incognito.
    PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_GO_INCOGNITO = 2,
    // User arrived at the Account management screen, and clicked on primary.
    PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_CLICK_PRIMARY_ACCOUNT = 3,
    // User arrived at the Account management screen, and clicked on secondary.
    PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_CLICK_SECONDARY_ACCOUNT = 4,
    // Despite the name of this enum, the following three interactions track
    // actions triggered from all user-triggered entry points for the signout
    // dialog.  Currently these are:
    // * The Account management settings screen
    // * The Sync settings screen
    // * The Google Services settings screen
    //
    // User toggled Chrome signout.
    PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_TOGGLE_SIGNOUT = 5,
    // User toggled Chrome signout, and clicked Signout.
    PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_SIGNOUT_SIGNOUT = 6,
    // User toggled Chrome signout, and clicked Cancel.
    PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_SIGNOUT_CANCEL = 7,
    // User arrived at the android Account management screen directly from some
    // Gaia requests.
    PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_DIRECT_ADD_ACCOUNT = 8,
    NUM_PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_METRICS,
  };
#endif  // BUILDFLAG(IS_ANDROID)

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
  static void LogLacrosPrimaryProfileFirstRunOutcome(
      ProfileSignedInFlowOutcome outcome);
  static void LogProfileAvatarSelection(size_t icon_index);
  static void LogProfileDeleteUser(ProfileDelete metric);
  static void LogProfileSwitchGaia(ProfileGaia metric);
  static void LogProfileSyncInfo(ProfileSync metric);

#if BUILDFLAG(IS_ANDROID)
  static void LogProfileAndroidAccountManagementMenu(
      ProfileAndroidAccountManagementMenu metric,
      signin::GAIAServiceType gaia_service);
#endif  // BUILDFLAG(IS_ANDROID)

  // These functions should only be called on the UI thread because they hook
  // into g_browser_process through a helper function.
  static void LogProfileLaunch(Profile* profile);
  static void LogProfileUpdate(const base::FilePath& profile_path);

  // Records the count of KeyedService active for the System Profile histogram.
  // Expects only System Profiles.
  static void LogSystemProfileKeyedServicesCount(Profile* profile);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_METRICS_H_
