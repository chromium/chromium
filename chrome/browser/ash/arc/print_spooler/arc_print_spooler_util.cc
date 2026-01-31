// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/print_spooler/arc_print_spooler_util.h"

#include <utility>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "mojo/public/c/system/types.h"

namespace arc {

void DeletePrintDocument(const base::FilePath& file_path) {
  if (!base::DeleteFile(file_path))
    LOG(ERROR) << "Failed to delete print document.";
}

base::FilePath SavePrintDocument(mojo::ScopedHandle scoped_handle) {
  base::ScopedPlatformFile platform_file;
  if (mojo::UnwrapPlatformFile(std::move(scoped_handle), &platform_file) !=
      MOJO_RESULT_OK) {
    PLOG(ERROR) << "UnwrapPlatformFile failed.";
    return base::FilePath();
  }

  base::File src_file(std::move(platform_file));
  if (!src_file.IsValid()) {
    PLOG(ERROR) << "Source file is invalid.";
    return base::FilePath();
  }

  base::FilePath temp_path;
  if (!base::CreateTemporaryFile(&temp_path)) {
    PLOG(ERROR) << "Failed to create file.";
    return base::FilePath();
  }

  base::File temp(temp_path, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  char buf[4096];
  const base::span<uint8_t> buf_span = base::as_writable_byte_span(buf);
  while (true) {
    std::optional<size_t> bytes_read = src_file.ReadAtCurrentPos(buf_span);
    if (!bytes_read) {
      PLOG(ERROR) << "Error reading PDF.";
      return base::FilePath();
    }

    if (*bytes_read == 0) {
      break;
    }

    if (!temp.WriteAtCurrentPosAndCheck(buf_span.first(*bytes_read))) {
      PLOG(ERROR) << "Error while saving PDF to disk.";
      return base::FilePath();
    }
  }

  return base::MakeAbsoluteFilePath(temp_path);
}

}  // namespace arc
