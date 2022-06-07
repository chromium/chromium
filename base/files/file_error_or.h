// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_FILE_ERROR_OR_H_
#define BASE_FILES_FILE_ERROR_OR_H_

#include <utility>

#include "base/check.h"
#include "base/files/file.h"
#include "base/types/expected.h"

namespace base {

// Helper for methods which perform file system operations and which may fail.
// Objects of this type can take on EITHER a base::File::Error value OR a result
// value of the specified type.
template <typename ValueType>
class FileErrorOr {
 public:
  // These constructors are intentionally not marked `explicit` for cleaner code
  // at call sites.
  FileErrorOr(File::Error error) : value_or_error_(unexpected(error)) {}
  FileErrorOr(ValueType&& value) : value_or_error_(std::move(value)) {}
  FileErrorOr(const FileErrorOr&) = default;
  FileErrorOr(FileErrorOr&&) = default;
  FileErrorOr& operator=(const FileErrorOr&) = default;
  FileErrorOr& operator=(FileErrorOr&&) = default;
  ~FileErrorOr() = default;

  bool is_error() const { return !value_or_error_.has_value(); }
  File::Error error() const { return value_or_error_.error(); }

  bool is_value() const { return value_or_error_.has_value(); }
  ValueType& value() { return value_or_error_.value(); }
  const ValueType& value() const { return value_or_error_.value(); }

  ValueType* operator->() { return &value(); }
  const ValueType* operator->() const { return &value(); }

 private:
  expected<ValueType, File::Error> value_or_error_;
};

}  // namespace base

#endif  // BASE_FILES_FILE_ERROR_OR_H_
