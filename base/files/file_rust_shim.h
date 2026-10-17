// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_FILE_RUST_SHIM_H_
#define BASE_FILES_FILE_RUST_SHIM_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/files/file.h"
#include "build/build_config.h"

namespace base::rust::file {

#if BUILDFLAG(IS_WIN)
// `handle` must be an open HANDLE owned by the caller.
std::unique_ptr<File> CreateFileFromRawHandle(size_t handle);
size_t TakeRawHandle(File& file);
#else
// `fd` must be an open file descriptor owned by the caller.
std::unique_ptr<File> CreateFileFromRawFd(int32_t fd);
int32_t TakeRawFd(File& file);
#endif

}  // namespace base::rust::file

#endif  // BASE_FILES_FILE_RUST_SHIM_H_
