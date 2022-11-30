// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_TASK_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_TASK_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"

namespace sync_file_system {
namespace drive_backend {

class SyncTaskToken;

class SyncTask {
 public:
  SyncTask() : used_network_(false) {}

  SyncTask(const SyncTask&) = delete;
  SyncTask& operator=(const SyncTask&) = delete;

  virtual ~SyncTask() {}
  virtual void RunPreflight(std::unique_ptr<SyncTaskToken> token) = 0;

  bool used_network() { return used_network_; }

 protected:
  void set_used_network(bool used_network) {
    used_network_ = used_network;
  }

 private:
  bool used_network_;
};

class ExclusiveTask : public SyncTask {
 public:
  ExclusiveTask();

  ExclusiveTask(const ExclusiveTask&) = delete;
  ExclusiveTask& operator=(const ExclusiveTask&) = delete;

  ~ExclusiveTask() override;

  void RunPreflight(std::unique_ptr<SyncTaskToken> token) final;
  virtual void RunExclusive(SyncStatusCallback callback) = 0;

 private:
  base::WeakPtrFactory<ExclusiveTask> weak_ptr_factory_{this};
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_TASK_H_
