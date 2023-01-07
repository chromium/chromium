// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_ARC_ARC_PROCESS_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_ARC_ARC_PROCESS_TASK_H_

#include <memory>
#include <string>

#include "ash/components/arc/mojom/intent_helper.mojom-forward.h"
#include "ash/components/arc/mojom/process.mojom-forward.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/arc/process/arc_process.h"
#include "chrome/browser/task_manager/providers/task.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

namespace task_manager {

// Defines a task that represents an ARC process.
class ArcProcessTask
    : public Task,
      public arc::ConnectionObserver<arc::mojom::IntentHelperInstance> {
 public:
  explicit ArcProcessTask(arc::ArcProcess arc_process);
  ArcProcessTask(const ArcProcessTask&) = delete;
  ArcProcessTask& operator=(const ArcProcessTask&) = delete;
  ~ArcProcessTask() override;

  // task_manager::Task:
  Type GetType() const override;
  int GetChildProcessUniqueID() const override;
  bool IsKillable() override;
  void Kill() override;
  bool IsRunningInVM() const override;

  // arc::ConnectionObserver<arc::mojom::IntentHelperInstance>:
  void OnConnectionReady() override;

  void SetProcessState(arc::mojom::ProcessState process_state);

  base::ProcessId nspid() const { return arc_process_.nspid(); }
  const std::string& process_name() const {
    return arc_process_.process_name();
  }

 private:
  void StartIconLoading();
  void OnIconLoaded(
      std::unique_ptr<arc::ArcIntentHelperBridge::ActivityToIconsMap> icons);

  arc::ArcProcess arc_process_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ArcProcessTask> weak_ptr_factory_{this};
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_ARC_ARC_PROCESS_TASK_H_
