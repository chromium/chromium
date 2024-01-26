// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_PROFILE_OBSERVER_H_
#define CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_PROFILE_OBSERVER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/supports_user_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"

class ProfileManager;

namespace segmentation_platform {
class SegmentationPlatformService;

// This class tracks whether or not there is an off the record profile. It
// observes profile creation and destruction events, and based on that notifies
// the segmentation platform to pause/resume signal collection. The signal
// collection is disabled whenever there is at least one off the record profile.
class SegmentationPlatformProfileObserver : public base::SupportsUserData::Data,
                                            public ProfileManagerObserver,
                                            public ProfileObserver {
 public:
  SegmentationPlatformProfileObserver(
      SegmentationPlatformService* segmentation_platform_service,
      ProfileManager* profile_manager);
  ~SegmentationPlatformProfileObserver() override;

  // Disallow copy/assign.
  SegmentationPlatformProfileObserver(
      const SegmentationPlatformProfileObserver&) = delete;
  SegmentationPlatformProfileObserver& operator=(
      const SegmentationPlatformProfileObserver&) = delete;

  // ProfileManagerObserver overrides.
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // ProfileObserver overrides.
  void OnOffTheRecordProfileCreated(Profile* profile) override;
  void OnProfileWillBeDestroyed(Profile* profile) override;

 private:
  void NotifyExistenceOfOTRProfile(bool has_otr_profiles);

  raw_ptr<SegmentationPlatformService> segmentation_platform_service_;
  raw_ptr<ProfileManager> profile_manager_;

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      observed_profiles_{this};

  // Whether or not we currently have any off the record profiles.
  std::optional<bool> has_otr_profiles_;
};

}  // namespace segmentation_platform

#endif  // CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_PROFILE_OBSERVER_H_
