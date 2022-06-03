// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/file_utils.h"

#include <fcntl.h>
#include <lib/fdio/fd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <utility>

#include "base/files/scoped_file.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/macros.h"

namespace base {

const char kPersistedDataDirectoryPath[] = "/data";
const char kPersistedCacheDirectoryPath[] = "/cache";
const char kServiceDirectoryPath[] = "/svc";
const char kPackageRootDirectoryPath[] = "/pkg";

fidl::InterfaceHandle<::fuchsia::io::Directory> OpenDirectoryHandle(
    const base::FilePath& path) {
  ScopedFD fd(open(path.value().c_str(), O_DIRECTORY | O_RDONLY));
  if (!fd.is_valid()) {
    DPLOG(ERROR) << "Failed to open " << path;
    return fidl::InterfaceHandle<::fuchsia::io::Directory>();
  }

  zx::channel channel;
  zx_status_t status =
      fdio_fd_transfer(fd.get(), channel.reset_and_get_address());
  if (status != ZX_ERR_UNAVAILABLE)
    ignore_result(fd.release());
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fdio_fd_transfer";
    return fidl::InterfaceHandle<::fuchsia::io::Directory>();
  }

  return fidl::InterfaceHandle<::fuchsia::io::Directory>(std::move(channel));
}

}  // namespace base
