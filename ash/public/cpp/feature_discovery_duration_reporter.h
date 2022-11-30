// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_FEATURE_DISCOVERY_DURATION_REPORTER_H_
#define ASH_PUBLIC_CPP_FEATURE_DISCOVERY_DURATION_REPORTER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"

namespace ash {

namespace feature_discovery {
enum class TrackableFeature;
}  // namespace feature_discovery

// A singleton class that records feature discovery duration when the following
// conditions are met:
// 1. the current session state is active; and
// 2. the current profile is the primary one; and
// 3. the target feature's discovery duration has never been recorded for the
// current user on the current device.
//
// Only the discovery duration under an active session is counted. Also, the
// time duration when the device is suspended is not counted into the feature
// discovery time. The time spent in a different account from the one where the
// feature usage observation starts is excluded from the total duration. For
// example, if the user:
// 1. spends A time in the primary user session,
// 2. switches to a secondary user session for B time,
// 3. switches back to the primary user session,
// 4. discovers the new feature after C time.
// In this case, the feature discovery time duration should be A + C.
class ASH_PUBLIC_EXPORT FeatureDiscoveryDurationReporter {
 public:
  class ReporterObserver : public base::CheckedObserver {
   public:
    // Called when the reporter is ready to use.
    virtual void OnReporterActivated() = 0;
  };

  FeatureDiscoveryDurationReporter();
  FeatureDiscoveryDurationReporter(const FeatureDiscoveryDurationReporter&) =
      delete;
  FeatureDiscoveryDurationReporter& operator=(
      const FeatureDiscoveryDurationReporter&) = delete;
  virtual ~FeatureDiscoveryDurationReporter();

  static FeatureDiscoveryDurationReporter* GetInstance();

  // Activates the observation if the restrictions, which are introduced in the
  // class comment, are met.
  virtual void MaybeActivateObservation(
      feature_discovery::TrackableFeature feature) = 0;

  // Finishes the observation on the specified feature only when the observation
  // on this feature is in progress.
  virtual void MaybeFinishObservation(
      feature_discovery::TrackableFeature feature) = 0;

  virtual void AddObserver(ReporterObserver* observer) = 0;
  virtual void RemoveObserver(ReporterObserver* observer) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_FEATURE_DISCOVERY_DURATION_REPORTER_H_
