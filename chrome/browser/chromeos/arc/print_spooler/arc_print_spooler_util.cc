// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/print_spooler/arc_print_spooler_util.h"

#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/files/scoped_file.h"
#include "mojo/public/c/system/types.h"

namespace arc {

void DeletePrintDocument(const base::FilePath& file_path) {
  if (!base::DeleteFile(file_path, false))
    LOG(ERROR) << "Failed to delete print document.";
}

base::FilePath SavePrintDocument(mojo::ScopedHandle scoped_handle) {
  base::PlatformFile platform_file = base::kInvalidPlatformFile;
  if (mojo::UnwrapPlatformFile(std::move(scoped_handle), &platform_file) !=
      MOJO_RESULT_OK) {
    PLOG(ERROR) << "UnwrapPlatformFile failed.";
    return base::FilePath();
  }

  base::File src_file(platform_file);
  if (!src_file.IsValid()) {
    PLOG(ERROR) << "Source file is invalid.";
    return base::FilePath();
  }

  // TODO(jschettler): Determine a more secure location to save the print
  // document.
  base::FilePath temp_path;
  if (!base::CreateTemporaryFile(&temp_path)) {
    PLOG(ERROR) << "Failed to create file.";
    return base::FilePath();
  }

  base::File temp(temp_path, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  char buf[4096];
  int bytes;
  while ((bytes = src_file.ReadAtCurrentPos(buf, sizeof(buf))) > 0) {
    if (!temp.WriteAtCurrentPosAndCheck(
            base::as_bytes(base::make_span(buf, bytes)))) {
      PLOG(ERROR) << "Error while saving PDF to disk.";
      return base::FilePath();
    }
  }

  if (bytes < 0) {
    PLOG(ERROR) << "Error reading PDF.";
    return base::FilePath();
  }

  return temp_path;
}

}  // namespace arc
