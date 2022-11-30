// Copyright 2012 The Chromium Authors
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

base::expected<Value, JSONReader::Error>
DecodeJSONInRust(const base::StringPiece& json, int options, size_t max_depth) {
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
  base::JSONReader::Error error;
  absl::optional<base::Value> value;
  bool ok = base::ffi::json::json_parser::decode_json_from_cpp(
      base::StringPieceToRustSlice(json), rust_options, value, error_line,
      error_column, error.message);
  if (!ok) {
    error.line = error_line;
    error.column = error_column;
    return base::unexpected(std::move(error));
  }
  return std::move(*value);
}

}  // anonymous namespace

#endif  // BUILDFLAG(BUILD_RUST_JSON_PARSER)

JSONReader::Error::Error() = default;

JSONReader::Error::Error(Error&& other) = default;

JSONReader::Error::~Error() = default;

JSONReader::Error& JSONReader::Error::operator=(Error&& other) = default;

// static
absl::optional<Value> JSONReader::Read(StringPiece json,
                                       int options,
                                       size_t max_depth) {
#if BUILDFLAG(BUILD_RUST_JSON_PARSER)
  auto result = DecodeJSONInRust(json, options, max_depth);
  if (!result.has_value()) {
    return absl::nullopt;
  }
  return std::move(*result);
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
JSONReader::Result JSONReader::ReadAndReturnValueWithError(StringPiece json,
                                                           int options) {
#if BUILDFLAG(BUILD_RUST_JSON_PARSER)
  return DecodeJSONInRust(json, options, internal::kAbsoluteMaxDepth);
#else   // BUILDFLAG(BUILD_RUST_JSON_PARSER)
  internal::JSONParser parser(options);
  auto value = parser.Parse(json);
  if (!value) {
    Error error;
    error.message = parser.GetErrorMessage();
    error.line = parser.error_line();
    error.column = parser.error_column();
    return base::unexpected(std::move(error));
  }

  return std::move(*value);
#endif  // BUILDFLAG(BUILD_RUST_JSON_PARSER)
}

}  // namespace base
