// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_SELECTIONS_H_
#define CHROME_BROWSER_PROFILES_PROFILE_SELECTIONS_H_

#include <memory>

class Profile;

// A helper function that checks whether Keyed Services should be created for
// the given `profile` based on the default profile type value. Currently only
// returns true for a valid System Profile.
// This method is intended to be used only to bypass multiple factory/service
// checks.
bool AreKeyedServicesDisabledForProfileByDefault(const Profile* profile);

// The class `ProfileSelections` and enum `ProfileSelection` are not coupled
// with the usage of `ProfileKeyedServiceFactory` and can be used separately to
// filter out profiles based on their types.

// Enum used to map the logic of selecting the right profile based on the given
// profile.
enum class ProfileSelection {
  kNone,                  // Original: No Profile  --  OTR: No Profile
  kOriginalOnly,          // Original: Self        --  OTR: No Profile
  kOwnInstance,           // Original: Self        --  OTR: Self
  kRedirectedToOriginal,  // Original: Self        --  OTR: Original
  kOffTheRecordOnly       // Original: No Profile  --  OTR: Self
};

// Contains the logic for ProfileSelection for the different main Profile types
// (Regular, Guest and System). Each of these profile types also have Off the
// Record profiles equivalent, e.g. Incognito is Off the Record profile for
// Regular profile, the Guest user-visible profile is off-the-record, the
// Profile Picker uses the off-the-record System Profile.
// Maps Profile types to `ProfileSelection`.
//
// You can use predefined builders listed below for easier usages.
// If you need non trivial behavior (for Guest or System profiles for example),
// you should write your own expanded version of the builder.
class ProfileSelections {
 public:
  ProfileSelections(const ProfileSelections& other);
  ~ProfileSelections();

  // Builder to construct the `ProfileSelections` parameters for different
  // profile types.
  class Builder {
   public:
    Builder();
    ~Builder();

    // Builder setters
    Builder& WithRegular(ProfileSelection selection);
    // Note: When Guest and Regular are not mutually exclusive on ChromeOS, a
    // Profile can potentially return true for both `IsRegularProfile()` and
    // `IsGuestSession()`. This is currently not supported by the API, meaning
    // that extra code might need to be added to make sure all the cases are
    // properly covered. Using the API, if both `IsRegularProfile()` and
    // `IsGuestSession()` are true, Regular ProfileSelection logic will be used.
    // TODO(crbug.com/40233408): remove this comment once `IsGuestSession()` is
    // fixed.
    Builder& WithGuest(ProfileSelection selection);
    Builder& WithSystem(ProfileSelection selection);
    // In Ash there are internal profiles that are not user profiles, such as a
    // the signin or the lockscreen profile.
    // Note: ash internal profiles are regular profiles. If the value is not
    // set, they will default to the regular profiles behavior.
    Builder& WithAshInternals(ProfileSelection selection);

    // Builds the `ProfileSelections`.
    ProfileSelections Build();

   private:
    std::unique_ptr<ProfileSelections> selections_;
  };

  // - Predefined `ProfileSelections` builders:

  // Only select the regular profile.
  // Note: Ash internal profiles are of type Regular. In order to have a
  // different filter for those profiles, a specific builder should be
  // constructed with a value for
  // `ProfileSelections::Builder::WithAshInternals()`.
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | self       | no profile |
  // | Guest   | no profile | no profile |
  // | System  | no profile | no profile |
  // | Ash Int.| no profile | no profile |
  // +---------+------------+------------+
  static ProfileSelections BuildForRegularProfile();

  // No Profiles are selected.
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | no profile | no profile |
  // | Guest   | no profile | no profile |
  // | System  | no profile | no profile |
  // | Ash Int.| no profile | no profile |
  // +---------+------------+------------+
  static ProfileSelections BuildNoProfilesSelected();

  // Only select the regular profile and incognito for regular profiles. No
  // profiles for Guest and System profiles.
  // Note: Ash internal profiles are of type Regular. In order to have a
  // different filter for those profiles, a specific builder should be
  // constructed with a value for
  // `ProfileSelections::Builder::WithAshInternals()`.
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | self       | self       |
  // | Guest   | no profile | no profile |
  // | System  | no profile | no profile |
  // | Ash Int.| no profile | no profile |
  // +---------+------------+------------+
  static ProfileSelections BuildForRegularAndIncognito();

  // Redirects incognito profiles to their original regular profile. No
  // profiles for Guest and System profiles.
  // Note: Ash internal profiles are of type Regular. In order to have a
  // different filter for those profiles, a specific builder should be
  // constructed with a value for
  // `ProfileSelections::Builder::WithAshInternals()`.
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | self       | original   |
  // | Guest   | no profile | no profile |
  // | System  | no profile | no profile |
  // | Ash Int.| no profile | no profile |
  // +---------+------------+------------+
  static ProfileSelections BuildRedirectedInIncognito();

  // Given a Profile and a ProfileSelection enum, returns the right profile
  // (can potentially return nullptr).
  // The `profile` is expected to be non-null.
  Profile* ApplyProfileSelection(Profile* profile) const;

 private:
  // Default constructor settings sets Regular Profile ->
  // `ProfileSelection::kOriginalOnly`. It should be constructed through the
  // Builder. Value for Guest, System and Ash internals profile not being
  // overridden will default to `ProfileSelection::kNone`.
  ProfileSelections();

  void SetProfileSelectionForRegular(ProfileSelection selection);
  void SetProfileSelectionForGuest(ProfileSelection selection);
  void SetProfileSelectionForSystem(ProfileSelection selection);
  void SetProfileSelectionForAshInternals(ProfileSelection selection);

  // Returns the `ProfileSelection` based on the profile information through the
  // set mapping.
  ProfileSelection GetProfileSelection(Profile* profile) const;

  // Default value for the mapping of
  // Regular Profile -> `ProfileSelection::kOriginalOnly`
  // Other Profile -> `ProfileSelection::kNone`.
  ProfileSelection regular_profile_selection_ = ProfileSelection::kOriginalOnly;
  ProfileSelection guest_profile_selection_ = ProfileSelection::kNone;
  ProfileSelection system_profile_selection_ = ProfileSelection::kNone;
  ProfileSelection ash_internals_profile_selection_ = ProfileSelection::kNone;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_SELECTIONS_H_
