// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_FILE_PATH_RUST_SHIM_H_
#define BASE_FILES_FILE_PATH_RUST_SHIM_H_

#include <memory>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace base::rust::file_path {

#if BUILDFLAG(IS_WIN)
std::unique_ptr<FilePath> CreateFilePathFromWide(
    ::rust::Slice<const uint16_t> wide);
::rust::Slice<const uint16_t> FilePathToWide(const FilePath& path);
#else
std::unique_ptr<FilePath> CreateFilePathFromBytes(
    ::rust::Slice<const uint8_t> bytes);
::rust::Slice<const uint8_t> FilePathToBytes(const FilePath& path);
#endif

}  // namespace base::rust::file_path

#endif  // BASE_FILES_FILE_PATH_RUST_SHIM_H_
