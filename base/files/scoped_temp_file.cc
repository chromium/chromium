// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_file.h"

#include <utility>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/logging.h"

namespace base {

ScopedTempFile::ScopedTempFile() = default;

ScopedTempFile::ScopedTempFile(ScopedTempFile&& other) noexcept
    : path_(std::move(other.path_)) {}

ScopedTempFile& ScopedTempFile::operator=(ScopedTempFile&& other) noexcept {
  if (!path_.empty()) {
    CHECK_NE(path_, other.path_);
  }
  if (!Delete()) {
    DLOG(WARNING) << "Could not delete temp dir in operator=().";
  }
  path_ = std::move(other.path_);
  return *this;
}

ScopedTempFile::~ScopedTempFile() {
  if (!Delete()) {
    DLOG(WARNING) << "Could not delete temp dir in destructor.";
  }
}

bool ScopedTempFile::Create() {
  CHECK(path_.empty());
  return base::CreateTemporaryFile(&path_);
}

bool ScopedTempFile::Delete() {
  if (path_.empty()) {
    return true;
  }
  if (DeleteFile(path_)) {
    path_.clear();
    return true;
  }
  return false;
}

void ScopedTempFile::Reset() {
  if (!Delete()) {
    DLOG(WARNING) << "Could not delete temp dir in Reset().";
  }
  path_.clear();
}

}  // namespace base
