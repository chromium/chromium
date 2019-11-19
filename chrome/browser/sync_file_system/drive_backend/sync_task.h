// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_TASK_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_TASK_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"

namespace sync_file_system {
namespace drive_backend {

class SyncTaskToken;

class SyncTask {
 public:
  SyncTask() : used_network_(false) {}
  virtual ~SyncTask() {}
  virtual void RunPreflight(std::unique_ptr<SyncTaskToken> token) = 0;

  bool used_network() { return used_network_; }

 protected:
  void set_used_network(bool used_network) {
    used_network_ = used_network;
  }

 private:
  bool used_network_;

  DISALLOW_COPY_AND_ASSIGN(SyncTask);
};

class ExclusiveTask : public SyncTask {
 public:
  ExclusiveTask();
  ~ExclusiveTask() override;

  void RunPreflight(std::unique_ptr<SyncTaskToken> token) final;
  virtual void RunExclusive(const SyncStatusCallback& callback) = 0;

 private:
  base::WeakPtrFactory<ExclusiveTask> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExclusiveTask);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_TASK_H_
