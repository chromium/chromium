// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_selections.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_types_ash.h"
#include "components/profile_metrics/browser_profile_type.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/common/chrome_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

BASE_FEATURE(kSystemProfileSelectionDefaultNone,
             "SystemProfileSelectionDefaultNone",
             base::FeatureState::FEATURE_DISABLED_BY_DEFAULT);

bool AreKeyedServicesDisabledForProfileByDefault(const Profile* profile) {
  if (profile && profile->IsSystemProfile()) {
    // The default behavior of the system profile selection depends on the value
    // of `kSystemProfileSelectionDefaultNone` feature flag.
    ProfileSelection system_profile_default =
        base::FeatureList::IsEnabled(kSystemProfileSelectionDefaultNone)
            ? ProfileSelections::kSystemProfileExperimentDefault
            : ProfileSelections::kRegularProfileDefault;

    return system_profile_default == ProfileSelection::kNone;
  }

  return false;
}

ProfileSelections::Builder::Builder()
    : selections_(base::WrapUnique(new ProfileSelections())) {}

ProfileSelections::Builder::~Builder() = default;

ProfileSelections::Builder& ProfileSelections::Builder::WithRegular(
    ProfileSelection selection) {
  selections_->SetProfileSelectionForRegular(selection);
  return *this;
}

ProfileSelections::Builder& ProfileSelections::Builder::WithGuest(
    ProfileSelection selection) {
  selections_->SetProfileSelectionForGuest(selection);
  return *this;
}

ProfileSelections::Builder& ProfileSelections::Builder::WithSystem(
    ProfileSelection selection) {
  selections_->SetProfileSelectionForSystem(selection);
  return *this;
}

ProfileSelections::Builder& ProfileSelections::Builder::WithAshInternals(
    ProfileSelection selection) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  selections_->SetProfileSelectionForAshInternals(selection);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return *this;
}

ProfileSelections ProfileSelections::Builder::Build() {
  DCHECK(selections_) << "Build() already called";

  ProfileSelections to_return = *selections_;
  selections_.reset();

  return to_return;
}

ProfileSelections::ProfileSelections() = default;
ProfileSelections::~ProfileSelections() = default;
ProfileSelections::ProfileSelections(const ProfileSelections& other) = default;

ProfileSelections ProfileSelections::BuildForAllProfiles() {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kOwnInstance)
      .WithGuest(ProfileSelection::kOwnInstance)
      .WithSystem(ProfileSelection::kOwnInstance)
      .WithAshInternals(ProfileSelection::kOwnInstance)
      .Build();
}

ProfileSelections ProfileSelections::BuildNoProfilesSelected() {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kNone)
      .WithGuest(ProfileSelection::kNone)
      .WithSystem(ProfileSelection::kNone)
      .WithAshInternals(ProfileSelection::kNone)
      .Build();
}

ProfileSelections ProfileSelections::BuildForRegularProfile() {
  return ProfileSelections::Builder()
      .WithGuest(ProfileSelection::kNone)
      .WithSystem(ProfileSelection::kNone)
      .Build();
}

ProfileSelections
ProfileSelections::BuildForRegularAndIncognitoNonExperimental() {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kOwnInstance)
      .WithGuest(ProfileSelection::kNone)
      .WithSystem(ProfileSelection::kNone)
      .Build();
}

ProfileSelections
ProfileSelections::BuildRedirectedInIncognitoNonExperimental() {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kRedirectedToOriginal)
      .WithGuest(ProfileSelection::kNone)
      .WithSystem(ProfileSelection::kNone)
      .Build();
}

ProfileSelections ProfileSelections::BuildRedirectedToOriginal() {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kRedirectedToOriginal)
      .WithGuest(ProfileSelection::kRedirectedToOriginal)
      .WithSystem(ProfileSelection::kRedirectedToOriginal)
      .Build();
}

ProfileSelections ProfileSelections::BuildDefault(bool force_guest,
                                                  bool force_system) {
  Builder builder;
  if (force_guest)
    builder.WithGuest(ProfileSelection::kOriginalOnly);
  if (force_system)
    builder.WithSystem(ProfileSelection::kOriginalOnly);
  return builder.Build();
}

ProfileSelections ProfileSelections::BuildRedirectedInIncognito(
    bool force_guest,
    bool force_system) {
  Builder builder;
  builder.WithRegular(ProfileSelection::kRedirectedToOriginal);
  if (force_guest)
    builder.WithGuest(ProfileSelection::kRedirectedToOriginal);
  if (force_system)
    builder.WithSystem(ProfileSelection::kRedirectedToOriginal);
  return builder.Build();
}

ProfileSelections ProfileSelections::BuildForRegularAndIncognito(
    bool force_guest,
    bool force_system) {
  Builder builder;
  builder.WithRegular(ProfileSelection::kOwnInstance);
  if (force_guest)
    builder.WithGuest(ProfileSelection::kOwnInstance);
  if (force_system)
    builder.WithSystem(ProfileSelection::kOwnInstance);
  return builder.Build();
}

Profile* ProfileSelections::ApplyProfileSelection(Profile* profile) const {
  DCHECK(profile);

  ProfileSelection selection = GetProfileSelection(profile);
  switch (selection) {
    case ProfileSelection::kNone:
      return nullptr;
    case ProfileSelection::kOriginalOnly:
      return profile->IsOffTheRecord() ? nullptr : profile;
    case ProfileSelection::kOwnInstance:
      return profile;
    case ProfileSelection::kRedirectedToOriginal:
      return profile->GetOriginalProfile();
    case ProfileSelection::kOffTheRecordOnly:
      return profile->IsOffTheRecord() ? profile : nullptr;
  }
}

ProfileSelection ProfileSelections::GetProfileSelection(
    const Profile* profile) const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // This check has to be performed before the check on
  // `profile->IsRegularProfile()` because profiles that are internal ASH
  // (non-user) profiles will also satisfy the later condition.
  if (!IsUserProfile(profile)) {
    // If the value for `ash_internals_profile_selection_` is not set, redirect
    // to the default behavior, which is the behavior given to the
    // RegularProfile.
    return ash_internals_profile_selection_.value_or(
        regular_profile_selection_);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Treat other off the record profiles as Incognito (primary otr) Profiles.
  if (profile->IsRegularProfile() || profile->IsIncognitoProfile() ||
      profile_metrics::GetBrowserProfileType(profile) ==
          profile_metrics::BrowserProfileType::kOtherOffTheRecordProfile) {
    return regular_profile_selection_;
  }

  if (profile->IsGuestSession()) {
    // If a value is not set for the Guest Profile Selection,
    // `ProfileSelection::kNone` is set by default, meaning no profile will be
    // selected.
    return guest_profile_selection_.value_or(ProfileSelection::kNone);
  }

  if (profile->IsSystemProfile()) {
    // Default value depends on the experiment
    // `kSystemProfileSelectionDefaultNone`. If experiment is active default
    // value is ProfileSelection::kNone, otherwise the behavior is redirected to
    // the `regular_profile_selection_` value (old default behavior).
    ProfileSelection system_profile_default =
        base::FeatureList::IsEnabled(kSystemProfileSelectionDefaultNone)
            ? ProfileSelections::kSystemProfileExperimentDefault
            : regular_profile_selection_;

    // If the value for SystemProfileSelection is set, use it.
    // Otherwise, use the default value set above.
    // This is used for both original system profile (not user visible) and for
    // the off-the-record system profile (used in the Profile Picker).
    return system_profile_selection_.value_or(system_profile_default);
  }

  NOTREACHED();
  return ProfileSelection::kNone;
}

void ProfileSelections::SetProfileSelectionForRegular(
    ProfileSelection selection) {
  regular_profile_selection_ = selection;
}

void ProfileSelections::SetProfileSelectionForGuest(
    ProfileSelection selection) {
  guest_profile_selection_ = selection;
}

void ProfileSelections::SetProfileSelectionForSystem(
    ProfileSelection selection) {
  system_profile_selection_ = selection;
}

void ProfileSelections::SetProfileSelectionForAshInternals(
    ProfileSelection selection) {
  ash_internals_profile_selection_ = selection;
}
