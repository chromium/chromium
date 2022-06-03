// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_FILE_ERROR_OR_H_
#define BASE_FILES_FILE_ERROR_OR_H_

#include <utility>

#include "base/check.h"
#include "base/files/file.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace base {

// Helper for methods which perform file system operations and which may fail.
// Objects of this type can take on EITHER a base::File::Error value OR a result
// value of the specified type.
template <typename ValueType>
class FileErrorOr {
 public:
  // These constructors are intentionally not marked `explicit` for cleaner code
  // at call sites.
  FileErrorOr(base::File::Error error) : error_or_value_(error) {}
  FileErrorOr(ValueType&& value) : error_or_value_(std::move(value)) {}
  FileErrorOr(const FileErrorOr&) = default;
  FileErrorOr(FileErrorOr&&) = default;
  FileErrorOr& operator=(const FileErrorOr&) = default;
  FileErrorOr& operator=(FileErrorOr&&) = default;
  ~FileErrorOr() = default;

  bool is_error() const {
    return absl::get_if<base::File::Error>(&error_or_value_);
  }
  base::File::Error error() const {
    CHECK(is_error());
    return absl::get<base::File::Error>(error_or_value_);
  }

  bool is_value() const { return absl::get_if<ValueType>(&error_or_value_); }
  ValueType& value() {
    CHECK(!is_error());
    return absl::get<ValueType>(error_or_value_);
  }
  const ValueType& value() const {
    CHECK(!is_error());
    return absl::get<const ValueType>(error_or_value_);
  }

  ValueType* operator->() { return &value(); }
  const ValueType* operator->() const { return &value(); }

 private:
  absl::variant<base::File::Error, ValueType> error_or_value_;
};

}  // namespace base

#endif  // BASE_FILES_FILE_ERROR_OR_H_
