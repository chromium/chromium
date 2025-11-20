// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/waitable_scoped_temp_dir.h"

#include "base/task/thread_pool.h"

namespace {

base::ScopedTempDir CreateTempDir() {
  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());
  return temp_dir;
}

}  // namespace

WaitableScopedTempDir::WaitableScopedTempDir() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&CreateTempDir),
      base::BindOnce(&WaitableScopedTempDir::OnTempDirCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

WaitableScopedTempDir::~WaitableScopedTempDir() {
  base::ThreadPool::PostTask(FROM_HERE,
                             {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                              base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
                             base::BindOnce(
                                 [](base::ScopedTempDir) {
                                   // No-op other than running
                                   // the base::ScopedTempDir
                                   // destructor.
                                 },
                                 std::move(odfs_temp_dir_)));
}

void WaitableScopedTempDir::WaitForPath(base::OnceClosure callback) {
  if (created_) {
    std::move(callback).Run();
  } else {
    on_path_ready_callbacks_.push(std::move(callback));
  }
}

base::FilePath WaitableScopedTempDir::path() const {
  CHECK(created_);
  return odfs_temp_dir_.GetPath();
}

void WaitableScopedTempDir::OnTempDirCreated(base::ScopedTempDir temp_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  odfs_temp_dir_ = std::move(temp_dir);
  created_ = true;
  while (!on_path_ready_callbacks_.empty()) {
    std::move(on_path_ready_callbacks_.front()).Run();
    on_path_ready_callbacks_.pop();
  }
}
