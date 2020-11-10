// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_MANAGER_IMPL_H_

#include <memory>

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
  void ShutDownBorealis() override;

  // Public due to testing.
  virtual base::queue<std::unique_ptr<BorealisTask>> GetTasks();

 private:
  void AddCallback(ResultCallback callback);
  void NextTask();
  void TaskCallback(BorealisStartupResult result, std::string error);

  // Completes the startup with the given |result| and error messgae, invoking
  // all callbacks with the result. For any result except kSuccess the state of
  // the system will be as though StartBorealis() had not been called.
  void Complete(BorealisStartupResult result, std::string error_or_empty);

  // Returns the result of the startup (i.e. the context if it succeeds, or an
  // error if it doesn't).
  BorealisContextManager::Result GetResult();

  Profile* profile_ = nullptr;
  BorealisStartupResult startup_result_ = BorealisStartupResult::kSuccess;
  base::TimeTicks startup_start_tick_;
  std::string startup_error_;
  std::unique_ptr<BorealisContext> context_;
  base::queue<ResultCallback> callback_queue_;
  base::queue<std::unique_ptr<BorealisTask>> task_queue_;

  base::WeakPtrFactory<BorealisContextManagerImpl> weak_factory_{this};
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_MANAGER_IMPL_H_
