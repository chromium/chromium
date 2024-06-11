// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/native_library.h"

#include <fcntl.h>
#include <fuchsia/io/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/fdio/io.h>
#include <lib/zx/vmo.h>
#include <stdio.h>
#include <zircon/dlfcn.h>
#include <zircon/status.h>
#include <zircon/syscalls.h>

#include <string_view>

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/posix/safe_strerror.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base_paths.h"

namespace base {

std::string NativeLibraryLoadError::ToString() const {
  return message;
}

NativeLibrary LoadNativeLibraryWithOptions(const FilePath& library_path,
                                           const NativeLibraryOptions& options,
                                           NativeLibraryLoadError* error) {
  FilePath computed_path;
  FilePath library_root_path =
      base::PathService::CheckedGet(DIR_ASSETS).Append("lib");
  if (library_path.IsAbsolute()) {
    // See more info in fxbug.dev/105910.
    if (!library_root_path.IsParent(library_path)) {
      auto error_message =
          base::StringPrintf("Absolute library paths must begin with %s",
                             library_root_path.value().c_str());
      DLOG(ERROR) << error_message;
      if (error) {
        error->message = std::move(error_message);
      }
      return nullptr;
    }
    computed_path = library_path;
  } else {
    computed_path = library_root_path.Append(library_path);
  }

  // Use fdio_open_fd (a Fuchsia-specific API) here so we can pass the
  // appropriate FS rights flags to request executability.
  // TODO(crbug.com/40655456): Teach base::File about FLAG_WIN_EXECUTE on
  // Fuchsia, and then use it here instead of using fdio_open_fd() directly.
  base::ScopedFD fd;
  zx_status_t status = fdio_open_fd(
      computed_path.value().c_str(),
      static_cast<uint32_t>(fuchsia::io::OpenFlags::RIGHT_READABLE |
                            fuchsia::io::OpenFlags::RIGHT_EXECUTABLE),
      base::ScopedFD::Receiver(fd).get());
  if (status != ZX_OK) {
    if (error) {
      error->message =
          base::StringPrintf("fdio_open_fd: %s", zx_status_get_string(status));
    }
    return nullptr;
  }

  zx::vmo vmo;
  status = fdio_get_vmo_exec(fd.get(), vmo.reset_and_get_address());
  if (status != ZX_OK) {
    if (error) {
      error->message = base::StringPrintf("fdio_get_vmo_exec: %s",
                                          zx_status_get_string(status));
    }
    return nullptr;
  }

  NativeLibrary result = dlopen_vmo(vmo.get(), RTLD_LAZY | RTLD_LOCAL);
  return result;
}

void UnloadNativeLibrary(NativeLibrary library) {
  // dlclose() is a no-op on Fuchsia, so do nothing here.
}

void* GetFunctionPointerFromNativeLibrary(NativeLibrary library,
                                          const char* name) {
  return dlsym(library, name);
}

std::string GetNativeLibraryName(std::string_view name) {
  return StrCat({"lib", name, ".so"});
}

std::string GetLoadableModuleName(std::string_view name) {
  return GetNativeLibraryName(name);
}

}  // namespace base
