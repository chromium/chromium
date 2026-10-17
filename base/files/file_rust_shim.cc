// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_rust_shim.h"

namespace base::rust::file {

#if BUILDFLAG(IS_WIN)
std::unique_ptr<File> CreateFileFromRawHandle(size_t handle) {
  return std::make_unique<File>(reinterpret_cast<PlatformFile>(handle));
}

size_t TakeRawHandle(File& file) {
  return reinterpret_cast<size_t>(file.TakePlatformFile());
}
#else
std::unique_ptr<File> CreateFileFromRawFd(int32_t fd) {
  return std::make_unique<File>(fd);
}

int32_t TakeRawFd(File& file) {
  return file.TakePlatformFile();
}
#endif

}  // namespace base::rust::file
