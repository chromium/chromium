// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_selections.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/profile_metrics/browser_profile_type.h"

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
      .Build();
}

ProfileSelections ProfileSelections::BuildNoProfilesSelected() {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kNone)
      .Build();
}

ProfileSelections ProfileSelections::BuildForRegularProfile() {
  return ProfileSelections::Builder()
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
    Profile* profile) const {
  // Treat other off the record profiles as Incognito (primary otr) Profiles.
  if (profile->IsRegularProfile() || profile->IsIncognitoProfile() ||
      profile_metrics::GetBrowserProfileType(profile) ==
          profile_metrics::BrowserProfileType::kOtherOffTheRecordProfile)
    return regular_profile_selection_;

  if (profile->IsGuestSession()) {
    // If the default value for GuestProfile is overridden, use it.
    // otherwise, redirect to the old behavior (same as regular profile).
    // This is used for both original guest profile (not user visible) and for
    // the off-the-record guest (user visible, ui guest session).
    return guest_profile_selection_.value_or(regular_profile_selection_);
  }

  if (profile->IsSystemProfile()) {
    // If the default value for SystemProfile is overridden, use it.
    // otherwise, redirect to the old behavior (same as regular profile).
    // This is used for both original system profile (not user visible) and for
    // the off-the-record system profile (used in the Profile Picker).
    return system_profile_selection_.value_or(regular_profile_selection_);
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
