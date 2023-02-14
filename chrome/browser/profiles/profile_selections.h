// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_SELECTIONS_H_
#define CHROME_BROWSER_PROFILES_PROFILE_SELECTIONS_H_

#include "base/feature_list.h"

#include "third_party/abseil-cpp/absl/types/optional.h"

// This feature flag will change the default beahvoir of
// `ProfileSelections::system_profile_selection_` when the value is not set. The
// old behavior simply follows `regular_profile_selection_` value, with the
// experiment active the default value will be `ProfileSelection::kNone`,
// meaning the default ProfileSelection for System Profile will be no profile.
// This feature flag will only affect builders that has are marked as
// `Experimental Builders` below, and any customized builder that will not
// explicitly use `ProfileSelections::Builder::WithSystem()`.
BASE_DECLARE_FEATURE(kSystemProfileSelectionDefaultNone);

class Profile;

// A helper function that checks whether Keyed Services should be created for
// the given `profile` based on the default profile type value. Currently checks
// if it is the System Profile and if it's equivalent feature flag to disable
// the creation of Keyed Services (`kSystemProfileSelectionDefaultNone`) is
// activated.
bool AreKeyedServicesDisabledForProfileByDefault(const Profile* profile);

// The class `ProfileSelections` and enum `ProfileSelection` are not coupled
// with the usage of `ProfileKeyedServiceFactory`, however the experiment of
// changing the default value of `ProfileSelections` behavior is mainly done for
// the `ProfileKeyedServiceFactory`.
// If other usages are needed it is best not to use the builders that contains
// experimental code (mentioned below).

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
class ProfileSelections {
 public:
  ProfileSelections(const ProfileSelections& other);
  ~ProfileSelections();

  static const ProfileSelection kRegularProfileDefault =
      ProfileSelection::kOriginalOnly;
  static const ProfileSelection kSystemProfileExperimentDefault =
      ProfileSelection::kNone;

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

  // - Predefined `ProfileSelections`

  // Regular builders (independent of the experiments):

  // All Profiles are selected.
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | self       | self       |
  // | Guest   | self       | self       |
  // | System  | self       | self       |
  // | Ash Int.| self       | self       |
  // +---------+------------+------------+
  static ProfileSelections BuildForAllProfiles();

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
  // | Ash Int.| self       | no profile |
  // +---------+------------+------------+
  static ProfileSelections BuildForRegularProfile();

  // Only select the regular profile and incognito for regular profiles. No
  // profiles for Guest and System profiles. "NonExperimental" is added to
  // differentiate with the experimental behavior during the experiment, once
  // done it will be the equivalent builder.
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
  // | Ash Int.| self       | self       |
  // +---------+------------+------------+
  static ProfileSelections BuildForRegularAndIncognitoNonExperimental();

  // Redirects incognito profiles to their original regular profile. No
  // profiles for Guest and System profiles. "NonExperimental" is added to
  // differentiate with the experimental behavior during the experiment, once
  // done it will be the equivalent builder.
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
  // | Ash Int.| self       | original   |
  // +---------+------------+------------+
  static ProfileSelections BuildRedirectedInIncognitoNonExperimental();

  // Redirects all OTR profiles to their original profiles.
  // Includes all profile types (Regular, Guest and System).
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | self       | original   |
  // | Guest   | self       | original   |
  // | System  | self       | original   |
  // | Ash Int.| self       | original   |
  // +---------+------------+------------+
  static ProfileSelections BuildRedirectedToOriginal();

  // Experimental builders:
  //
  // Experimental builders (should only be used for the transition from
  // `BrowserContextKeyedServiceFactory` to `ProfileKeyedServiceFactory`):
  // The following builder will contain experimental code indirectly, by not
  // giving a value to Guest and System Profile (unless forced by parameters).
  // The experiment is targeted to affect usages on
  // `ProfileKeyedServiceFactory`. During the experiment phase, these builders
  // will not have very accurate function names, the name is based on the end
  // result behavior the experiment enforced behavior. With/Without experiment
  // behavior is described per experimental builder. The parameters force_* will
  // allow to have an easier transition when adapting to the experiment.

  // Default implementation, without the experiment:
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | self       | no profile |
  // | Guest   | self       | no profile |
  // | System  | self       | no profile |
  // | Ash Int.| self       | no profile |
  // +---------+------------+------------+
  //
  // After the migration(crbug.com/1284664) this default behaviour will change.
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | self       | no profile |
  // | Guest   | no profile | no profile |
  // | System  | no profile | no profile |
  // | Ash Int.| self       | no profile |
  // +---------+------------+------------+
  //
  // Parameters: (used during the experiment)
  // - force_guest: true, force Guest with `ProfileSelection::kOriginalOnly`.
  // - force_system: true, force System with `ProfileSelection::kOriginalOnly`.
  static ProfileSelections BuildDefault(bool force_guest = true,
                                        bool force_system = false);

  // Without the experiment:
  // - Returns Regular for Regular, incognito and other regular OTR profiles.
  // - Returns Guest Original for GuestOriginal  and GuestOTR (same as Regular).
  // - Returns System Original for SystemOriginal  and SystemOTR (same as
  // Regular).
  //
  // With the experiment:
  // - Returns Regular for Regular, incognito and other regular OTR profiles.
  // - Return nullptr for all Guest and System profiles.
  //
  // Without the experiment:
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | self       | original   |
  // | Guest   | self       | original   |
  // | System  | self       | original   |
  // | Ash Int.| self       | original   |
  // +---------+------------+------------+
  //
  // With the experiment:
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | self       | original   |
  // | Guest   | no profile | no profile |
  // | System  | no profile | no profile |
  // | Ash Int.| self       | original   |
  // +---------+------------+------------+
  //
  // Parameters: (used during the experiment)
  // - force_guest: true, force Guest with
  // `ProfileSelecion::kRedirectedToOriginal`.
  // - force_system: true, force System with
  // `ProfileSelecion::kRedirectedToOriginal`.
  static ProfileSelections BuildRedirectedInIncognito(
      bool force_guest = true,
      bool force_system = false);

  // Without the experiment:
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | self       | self       |
  // | Guest   | self       | self       |
  // | System  | self       | self       |
  // | Ash Int.| self       | self       |
  // +---------+------------+------------+
  //
  // With the experiment:
  // +---------+------------+------------+
  // |         |  Original  |    OTR     |
  // +---------+------------+------------+
  // | Regular | self       | self       |
  // | Guest   | no profile | no profile |
  // | System  | no profile | no profile |
  // | Ash Int.| self       | self       |
  // +---------+------------+------------+
  //
  // Parameters: (used during the experiment)
  // - force_guest: true, force Guest with `ProfileSelecion::kOwnInstance`.
  // - force_system: true, force System with `ProfileSelecion::kOwnInstance`.
  static ProfileSelections BuildForRegularAndIncognito(
      bool force_guest = true,
      bool force_system = false);

  // Given a Profile and a ProfileSelection enum, returns the right profile
  // (can potentially return nullptr).
  Profile* ApplyProfileSelection(Profile* profile) const;

 private:
  // Default constructor settings sets Regular Profile ->
  // `ProfileSelection::kOriginalOnly`. It should be constructed through the
  // Builder. Value for Guest, System and Ash internals profile not being
  // overridden will default to the behaviour of Regular Profile.
  ProfileSelections();

  void SetProfileSelectionForRegular(ProfileSelection selection);
  void SetProfileSelectionForGuest(ProfileSelection selection);
  void SetProfileSelectionForSystem(ProfileSelection selection);
  void SetProfileSelectionForAshInternals(ProfileSelection selection);

  // Returns the `ProfileSelection` based on the profile information through the
  // set mapping.
  ProfileSelection GetProfileSelection(const Profile* profile) const;

  // Default value for the mapping of
  // Regular Profile -> `ProfileSelection::kOriginalOnly`
  // Not assigning values for Guest and System Profiles now defaults to the
  // behavior of regular profiles. This will change later on to default to
  // `ProfileSelection::kNone`.
  ProfileSelection regular_profile_selection_ = kRegularProfileDefault;
  absl::optional<ProfileSelection> guest_profile_selection_;
  absl::optional<ProfileSelection> system_profile_selection_;
  absl::optional<ProfileSelection> ash_internals_profile_selection_;
};

#endif  // !CHROME_BROWSER_PROFILES_PROFILE_SELECTIONS_H_
