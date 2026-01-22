// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/tpcd_metadata_component_remover.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"

namespace component_updater {

namespace {

void DeleteTPCDMetadataComponentFromDisk(const base::FilePath& user_data_dir) {
  base::DeletePathRecursively(
      user_data_dir.Append(kTpcdMetadataComponentFileName));
}

}  // namespace

void DeleteTPCDMetadataComponent(const base::FilePath& user_data_dir) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&DeleteTPCDMetadataComponentFromDisk, user_data_dir));
}

}  // namespace component_updater
