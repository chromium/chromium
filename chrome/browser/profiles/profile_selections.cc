// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_selections.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/profile_metrics/browser_profile_type.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

bool AreKeyedServicesDisabledForProfileByDefault(const Profile* profile) {
  // By default disable all services for System Profile.
  // Even though having no services is also the default value for Guest Profile,
  // this is not really the case in practice because a lot of Service Factories
  // override the default value for the `ProfileSelection` of the Guest Profile.
  if (profile && profile->IsSystemProfile()) {
    return true;
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
#if BUILDFLAG(IS_CHROMEOS)
  selections_->SetProfileSelectionForAshInternals(selection);
#endif  // BUILDFLAG(IS_CHROMEOS)
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
      .WithAshInternals(ProfileSelection::kNone)
      .Build();
}

ProfileSelections ProfileSelections::BuildForRegularAndIncognito() {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kOwnInstance)
      .WithGuest(ProfileSelection::kNone)
      .WithSystem(ProfileSelection::kNone)
      .WithAshInternals(ProfileSelection::kNone)
      .Build();
}

ProfileSelections ProfileSelections::BuildRedirectedInIncognito() {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kRedirectedToOriginal)
      .WithGuest(ProfileSelection::kNone)
      .WithSystem(ProfileSelection::kNone)
      .WithAshInternals(ProfileSelection::kNone)
      .Build();
}

Profile* ProfileSelections::ApplyProfileSelection(Profile* profile) const {
  CHECK(profile);

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
#if BUILDFLAG(IS_CHROMEOS)
  // This check has to be performed before the check on
  // `profile->IsRegularProfile()` because profiles that are internal ASH
  // (non-user) profiles will also satisfy the later condition.
  if (!ash::IsUserBrowserContext(profile)) {
    return ash_internals_profile_selection_;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Treat other off the record profiles as Incognito (primary otr) Profiles.
  if (profile->IsRegularProfile() || profile->IsIncognitoProfile() ||
      profile_metrics::GetBrowserProfileType(profile) ==
          profile_metrics::BrowserProfileType::kOtherOffTheRecordProfile) {
    return regular_profile_selection_;
  }

  if (profile->IsGuestSession()) {
    return guest_profile_selection_;
  }

  if (profile->IsSystemProfile()) {
    return system_profile_selection_;
  }

  NOTREACHED_IN_MIGRATION();
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
