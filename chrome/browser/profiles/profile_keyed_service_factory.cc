// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

Profile* ApplyProfileSelection(Profile* profile, ProfileSelection selection) {
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

}  // namespace

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

ProfileSelections ProfileSelections::BuildDefault() {
  return ProfileSelections::Builder().Build();
}

ProfileSelections ProfileSelections::BuildNoServicesForAllProfiles() {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kNone)
      .Build();
}

ProfileSelections ProfileSelections::BuildServicesForRegularProfile() {
  return ProfileSelections::Builder()
      .WithGuest(ProfileSelection::kNone)
      .WithSystem(ProfileSelection::kNone)
      .Build();
}

ProfileSelections ProfileSelections::BuildServicesRedirectedInIncognito() {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kRedirectedToOriginal)
      .WithGuest(ProfileSelection::kNone)
      .WithSystem(ProfileSelection::kNone)
      .Build();
}

ProfileSelections ProfileSelections::BuildServicesRedirectedInOTR() {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kRedirectedToOriginal)
      .Build();
}

ProfileSelection ProfileSelections::GetProfileSelection(
    Profile* profile) const {
  // Treat other off the record profiles as Incognito (primary otr) Profiles.
  if (profile->IsRegularProfile() || profile->IsIncognitoProfile() ||
      profile_metrics::GetBrowserProfileType(profile) ==
          profile_metrics::BrowserProfileType::kOtherOffTheRecordProfile)
    return regular_profile_selection_;

  if (profile->IsGuestSession()) {
    // if the default value for GuestProfile is overridden, use it.
    // otherwise, redirect to the old behavior (same as regular profile).
    // This is used for both original guest profile (not user visible) and for
    // the off-the-record guest (user visible, ui guest session).
    return guest_profile_selection_.value_or(regular_profile_selection_);
  }

  if (profile->IsSystemProfile()) {
    // if the default value for SystemProfile is overridden, use it.
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

ProfileKeyedServiceFactory::ProfileKeyedServiceFactory(const char* name)
    : ProfileKeyedServiceFactory(name, ProfileSelections::BuildDefault()) {}

ProfileKeyedServiceFactory::ProfileKeyedServiceFactory(
    const char* name,
    const ProfileSelections& profile_selections)
    : BrowserContextKeyedServiceFactory(
          name,
          BrowserContextDependencyManager::GetInstance()),
      profile_selections_(profile_selections) {}

ProfileKeyedServiceFactory::~ProfileKeyedServiceFactory() = default;

// BrowserContextKeyedServiceFactory
content::BrowserContext* ProfileKeyedServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  ProfileSelection selection = profile_selections_.GetProfileSelection(profile);

  return ApplyProfileSelection(profile, selection);
}
