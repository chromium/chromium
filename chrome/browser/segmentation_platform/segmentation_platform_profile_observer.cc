// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/segmentation_platform_profile_observer.h"

#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

namespace segmentation_platform {
namespace {

bool AnyOffTheRecordProfilesExist(const std::vector<Profile*>& profiles,
                                  Profile* destroying_profile) {
  bool has_otr_profiles = false;
  for (auto* profile : profiles) {
    // If the profile being destroyed is the last OTR profile, skip to check
    // next profile.
    if (profile == destroying_profile->GetOriginalProfile() &&
        profile->GetAllOffTheRecordProfiles().size() == 1) {
      continue;
    }

    if (profile->HasAnyOffTheRecordProfile()) {
      has_otr_profiles = true;
      break;
    }
  }

  return has_otr_profiles;
}

}  // namespace

SegmentationPlatformProfileObserver::SegmentationPlatformProfileObserver(
    SegmentationPlatformService* segmentation_platform_service,
    ProfileManager* profile_manager)
    : segmentation_platform_service_(segmentation_platform_service),
      profile_manager_(profile_manager) {
  profile_manager_->AddObserver(this);
  // Start observing all the regular and OTR profiles.
  for (auto* profile : profile_manager_->GetLoadedProfiles()) {
    OnProfileAdded(profile);
    for (Profile* otr_profile : profile->GetAllOffTheRecordProfiles())
      OnProfileAdded(otr_profile);
  }
}

SegmentationPlatformProfileObserver::~SegmentationPlatformProfileObserver() {
  if (profile_manager_)
    profile_manager_->RemoveObserver(this);
}

void SegmentationPlatformProfileObserver::OnProfileAdded(Profile* profile) {
  observed_profiles_.AddObservation(profile);

  // Check if we have any OTR profiles.
  bool has_otr_profiles = false;
  for (auto* loaded_profile : profile_manager_->GetLoadedProfiles()) {
    if (loaded_profile->HasAnyOffTheRecordProfile()) {
      has_otr_profiles = true;
      break;
    }
  }

  NotifyExistenceOfOTRProfile(has_otr_profiles);
}

void SegmentationPlatformProfileObserver::OnProfileManagerDestroying() {
  profile_manager_ = nullptr;
}

void SegmentationPlatformProfileObserver::OnOffTheRecordProfileCreated(
    Profile* profile) {
  OnProfileAdded(profile);
}

void SegmentationPlatformProfileObserver::OnProfileWillBeDestroyed(
    Profile* profile) {
  observed_profiles_.RemoveObservation(profile);
  if (!profile->IsOffTheRecord() && !profile->HasAnyOffTheRecordProfile())
    return;

  // If the profile manager is destroyed, then skip changing the recording
  // state.
  if (!profile_manager_)
    return;

  // We are destroying a profile which is an OTR profile or has an OTR profile.
  // Let's check whether we still have another OTR profile.
  bool has_otr_profiles = AnyOffTheRecordProfilesExist(
      profile_manager_->GetLoadedProfiles(), profile);

  NotifyExistenceOfOTRProfile(has_otr_profiles);
}

void SegmentationPlatformProfileObserver::NotifyExistenceOfOTRProfile(
    bool has_otr_profiles) {
  if (has_otr_profiles_.has_value() && has_otr_profiles_ == has_otr_profiles)
    return;

  has_otr_profiles_ = has_otr_profiles;
  segmentation_platform_service_->EnableMetrics(!has_otr_profiles_.value());
}

}  // namespace segmentation_platform
