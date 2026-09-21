// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/child_module/child_module_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/child_module/child_module_watcher.h"

namespace child_module {

ChildModuleManager::ChildModuleManager() {
  auto background_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  watcher_.emplace(std::move(background_task_runner), GetModulesDir(),
                   base::BindPostTaskToCurrentDefault(base::BindRepeating(
                       &ChildModuleManager::OnVersionSetChanged,
                       weak_factory_.GetWeakPtr())));
}

ChildModuleManager::~ChildModuleManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ChildModuleManager::WaitForInitialScanForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  watcher_.FlushPostedTasksForTesting();  // IN-TEST
}

std::optional<base::Version> ChildModuleManager::GetLatestVersion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (available_versions_.empty()) {
    return std::nullopt;
  }
  return *available_versions_.begin();
}

base::FilePath ChildModuleManager::GetRendererBinaryPath(
    const base::Version& version) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!available_versions_.contains(version)) {
    return base::FilePath();
  }
  return child_module::GetRendererBinaryPath(version);
}

const VersionSet& ChildModuleManager::GetAvailableVersions() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return available_versions_;
}

void ChildModuleManager::OnVersionSetChanged(VersionSet versions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  available_versions_ = std::move(versions);
}

}  // namespace child_module
