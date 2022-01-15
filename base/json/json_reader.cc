// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"

#include <utility>

#include "base/logging.h"
#include "base/parsing_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(BUILD_RUST_JSON_PARSER)
#include "base/json/json_parser.rs.h"
#include "base/strings/string_piece_rust.h"
#else
#include "base/json/json_parser.h"
#endif

namespace base {

#if BUILDFLAG(BUILD_RUST_JSON_PARSER)

namespace {

base::JSONReader::ValueWithError DecodeJSONInRust(const base::StringPiece& json,
                                                  int options,
                                                  size_t max_depth) {
  int32_t error_line;
  int32_t error_column;
  base::ffi::json::json_parser::JsonOptions rust_options;
  rust_options.allow_trailing_commas =
      options & base::JSON_ALLOW_TRAILING_COMMAS;
  rust_options.replace_invalid_characters =
      options & base::JSON_REPLACE_INVALID_CHARACTERS;
  rust_options.allow_comments = options & base::JSON_ALLOW_COMMENTS;
  rust_options.allow_vert_tab = options & base::JSON_ALLOW_VERT_TAB;
  rust_options.allow_control_chars = options & base::JSON_ALLOW_CONTROL_CHARS;
  rust_options.allow_x_escapes = options & base::JSON_ALLOW_X_ESCAPES;
  rust_options.max_depth = max_depth;
  base::JSONReader::ValueWithError ret;
  bool ok = base::ffi::json::json_parser::decode_json_from_cpp(
      base::StringPieceToRustSlice(json), rust_options, ret.value, error_line,
      error_column, ret.error_message);
  if (!ok) {
    ret.value.reset();
    ret.error_line = error_line;
    ret.error_column = error_column;
  }
  return ret;
}

}  // anonymous namespace

#endif  // BUILDFLAG(BUILD_RUST_JSON_PARSER)

JSONReader::ValueWithError::ValueWithError() = default;

JSONReader::ValueWithError::ValueWithError(ValueWithError&& other) = default;

JSONReader::ValueWithError::~ValueWithError() = default;

JSONReader::ValueWithError& JSONReader::ValueWithError::operator=(
    ValueWithError&& other) = default;

// static
absl::optional<Value> JSONReader::Read(StringPiece json,
                                       int options,
                                       size_t max_depth) {
#if BUILDFLAG(BUILD_RUST_JSON_PARSER)
  ValueWithError result = DecodeJSONInRust(json, options, max_depth);
  return std::move(result.value);
#else   // BUILDFLAG(BUILD_RUST_JSON_PARSER)
  internal::JSONParser parser(options, max_depth);
  return parser.Parse(json);
#endif  // BUILDFLAG(BUILD_RUST_JSON_PARSER)
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
#if BUILDFLAG(BUILD_RUST_JSON_PARSER)
  return DecodeJSONInRust(json, options, internal::kAbsoluteMaxDepth);
#else   // BUILDFLAG(BUILD_RUST_JSON_PARSER)
  internal::JSONParser parser(options);
  ValueWithError ret;
  ret.value = parser.Parse(json);
  if (!ret.value) {
    ret.error_message = parser.GetErrorMessage();
    ret.error_line = parser.error_line();
    ret.error_column = parser.error_column();
  }
  return ret;
#endif  // BUILDFLAG(BUILD_RUST_JSON_PARSER)
}

}  // namespace base
