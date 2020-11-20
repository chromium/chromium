// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_ARC_DATA_SNAPSHOTD_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_ARC_DATA_SNAPSHOTD_DELEGATE_H_

#include "base/callback.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager_observer.h"
#include "components/arc/enterprise/arc_data_snapshotd_manager.h"
#include "components/arc/session/arc_stop_reason.h"

class PrefService;

namespace arc {

class ArcSessionManager;

namespace data_snapshotd {

// This class performs delegated actions from ArcDataSnapshotdManager.
class ArcDataSnapshotdDelegate : public ArcDataSnapshotdManager::Delegate,
                                 public arc::ArcSessionManagerObserver {
 public:
  ArcDataSnapshotdDelegate();
  ArcDataSnapshotdDelegate(const ArcDataSnapshotdDelegate&) = delete;
  ArcDataSnapshotdDelegate& operator=(const ArcDataSnapshotdDelegate&) = delete;
  ~ArcDataSnapshotdDelegate() override;

  // ArcDataSnapshotdManager::Delegate overrides:
  void RequestStopArcInstance(
      base::OnceCallback<void(bool)> stopped_callback) override;
  PrefService* GetProfilePrefService() override;

  // arc::ArcSessionManagerObserver overrides:
  void OnArcSessionStopped(arc::ArcStopReason reason) override;

 private:
  // Notifies via |arc_stopped_callback_| that ARC is successfully stopped
  // starting from ACTIVE state. Otherwise returns false.
  // |arc_stopped_callback_| should never be null when calling this method.
  void NotifyArcStopped(bool success);

  // Not owned. Owned by ArcServiceLauncher. Should never be nullptr.
  arc::ArcSessionManager* const arc_session_manager_;
  base::OnceCallback<void(bool)> arc_stopped_callback_;
};

}  // namespace data_snapshotd
}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ENTERPRISE_ARC_DATA_SNAPSHOTD_DELEGATE_H_
