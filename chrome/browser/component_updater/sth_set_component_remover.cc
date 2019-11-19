// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/sth_set_component_remover.h"

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/version.h"

namespace component_updater {

namespace {

void CleanupOnWorker(const base::FilePath& sth_directory) {
  const base::FilePath base_dir = sth_directory.DirName();
  base::FileEnumerator file_enumerator(base_dir, false,
                                       base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    base::Version version(path.BaseName().MaybeAsASCII());

    // Ignore folders that don't have valid version names. These folders are
    // not managed by the component installer, so don't try to remove them.
    if (!version.IsValid())
      continue;

    if (!base::DeleteFile(path, true)) {
      DLOG(ERROR) << "Couldn't delete " << path.value();
    }
  }

  if (base::IsDirectoryEmpty(base_dir)) {
    if (!base::DeleteFile(base_dir, false)) {
      DLOG(ERROR) << "Couldn't delete " << base_dir.value();
    }
  }
}

}  // namespace

void DeleteLegacySTHSet(const base::FilePath& user_data_dir) {
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&CleanupOnWorker, user_data_dir.Append(FILE_PATH_LITERAL(
                                           "CertificateTransparency"))));
}

}  // namespace component_updater
