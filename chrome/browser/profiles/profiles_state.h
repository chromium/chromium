// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILES_STATE_H_
#define CHROME_BROWSER_PROFILES_PROFILES_STATE_H_

#include <string>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include <vector>
#endif

struct AccountInfo;
struct CoreAccountInfo;
class Browser;
class PrefRegistrySimple;
class PrefService;
class Profile;

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace profiles {

// Assortment of methods for dealing with profiles.
// TODO(michaelpg): Most of these functions can be inlined or moved to more
// appropriate locations.

// Checks if multiple profiles is enabled.
bool IsMultipleProfilesEnabled();

// Returns the path to the default profile directory, based on the given
// user data directory.
base::FilePath GetDefaultProfileDir(const base::FilePath& user_data_dir);

// Register multi-profile related preferences in Local State.
void RegisterPrefs(PrefRegistrySimple* registry);

// Sets the last used profile pref to |profile_dir|, unless |profile_dir| is the
// System Profile directory, which is an invalid last used profile.
void SetLastUsedProfile(const base::FilePath& profile_dir);

// Returns true if the profile is a regular profile and specifically not an Ash
// internal profile. Callers who do not care about checking for Ash internal
// profiles should use `Profile::IsRegularProfile()` instead.
bool IsRegularUserProfile(Profile* profile);

#if !BUILDFLAG(IS_ANDROID)
// Returns the display name of the specified on-the-record profile (or guest),
// specified by |profile_path|, used in the avatar button or user manager. If
// |profile_path| is the guest path, it will return IDS_GUEST_PROFILE_NAME. If
// there is only one local profile present, it will return
// IDS_SINGLE_PROFILE_DISPLAY_NAME, unless the profile has a user entered
// custom name.
std::u16string GetAvatarNameForProfile(const base::FilePath& profile_path);

#if !BUILDFLAG(IS_CHROMEOS)
// Update the name of |profile| to |new_profile_name|. This updates the profile
// preferences, which triggers an update in the ProfileAttributesStorage. This
// method should be called when the user is explicitely changing the profile
// name, as it will always set |prefs::kProfileUsingDefaultName| to false.
void UpdateProfileName(Profile* profile,
                       const std::u16string& new_profile_name);

#endif  // !BUILDFLAG(IS_CHROMEOS)

// Returns whether the |browser|'s profile is not incognito (a regular profile
// or a guest session).
// The distinction is needed because guest profiles and incognito profiles are
// implemented as off-the-record profiles.
bool IsRegularOrGuestSession(Browser* browser);

// Returns true if starting in guest mode is requested at startup (e.g. through
// command line argument). If |show_warning| is true, send a warning if guest
// mode is requested but not allowed by policy.
bool IsGuestModeRequested(const base::CommandLine& command_line,
                          PrefService* local_state,
                          bool show_warning);

// Returns true if profile creation is allowed by prefs.
bool IsProfileCreationAllowed();

// Returns true if guest mode is allowed by prefs, for an entry point not
// associated with a specific profile.
bool IsGuestModeEnabled();

// Returns true if guest mode is allowed by prefs, for an entry point that is
// associated with |profile|.
bool IsGuestModeEnabled(const Profile& profile);

#if BUILDFLAG(IS_CHROMEOS)
// Returns true if secondary profiles are allowed by
// |prefs::kLacrosSecondaryProfilesAllowed|.
bool AreSecondaryProfilesAllowed();
#endif  // BUILDFLAG(IS_CHROMEOS)

// Returns true if sign in is required to browse as this profile.  Call with
// profile->GetPath() if you have a profile pointer.
// TODO(mlerman): Refactor appropriate calls to
// ProfileAttributesStorage::IsSigninRequired to call here instead.
bool IsProfileLocked(const base::FilePath& profile_path);

#if !BUILDFLAG(IS_CHROMEOS)
// Starts an update for a new version of the Gaia profile picture and other
// profile info.
void UpdateGaiaProfileInfoIfNeeded(Profile* profile);
#endif  // !BUILDFLAG(IS_CHROMEOS)

// If the profile given by |profile_path| is loaded in the ProfileManager, use
// a BrowsingDataRemover to delete all the Profile's data.
void RemoveBrowsingDataForProfile(const base::FilePath& profile_path);

// Returns true if the current session is a Demo session.
bool IsDemoSession();

// Returns true if the current session is a Chrome App Kiosk session.
bool IsChromeAppKioskSession();

#if !BUILDFLAG(IS_CHROMEOS)
// Returns the default name for a new enterprise profile. Never returns an empty
// string.
std::u16string GetDefaultNameForNewEnterpriseProfile(
    const std::string& hosted_domain = std::string());

// Returns the default name for a new signed-in profile, based on
// `account_info`. Never returns an empty string.
std::u16string GetDefaultNameForNewSignedInProfile(
    const AccountInfo& account_info);

// The same as above but using incomplete account info. `account_info` must be
// valid. Never returns an empty string.
std::u16string GetDefaultNameForNewSignedInProfileWithIncompleteInfo(
    const CoreAccountInfo& account_info);
#endif  // !BUILDFLAG(IS_CHROMEOS)

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace profiles

#endif  // CHROME_BROWSER_PROFILES_PROFILES_STATE_H_
