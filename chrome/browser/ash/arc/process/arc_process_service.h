// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_PROCESS_ARC_PROCESS_SERVICE_H_
#define CHROME_BROWSER_ASH_ARC_PROCESS_ARC_PROCESS_SERVICE_H_

#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <vector>

#include "ash/components/arc/mojom/process.mojom-forward.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_iterator.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/process/arc_process.h"
#include "chromeos/ash/components/process_snapshot/process_snapshot_server.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// A single global entry to get a list of ARC processes.
//
// Call RequestAppProcessList() / RequestSystemProcessList() on the main UI
// thread to get a list of all ARC app / system processes. It returns
// std::optional<vector<arc::ArcProcess>>, which includes pid <-> nspid
// mapping. Example:
//   void OnUpdateProcessList(
//       std::optional<vector<arc::ArcProcess>> processes) {
//     if (!processes) {
//         // Arc process service is not ready.
//        return;
//     }
//     ...
//   }
//
//   arc::ArcProcessService* arc_process_service =
//       arc::ArcProcessService::Get();
//   if (!arc_process_service)
//     LOG(ERROR) << "ARC process instance not ready.";
//     return;
//   }
//   arc_process_service->RequestAppProcessList(
//       base::BindOnce(&OnUpdateProcessList));
//
// [System Process]
// The system process here is defined by the scope. If the process is produced
// under system_server in Android, we regard it as one of Android app process.
// Otherwise, the processes that are introduced by init would then be regarded
// as System Process. RequestAppProcessList() is responsible for app processes
// while RequestSystemProcessList() is responsible for System Processes.
class ArcProcessService : public KeyedService,
                          public ConnectionObserver<mojom::ProcessInstance>,
                          public ash::ProcessSnapshotServer::Observer {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcProcessService* GetForBrowserContext(
      content::BrowserContext* context);

  using OptionalArcProcessList = std::optional<std::vector<ArcProcess>>;
  using RequestProcessListCallback =
      base::OnceCallback<void(OptionalArcProcessList)>;
  using RequestMemoryInfoCallback =
      base::OnceCallback<void(std::vector<mojom::ArcMemoryDumpPtr>)>;

  ArcProcessService(content::BrowserContext* context,
                    ArcBridgeService* bridge_service);

  ArcProcessService(const ArcProcessService&) = delete;
  ArcProcessService& operator=(const ArcProcessService&) = delete;

  ~ArcProcessService() override;

  // TODO(afakhry): The value of this delay was chosen to match the refresh time
  // of crostini and vm tasks in the task manager (See VmProcessTaskProvider).
  // Check if we need to pick a different value.
  // Also consider making ArcProcessService a push service rather than a pull
  // service.
  static constexpr base::TimeDelta kProcessSnapshotRefreshTime =
      base::Seconds(5);

  // Returns nullptr before the global instance is ready.
  static ArcProcessService* Get();

  // If ARC IPC is ready for the process list request, the result is returned
  // as the argument of |callback|. Otherwise, |callback| is called with
  // std::nullopt.
  // The process list maybe stale of up to |kProcessSnapshotRefreshTime|.
  void RequestAppProcessList(RequestProcessListCallback callback);
  void RequestSystemProcessList(RequestProcessListCallback callback);

  // An empty result will be returned in the argument of |callback| if ARC IPC
  // is not ready.
  void RequestAppMemoryInfo(RequestMemoryInfoCallback callback);
  void RequestSystemMemoryInfo(RequestMemoryInfoCallback callback);

  // ProcessSnapshotServer::Observer:
  void OnProcessSnapshotRefreshed(
      const base::ProcessIterator::ProcessEntries& snapshot) override;

  using PidMap = std::map<base::ProcessId, base::ProcessId>;

  class NSPidToPidMap : public base::RefCountedThreadSafe<NSPidToPidMap> {
   public:
    NSPidToPidMap();

    NSPidToPidMap(const NSPidToPidMap&) = delete;
    NSPidToPidMap& operator=(const NSPidToPidMap&) = delete;

    base::ProcessId& operator[](const base::ProcessId& key) {
      return pidmap_[key];
    }
    const base::ProcessId& at(const base::ProcessId& key) const {
      return pidmap_.at(key);
    }
    PidMap::size_type erase(const base::ProcessId& key) {
      return pidmap_.erase(key);
    }
    PidMap::const_iterator begin() const { return pidmap_.begin(); }
    PidMap::const_iterator end() const { return pidmap_.end(); }
    PidMap::const_iterator find(const base::ProcessId& key) const {
      return pidmap_.find(key);
    }
    void clear() { pidmap_.clear(); }

   private:
    friend base::RefCountedThreadSafe<NSPidToPidMap>;
    ~NSPidToPidMap();

    PidMap pidmap_;
  };

  static void EnsureFactoryBuilt();

 private:
  void OnReceiveProcessList(
      RequestProcessListCallback callback,
      std::vector<mojom::RunningAppProcessInfoPtr> processes);
  void OnReceiveMemoryInfo(RequestMemoryInfoCallback callback,
                           std::vector<mojom::ArcMemoryDumpPtr> process_dumps);
  void OnGetSystemProcessList(RequestMemoryInfoCallback callback,
                              std::vector<ArcProcess> processes);
  // ConnectionObserver<mojom::ProcessInstance> overrides.
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // Returns true if |cached_process_snapshot_| is recent enough.
  bool CanUseStaleProcessSnapshot() const;

  // Called when a request is handled to stop observing the
  // ProcessSnapshotServer if possible.
  void MaybeStopObservingProcessSnapshots();

  // Handles the given |request|, either immediately if
  // |cached_process_snapshot_| is recent enough, or defers it until a new
  // updated process snapshot is received.
  void HandleRequest(base::OnceClosure request);

  // The actual handlers of RequestAppProcessList() and
  // RequestSystemProcessList() calls.
  void ContinueAppProcessListRequest(RequestProcessListCallback callback);
  void ContinueSystemProcessListRequest(RequestProcessListCallback callback);

  // The actual handlers of RequestAppMemoryInfo() and
  // RequestSystemMemoryInfo() calls.
  void ContinueAppMemoryInfoRequest(RequestMemoryInfoCallback callback);
  void ContinueSystemMemoryInfoRequest(RequestMemoryInfoCallback callback);

  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  // The most recent process snapshot received from the ProcessSnapshotServer.
  base::ProcessIterator::ProcessEntries cached_process_snapshot_;

  // The time at which the current |cached_process_snapshot_| was received.
  base::Time last_process_snapshot_time_;

  // Whether ARC is ready to request its process list.
  bool connection_ready_ = false;

  // True if the ProcessSnapshotServer is currently being observed.
  bool is_observing_process_snapshot_ = false;

  // A FIFO queue of pending requests that were received before getting a recent
  // enough process snapshot.
  std::queue<base::OnceClosure> pending_requests_;

  // There are some expensive tasks such as traverse whole process tree that
  // we can't do it on the UI thread.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Keep a cache pid mapping of all arc processes so to minimize the number of
  // nspid lookup from /proc/<PID>/status.
  // To play safe, always modify |nspid_to_pid_| on the blocking pool.
  scoped_refptr<NSPidToPidMap> nspid_to_pid_;

  // Always keep this the last member of this class to make sure it's the
  // first thing to be destructed.
  base::WeakPtrFactory<ArcProcessService> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_PROCESS_ARC_PROCESS_SERVICE_H_
