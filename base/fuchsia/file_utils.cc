// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/file_utils.h"

#include <fuchsia/io/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/fdio/fd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <tuple>
#include <utility>

#include "base/files/scoped_file.h"
#include "base/fuchsia/fuchsia_logging.h"

namespace base {

fidl::InterfaceHandle<::fuchsia::io::Directory> OpenDirectoryHandle(
    const base::FilePath& path,
    DirectoryHandleRights rights) {
  fuchsia::io::OpenFlags open_flags = fuchsia::io::OpenFlags::DIRECTORY;
  if (rights.readable) {
    open_flags |= fuchsia::io::OpenFlags::RIGHT_READABLE;
  }
  if (rights.writable) {
    open_flags |= fuchsia::io::OpenFlags::RIGHT_WRITABLE;
  }
  if (rights.executable) {
    open_flags |= fuchsia::io::OpenFlags::RIGHT_EXECUTABLE;
  }
  const uint32_t flags = static_cast<uint32_t>(open_flags);
  ScopedFD fd;
  if (zx_status_t status = fdio_open_fd(path.value().c_str(), flags,
                                        base::ScopedFD::Receiver(fd).get());
      status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fdio_open_fd(" << path << ", "
                           << std::bitset<32>{flags} << ")";
    return {};
  }
  zx::channel channel;
  if (zx_status_t status =
          fdio_fd_transfer(fd.release(), channel.reset_and_get_address());
      status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fdio_fd_transfer";
    return {};
  }

  return fidl::InterfaceHandle<::fuchsia::io::Directory>(std::move(channel));
}

const char kPersistedDataDirectoryPath[] = "/data";
const char kPersistedCacheDirectoryPath[] = "/cache";
const char kServiceDirectoryPath[] = "/svc";
const char kPackageRootDirectoryPath[] = "/pkg";

}  // namespace base
