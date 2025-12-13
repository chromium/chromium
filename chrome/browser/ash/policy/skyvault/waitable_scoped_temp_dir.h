// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_WAITABLE_SCOPED_TEMP_DIR_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_WAITABLE_SCOPED_TEMP_DIR_H_

#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

// Wraps temporary directory creation in TMPDIR before it can be uploaded to
// OneDrive.
class WaitableScopedTempDir {
 public:
  WaitableScopedTempDir();
  ~WaitableScopedTempDir();

  // If the temporary directory is already created, `callback` is invoked
  // immediately. Otherwise, it is invoked once the directory is ready.
  void WaitForPath(base::OnceClosure callback);

  base::FilePath path() const;

 private:
  // Called back once temporary directory for OneDrive is created.
  void OnTempDirCreated(base::ScopedTempDir temp_dir);

  // Temporary directory to which files will be redirected before being uploaded
  // to OneDrive cloud. Created and destructed asynchronously.
  base::ScopedTempDir odfs_temp_dir_;
  // Track directory creation in a boolean as calling ScopedTempDir's IsValid()
  // requires a blocking scope and GetPath() cannot be called before
  // CreateUniqueTempDir().
  bool created_ = false;
  base::queue<base::OnceClosure> on_path_ready_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<WaitableScopedTempDir> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_WAITABLE_SCOPED_TEMP_DIR_H_
