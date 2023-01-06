// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_ENTERPRISE_ARC_APPS_TRACKER_H_
#define ASH_COMPONENTS_ARC_ENTERPRISE_ARC_APPS_TRACKER_H_

#include "base/functional/callback_forward.h"

namespace arc {
namespace data_snapshotd {

// This class tracks installation of the list of ARC apps.
class ArcAppsTracker {
 public:
  // The destruction of the instance stops tracking installation of the list of
  // ARC apps and compliance with policy.
  virtual ~ArcAppsTracker() = default;

  // Starts tracking installation of the list of ARC apps.
  // Invokes a repeating callback |update_callback| when the number of installed
  // tracked apps changes. |update_callback| is invoked with the percent of
  // installed tracked apps in a range of 0..100.
  // Invokes a |finish_callback| once ARC is compliant with policy and the
  // apps tracking is finished.
  // |update_callback| is guaranteed to be never invoked after
  // |finish_callback|.
  virtual void StartTracking(base::RepeatingCallback<void(int)> update_callback,
                             base::OnceClosure finish_callback) = 0;
};

}  // namespace data_snapshotd
}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_ENTERPRISE_ARC_APPS_TRACKER_H_
