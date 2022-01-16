// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_updater_utils.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/version.h"
#include "build/build_config.h"
#if BUILDFLAG(IS_WIN)
#include "chrome/installer/util/install_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace component_updater {

void DeleteFilesAndParentDirectory(const base::FilePath& file_path) {
  const base::FilePath base_dir = file_path.DirName();
  base::FileEnumerator file_enumerator(base_dir, false,
                                       base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    base::Version version(path.BaseName().MaybeAsASCII());

    // Ignore folders that don't have valid version names. These folders are
    // not managed by the component installer, so don't try to remove them.
    if (!version.IsValid())
      continue;

    if (!base::DeletePathRecursively(path)) {
      DLOG(ERROR) << "Couldn't delete " << path.value();
    }
  }

  if (base::IsDirectoryEmpty(base_dir)) {
    if (!base::DeleteFile(base_dir)) {
      DLOG(ERROR) << "Couldn't delete " << base_dir.value();
    }
  }
}

bool IsPerUserInstall() {
#if BUILDFLAG(IS_WIN)
  // The installer computes and caches this value in memory during the
  // process start up.
  return InstallUtil::IsPerUserInstall();
#else
  return true;
#endif
}

}  // namespace component_updater
