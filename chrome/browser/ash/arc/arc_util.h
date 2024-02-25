// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ARC_UTIL_H_
#define CHROME_BROWSER_ASH_ARC_ARC_UTIL_H_

#include <stdint.h>
#include <memory>

#include "ash/components/arc/session/arc_management_transition.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "storage/browser/file_system/file_system_url.h"

// Most utility should be put in components/arc/arc_util.{h,cc}, rather than
// here. However, some utility implementation requires other modules defined in
// chrome/, so this file contains such utilities.
// Note that it is not allowed to have dependency from components/ to chrome/
// by DEPS.

class AccountId;
class GURL;
class Profile;

namespace aura {
class Window;
}  // namespace aura

namespace base {
class FilePath;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace user_manager {
class User;
}  // namespace user_manager

namespace arc {

// Values to be stored in the local state preference to keep track of the
// filesystem encryption migration status.
enum FileSystemCompatibilityState : int32_t {
  // No migration has happened, user keeps using the old file system.
  kFileSystemIncompatible = 0,
  // Migration has happened. New filesystem is in use.
  kFileSystemCompatible = 1,
  // Migration has happened, and a notification about it was already shown.
  // This pref value will not be written anymore since we stopped showing the
  // notification, but existing profiles which had shown the notification can
  // have this value in their pref.
  kFileSystemCompatibleAndNotifiedDeprecated = 2,

  // Existing code assumes that kFileSystemIncompatible is the only state
  // representing incompatibility and other values are all variants of
  // "compatible" state. Be careful in the case adding a new enum value.
};

// Records ARC status i.e if ARC allowed or disallowed based on
// UnaffiliatedDeviceArcAllowed policy value.
void RecordArcStatusBasedOnDeviceAffiliationUMA(Profile* profile);

// Returns false if |profile| is not a real user profile but some internal
// profile for service purposes, which should be ignored for ARC and metrics
// recording. Also returns false if |profile| is null.
bool IsRealUserProfile(const Profile* profile);

// Returns true if ARC is allowed to run for the given profile.
// Otherwise, returns false, e.g. if the Profile is not for the primary user,
// ARC is not available on the device, it is in the flow to set up managed
// account creation.
// nullptr can be safely passed to this function. In that case, returns false.
bool IsArcAllowedForProfile(const Profile* profile);

// Returns whether ARC was successfully provisioned and the Primary/Device
// Account has been signed into ARC.
bool IsArcProvisioned(const Profile* profile);

// Returns true if the profile is temporarily blocked to run ARC in the current
// session, because the filesystem storing the profile is incompatible with the
// currently installed ARC version.
//
// The actual filesystem check is performed only when it is running on the
// Chrome OS device. Otherwise, it just returns the dummy value set by
// SetArcBlockedDueToIncompatibleFileSystemForTesting (false by default.)
bool IsArcBlockedDueToIncompatibleFileSystem(const Profile* profile);

// Sets the result of IsArcBlockedDueToIncompatibleFileSystem for testing.
void SetArcBlockedDueToIncompatibleFileSystemForTesting(bool block);

// Returns true if the user is already marked to be on a filesystem
// compatible to the currently installed ARC version. The check almost never
// is meaningful on test workstation. Usually it should be checked only when
// running on the real Chrome OS.
bool IsArcCompatibleFileSystemUsedForUser(const user_manager::User* user);

// Disallows ARC for all profiles for testing.
// In most cases, disabling ARC should be done via commandline. However,
// there are some cases to be tested where ARC is available, but ARC is not
// supported for some reasons (e.g. incognito mode, supervised user,
// secondary profile). On the other hand, some test infra does not support
// such situations (e.g. API test). This is for workaround to emulate the
// case.
void DisallowArcForTesting();

// Clears check if ARC allowed. For use at end of tests, in case a test
// has a profile with the same memory address as a profile in a previous test.
void ClearArcAllowedCheckForTesting();

// Resets check if ARC allowed for the given |profile|.
void ResetArcAllowedCheckForTesting(const Profile* profile);

// Returns whether the user has opted in (or is opting in now) to use Google
// Play Store on ARC.
// This is almost equivalent to the value of "arc.enabled" preference.
// However, in addition, if ARC is not allowed for the given |profile|, then
// returns false. Please see detailed condition for the comment of
// IsArcAllowedForProfile().
// Note: For historical reason, the preference name is not matched with the
// actual meaning.
bool IsArcPlayStoreEnabledForProfile(const Profile* profile);

// Returns whether the preference "arc.enabled" is managed or not.
// It is requirement for a caller to ensure ARC is allowed for the user of
// the given |profile|.
bool IsArcPlayStoreEnabledPreferenceManagedForProfile(const Profile* profile);

// Enables or disables Google Play Store on ARC. Currently, it is tied to
// ARC enabled state, too, so this also should trigger to enable or disable
// whole ARC system.
// If the preference is managed, then no-op.
// It is requirement for a caller to ensure ARC is allowed for the user of
// the given |profile|.
// TODO(hidehiko): De-couple the concept to enable ARC system and opt-in
// to use Google Play Store. Note that there is a plan to use ARC without
// Google Play Store, then ARC can run without opt-in. Returns false in case
// enabled state of the Play Store cannot be changed.
bool SetArcPlayStoreEnabledForProfile(Profile* profile, bool enabled);

// Returns whether all ARC related OptIn preferences (i.e.
// ArcBackupRestoreEnabled and ArcLocationServiceEnabled) are managed.
bool AreArcAllOptInPreferencesIgnorableForProfile(const Profile* profile);

// Returns true if ChromeOS OOBE opt-in window is currently showing.
bool IsArcOobeOptInActive();

// Returns true if opt-in during ChromeOS OOBE is triggered by configuration.
bool IsArcOobeOptInConfigurationBased();

// Returns true if Terms of Service negotiation is needed. Otherwise false.
bool IsArcTermsOfServiceNegotiationNeeded(const Profile* profile);

// Returns true if Terms of Service negotiation is needed in OOBE flow.
// Otherwise false. Similar to IsArcTermsOfServiceNegotiationNeeded but
// also checks set of preconditions and uses active user profile.
bool IsArcTermsOfServiceOobeNegotiationNeeded();

// Returns true if stats reporting is enabled. Otherwise false.
bool IsArcStatsReportingEnabled();

// Returns whether ARC opt-in in demo mode setup flow is in progress.
bool IsArcDemoModeSetupFlow();

// Checks and updates the preference value whether the underlying filesystem
// for the profile is compatible with ARC, when necessary. After it's done (or
// skipped), |callback| is run either synchronously or asynchronously.
void UpdateArcFileSystemCompatibilityPrefIfNeeded(
    const AccountId& account_id,
    const base::FilePath& profile_path,
    base::OnceClosure callback);

// Returns the supervision transition status as stored in profile prefs.
ArcManagementTransition GetManagementTransition(const Profile* profile);

// Returns true if Play Store package is present and can be launched in this
// session.
bool IsPlayStoreAvailable();

// Returns whether adding secondary account to ARC++ is enabled for child
// user.
bool IsSecondaryAccountForChildEnabled();

// Skip to show OOBE/in session UI asking users to set up ARC OptIn
// preferences, iff all of them are managed by the admin policy. Skips in
// session play terms of service for managed user and starts ARC directly.
// Leaves B&R/GLS off if not set by admin since users don't see the Tos page.
bool ShouldStartArcSilentlyForManagedProfile(const Profile* profile);

// Returns an ARC window with the given task ID.
aura::Window* GetArcWindow(int32_t task_id);

// Creates a web contents for an ARC Custom Tab using the given profile and
// url.
std::unique_ptr<content::WebContents> CreateArcCustomTabWebContents(
    Profile* profile,
    const GURL& url);

// Adds a suffix to the name based on the account type. If profile is not
// provided, then defaults to the primary user profile.
std::string GetHistogramNameByUserType(const std::string& base_name,
                                       const Profile* profile = nullptr);

// Adds a suffix to the name based on the account type of the primary user
// profile.
std::string GetHistogramNameByUserTypeForPrimaryProfile(
    const std::string& base_name);

using ConvertToContentUrlsAndShareCallback =
    base::OnceCallback<void(const std::vector<GURL>& content_urls)>;

// Asynchronously converts Chrome OS file system URLs to content:// URLs
// using file_manager::util::ConvertToContentUrls with the supplied profile.
// Subsequently, if the URLS needs to be made available for ARCVM, it will
// be shared by Seneschal.
void ConvertToContentUrlsAndShare(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& file_system_urls,
    ConvertToContentUrlsAndShareCallback callback);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ARC_UTIL_H_
