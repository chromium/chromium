// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_SELECTIONS_H_
#define CHROME_BROWSER_PROFILES_PROFILE_SELECTIONS_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

// Enum used to map the logic of selecting the right profile for the service to
// be created for, based on the given profile.
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
class ProfileSelections {
 public:
  ProfileSelections(const ProfileSelections& other);
  ~ProfileSelections();

  // - Predefined `ProfileSelections`

  // Default implementation, as of now:
  // - No services in OTR.
  // - Regular profile returns itself (original).
  // - Guest and System profiles follow Regular profile behaviour.
  //
  // After the migration(crbug.com/1284664) this default behaviour will change.
  // It will be similar to the current `BuildServicesForRegularProfile()`.
  // - No services in OTR.
  // - Regular profile returns itself (original).
  // - No services for Guest and System profile.
  static ProfileSelections BuildDefault();

  // Services available for all profiles.
  static ProfileSelections BuildServicesForAllProfiles();

  // No services for all profiles.
  static ProfileSelections BuildNoServicesForAllProfiles();

  // Only build services for the regular profile.
  static ProfileSelections BuildServicesForRegularProfile();

  // Redirects building services for regular off the record profiles (incognito
  // and other off the record profiles) to regular Profile.  Doesn't build
  // services for Guest and System profiles.
  static ProfileSelections BuildServicesRedirectedInIncognito();

  // Redirects building services for both OTR and Original profile to Original
  // Profile for all profile types (Regular, Guest and System).
  static ProfileSelections BuildServicesRedirectedToOriginal();

  // Builder to construct the `ProfileSelections` parameters.
  class Builder {
   public:
    Builder();
    ~Builder();

    // Builder setters
    Builder& WithRegular(ProfileSelection selection);
    // Note: When Guest and Regular are not mutually exclusive on Ash and
    // Lacros, a Profile can potentially return true for both
    // `IsRegularProfile()` and `IsGuestSession()`. This is currently not
    // supported by the API, meaning that extra code might need to be added to
    // make sure all the cases are properly covered. Using the API, if both
    // `IsRegularProfile()` and `IsGuestSession()` are true, Regular
    // ProfileSelection logic will be used.
    // TODO(crbug.com/1348572): remove this comment once `IsGuestSession()` is
    // fixed.
    Builder& WithGuest(ProfileSelection selection);
    Builder& WithSystem(ProfileSelection selection);

    // Builds the `ProfileSelections`.
    ProfileSelections Build();

   private:
    std::unique_ptr<ProfileSelections> selections_;
  };

  // Given a Profile and a ProfileSelection enum, returns the right profile
  // (can potentially return nullptr).
  Profile* ApplyProfileSelection(Profile* profile) const;

 private:
  // Default constructor settings sets Regular Profile ->
  // `ProfileSelection::kOriginalOnly`. It should be constructed through the
  // Builder. Value for Guest and System profile not being overridden will
  // default to the behaviour of Regular Profile.
  ProfileSelections();

  void SetProfileSelectionForRegular(ProfileSelection selection);
  void SetProfileSelectionForGuest(ProfileSelection selection);
  void SetProfileSelectionForSystem(ProfileSelection selection);

  // Returns the `ProfileSelection` based on the profile information through the
  // set mapping.
  ProfileSelection GetProfileSelection(Profile* profile) const;

  // Default value for the mapping of
  // Regular Profile -> `ProfileSelection::kOriginalOnly`
  // Not assigning values for Guest and System Profiles now defaults to the
  // behavior of regular profiles. This will change later on to default to
  // `ProfileSelection::kNone`.
  ProfileSelection regular_profile_selection_ = ProfileSelection::kOriginalOnly;
  absl::optional<ProfileSelection> guest_profile_selection_;
  absl::optional<ProfileSelection> system_profile_selection_;
};

#endif  // !CHROME_BROWSER_PROFILES_PROFILE_SELECTIONS_H_
