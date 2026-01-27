// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_CHROME_PROFILE_DOWNLOAD_SERVICE_TRACKER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_CHROME_PROFILE_DOWNLOAD_SERVICE_TRACKER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/optimization_guide/core/delivery/profile_download_service_tracker.h"

class ProfileManager;

namespace optimization_guide {

class ChromeProfileDownloadServiceTracker
    : public ProfileDownloadServiceTracker,
      public ProfileManagerObserver,
      public ProfileObserver {
 public:
  ChromeProfileDownloadServiceTracker();
  ~ChromeProfileDownloadServiceTracker() override;

  ChromeProfileDownloadServiceTracker(
      const ChromeProfileDownloadServiceTracker&) = delete;
  ChromeProfileDownloadServiceTracker& operator=(
      const ChromeProfileDownloadServiceTracker&) = delete;

  // ProfileDownloadServiceTracker:
  download::BackgroundDownloadService* GetBackgroundDownloadService() override;

 private:
  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  // Observed Profile instances. Cannot use base::ScopedMultiSourceObservation
  // as ChromeProfileDownloadServiceTracker depends on the source ordering but
  // base::ScopedMultiSourceObservation does not.
  std::vector<raw_ptr<Profile>> observed_profiles_;
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_CHROME_PROFILE_DOWNLOAD_SERVICE_TRACKER_H_
