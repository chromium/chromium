// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_MANAGER_IMPL_H_

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/borealis/borealis_context.h"
#include "chrome/browser/chromeos/borealis/borealis_context_manager.h"
#include "chrome/browser/profiles/profile.h"

namespace borealis {

class BorealisTask;

// The Borealis Context Manager is a keyed service responsible for managing
// the Borealis VM startup flow and guaranteeing its state to other processes.
class BorealisContextManagerImpl : public BorealisContextManager {
 public:
  explicit BorealisContextManagerImpl(Profile* profile);
  BorealisContextManagerImpl(const BorealisContextManagerImpl&) = delete;
  BorealisContextManagerImpl& operator=(const BorealisContextManagerImpl&) =
      delete;
  ~BorealisContextManagerImpl() override;

  // BorealisContextManager:
  void StartBorealis(ResultCallback callback) override;

  // Public due to testing.
  virtual base::queue<std::unique_ptr<BorealisTask>> GetTasks();

 private:
  void AddCallback(ResultCallback callback);
  void NextTask();
  void TaskCallback(Status status, std::string error);
  void OnQueueComplete();

  // Returns the result of the startup (i.e. the context if it succeeds, or an
  // error if it doesn't).
  BorealisContextManager::Result GetResult();

  bool is_borealis_running_ = false;
  bool is_borealis_starting_ = false;

  Profile* profile_ = nullptr;
  BorealisContextManager::Status startup_status_ = kSuccess;
  std::string startup_error_;
  BorealisContext context_;
  base::queue<ResultCallback> callback_queue_;
  base::queue<std::unique_ptr<BorealisTask>> task_queue_;
  std::unique_ptr<BorealisTask> current_task_;

  base::WeakPtrFactory<BorealisContextManagerImpl> weak_factory_{this};
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_MANAGER_IMPL_H_
