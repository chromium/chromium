// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_ENTERPRISE_SNAPSHOT_SESSION_CONTROLLER_H_
#define ASH_COMPONENTS_ARC_ENTERPRISE_SNAPSHOT_SESSION_CONTROLLER_H_

#include "ash/components/arc/enterprise/arc_apps_tracker.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/timer/timer.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace base {

class OneShotTimer;

}  // namespace base

namespace arc {
namespace data_snapshotd {

// This class observes the MGS state changes and duration.
class SnapshotSessionController {
 public:
  // Observer interface.
  class Observer : public base::CheckedObserver {
   public:
    // Called once MGS is started.
    virtual void OnSnapshotSessionStarted() = 0;
    // Called once MGS is finished with all required apps installed.
    virtual void OnSnapshotSessionStopped() = 0;
    // Called once MGS is finished with not all required apps installed.
    virtual void OnSnapshotSessionFailed() = 0;
    // Called once any required ARC app is installed.
    // |percent| is the number of percent of installed apps among the required
    // ARC apps in range [0..100].
    virtual void OnSnapshotAppInstalled(int percent) = 0;
    // Called once the snapshot session is compliant with ARC policy.
    virtual void OnSnapshotSessionPolicyCompliant() = 0;
  };

  virtual ~SnapshotSessionController();

  static std::unique_ptr<SnapshotSessionController> Create(
      std::unique_ptr<ArcAppsTracker> apps_tracker);

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual const base::OneShotTimer* get_timer_for_testing() const;
};

}  // namespace data_snapshotd
}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_ENTERPRISE_SNAPSHOT_SESSION_CONTROLLER_H_
