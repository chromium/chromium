// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_FREEZER_CGROUP_PROCESS_MANAGER_H_
#define CHROME_BROWSER_ASH_POWER_FREEZER_CGROUP_PROCESS_MANAGER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "chrome/browser/ash/power/renderer_freezer.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace ash {

// Manages all the processes in the freezer cgroup on Chrome OS.
class FreezerCgroupProcessManager : public RendererFreezer::Delegate {
 public:
  FreezerCgroupProcessManager();

  FreezerCgroupProcessManager(const FreezerCgroupProcessManager&) = delete;
  FreezerCgroupProcessManager& operator=(const FreezerCgroupProcessManager&) =
      delete;

  ~FreezerCgroupProcessManager() override;

  // RendererFreezer::Delegate overrides.
  void SetShouldFreezeRenderer(base::ProcessHandle handle,
                               bool frozen) override;
  void FreezeRenderers() override;
  void ThawRenderers(ResultCallback callback) override;
  void CheckCanFreezeRenderers(ResultCallback callback) override;

 private:
  scoped_refptr<base::SequencedTaskRunner> file_thread_;

  class FileWorker;
  std::unique_ptr<FileWorker> file_worker_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_FREEZER_CGROUP_PROCESS_MANAGER_H_
