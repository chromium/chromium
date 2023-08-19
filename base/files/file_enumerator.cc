// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_enumerator.h"

#include "base/files/file_util.h"
#include "base/functional/function_ref.h"

namespace base {

FileEnumerator::FileInfo::~FileInfo() = default;

bool FileEnumerator::ShouldSkip(const FilePath& path) {
  FilePath::StringType basename = path.BaseName().value();
  return basename == FILE_PATH_LITERAL(".") ||
         (basename == FILE_PATH_LITERAL("..") &&
          !(INCLUDE_DOT_DOT & file_type_));
}

bool FileEnumerator::IsTypeMatched(bool is_dir) const {
  return (file_type_ &
          (is_dir ? FileEnumerator::DIRECTORIES : FileEnumerator::FILES)) != 0;
}

void FileEnumerator::ForEach(FunctionRef<void(const FilePath& path)> ref) {
  for (FilePath name = Next(); !name.empty(); name = Next()) {
    ref(name);
  }
}

}  // namespace base
