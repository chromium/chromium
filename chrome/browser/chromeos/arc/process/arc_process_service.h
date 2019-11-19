// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_PROCESS_ARC_PROCESS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_PROCESS_ARC_PROCESS_SERVICE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/process/process_iterator.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/arc/process/arc_process.h"
#include "components/arc/mojom/process.mojom.h"
#include "components/arc/session/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// A single global entry to get a list of ARC processes.
//
// Call RequestAppProcessList() / RequestSystemProcessList() on the main UI
// thread to get a list of all ARC app / system processes. It returns
// base::Optional<vector<arc::ArcProcess>>, which includes pid <-> nspid
// mapping. Example:
//   void OnUpdateProcessList(
//       base::Optional<vector<arc::ArcProcess>> processes) {
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
                          public ConnectionObserver<mojom::ProcessInstance> {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcProcessService* GetForBrowserContext(
      content::BrowserContext* context);

  using OptionalArcProcessList = base::Optional<std::vector<ArcProcess>>;
  using RequestProcessListCallback =
      base::OnceCallback<void(OptionalArcProcessList)>;
  using RequestMemoryInfoCallback =
      base::OnceCallback<void(std::vector<mojom::ArcMemoryDumpPtr>)>;

  ArcProcessService(content::BrowserContext* context,
                    ArcBridgeService* bridge_service);
  ~ArcProcessService() override;

  // Returns nullptr before the global instance is ready.
  static ArcProcessService* Get();

  // If ARC IPC is ready for the process list request, the result is returned
  // as the argument of |callback|. Otherwise, |callback| is called with
  // base::nullopt.
  void RequestAppProcessList(RequestProcessListCallback callback);
  void RequestSystemProcessList(RequestProcessListCallback callback);

  bool RequestAppMemoryInfo(RequestMemoryInfoCallback callback);
  bool RequestSystemMemoryInfo(RequestMemoryInfoCallback callback);

  using PidMap = std::map<base::ProcessId, base::ProcessId>;

  class NSPidToPidMap : public base::RefCountedThreadSafe<NSPidToPidMap> {
   public:
    NSPidToPidMap();
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
    DISALLOW_COPY_AND_ASSIGN(NSPidToPidMap);
  };

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

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  // Whether ARC is ready to request its process list.
  bool connection_ready_ = false;

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

  DISALLOW_COPY_AND_ASSIGN(ArcProcessService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_PROCESS_ARC_PROCESS_SERVICE_H_
