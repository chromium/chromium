// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_ENTERPRISE_ARC_DATA_SNAPSHOTD_BRIDGE_H_
#define ASH_COMPONENTS_ARC_ENTERPRISE_ARC_DATA_SNAPSHOTD_BRIDGE_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace arc {
namespace data_snapshotd {

// This class is responsible for bootstrapping D-Bus communication with
// arc-data-snapshotd daemon and delegating all ARC data/ snapshot related
// operations to it.
class ArcDataSnapshotdBridge {
 public:
  explicit ArcDataSnapshotdBridge(
      base::OnceClosure on_bridge_available_callback);
  ArcDataSnapshotdBridge(const ArcDataSnapshotdBridge&) = delete;
  ArcDataSnapshotdBridge& operator=(const ArcDataSnapshotdBridge&) = delete;
  ~ArcDataSnapshotdBridge();

  static base::TimeDelta connection_attempt_interval_for_testing();
  static int max_connection_attempt_count_for_testing();

  // Delegates the key pair generation to arc-data-snapshotd daemon.
  void GenerateKeyPair(base::OnceCallback<void(bool)> callback);
  // Delegates the removal of snapshot to arc-data-snapshotd daemon. If |last|,
  // removes the last generated snapshot.
  void ClearSnapshot(bool last, base::OnceCallback<void(bool)> callback);
  // Delegates the taking of ARC data snapshot to arc-data-snapshotd daemon.
  // |account_id| is the current account_id of MGS.
  void TakeSnapshot(const std::string& account_id,
                    base::OnceCallback<void(bool)> callback);
  // Delegates the loading of ARC data snapshot to current MGS to
  // arc-data-snapshotd daemon. |account_id| is the current account_id of MGS.
  void LoadSnapshot(const std::string& account_id,
                    base::OnceCallback<void(bool, bool)> callback);
  // Delegates the updating of a progress bar on a UI screen to
  // arc-data-snapshotd daemon. |percent| is a percentage of installed required
  // ARC apps [0..100].
  void Update(int percent, base::OnceCallback<void(bool)> callback);

  // Connects to UiCancelled signal of arc-data-snapshotd D-Bus interface.
  // |signal_callback| is called when a signal is received.
  void ConnectToUiCancelledSignal(base::RepeatingClosure signal_callback);

  bool is_available_for_testing() { return is_available_; }

 private:
  // Starts waiting until the arc-data-snapshotd D-Bus service becomes available
  // (or until this waiting fails).
  void WaitForDBusService();
  // Schedules a postponed execution of WaitForDBusService().
  void ScheduleWaitingForDBusService();
  // Called once waiting for the D-Bus service, started by WaitForDBusService(),
  // finishes.
  void OnWaitedForDBusService(bool service_is_available);

  // Called once the object proxy is connected to UiCancelled signal or failed
  // to be connected.
  void OnUiCancelledSignalConnectedCallback(bool success);

  // Callback passed in constructor and called once the D-Bus bridge is set up
  // or the number of max attempts exceeded.
  base::OnceClosure on_bridge_available_callback_;

  // Current consecutive connection attempt number.
  int connection_attempt_ = 0;
  // True if D-Bus service is available.
  bool is_available_ = false;

  // Used for cancelling previously posted tasks that wait for the D-Bus service
  // availability.
  base::WeakPtrFactory<ArcDataSnapshotdBridge> dbus_waiting_weak_ptr_factory_{
      this};
  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ArcDataSnapshotdBridge> weak_ptr_factory_{this};
};

}  // namespace data_snapshotd
}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_ENTERPRISE_ARC_DATA_SNAPSHOTD_BRIDGE_H_
