// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"

#include <utility>

#include "base/json/json_parser.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

JSONReader::ValueWithError::ValueWithError() = default;

JSONReader::ValueWithError::ValueWithError(ValueWithError&& other) = default;

JSONReader::ValueWithError::~ValueWithError() = default;

JSONReader::ValueWithError& JSONReader::ValueWithError::operator=(
    ValueWithError&& other) = default;

// static
absl::optional<Value> JSONReader::Read(StringPiece json,
                                       int options,
                                       size_t max_depth) {
  internal::JSONParser parser(options, max_depth);
  return parser.Parse(json);
}

// static
std::unique_ptr<Value> JSONReader::ReadDeprecated(StringPiece json,
                                                  int options,
                                                  size_t max_depth) {
  absl::optional<Value> value = Read(json, options, max_depth);
  return value ? Value::ToUniquePtrValue(std::move(*value)) : nullptr;
}

// static
JSONReader::ValueWithError JSONReader::ReadAndReturnValueWithError(
    StringPiece json,
    int options) {
  ValueWithError ret;
  internal::JSONParser parser(options);
  ret.value = parser.Parse(json);
  if (!ret.value) {
    ret.error_message = parser.GetErrorMessage();
    ret.error_line = parser.error_line();
    ret.error_column = parser.error_column();
  }
  return ret;
}

}  // namespace base
