// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"

#include <string_view>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "base/strings/string_view_rust.h"
#include "third_party/rust/serde_json_lenient/v0_2/wrapper/functions.h"
#include "third_party/rust/serde_json_lenient/v0_2/wrapper/lib.rs.h"

// This namespace defines FFI-friendly functions that are be called from Rust in
// //third_party/rust/serde_json_lenient/v0_2/wrapper/.
namespace serde_json_lenient {

base::ListValue& list_append_list(base::ListValue& ctx) {
  ctx.Append(base::ListValue());
  return ctx.back().GetList();
}

base::DictValue& list_append_dict(base::ListValue& ctx) {
  ctx.Append(base::DictValue());
  return ctx.back().GetDict();
}

void list_append_none(base::ListValue& ctx) {
  ctx.Append(base::Value());
}

void list_append_bool(base::ListValue& ctx, bool val) {
  ctx.Append(val);
}

void list_append_i32(base::ListValue& ctx, int32_t val) {
  ctx.Append(val);
}

void list_append_f64(base::ListValue& ctx, double val) {
  ctx.Append(val);
}

void list_append_str(base::ListValue& ctx, rust::Str val) {
  ctx.Append(std::string(val));
}

base::ListValue& dict_set_list(base::DictValue& ctx, rust::Str key) {
  base::Value* value =
      ctx.Set(base::RustStrToStringView(key), base::ListValue());
  return value->GetList();
}

base::DictValue& dict_set_dict(base::DictValue& ctx, rust::Str key) {
  base::Value* value =
      ctx.Set(base::RustStrToStringView(key), base::DictValue());
  return value->GetDict();
}

void dict_set_none(base::DictValue& ctx, rust::Str key) {
  ctx.Set(base::RustStrToStringView(key), base::Value());
}

void dict_set_bool(base::DictValue& ctx, rust::Str key, bool val) {
  ctx.Set(base::RustStrToStringView(key), val);
}

void dict_set_i32(base::DictValue& ctx, rust::Str key, int32_t val) {
  ctx.Set(base::RustStrToStringView(key), val);
}

void dict_set_f64(base::DictValue& ctx, rust::Str key, double val) {
  ctx.Set(base::RustStrToStringView(key), val);
}

void dict_set_str(base::DictValue& ctx, rust::Str key, rust::Str val) {
  ctx.Set(base::RustStrToStringView(key), std::string(val));
}

namespace {

base::JSONReader::Result DecodeJSONInRust(std::string_view json,
                                          int options,
                                          size_t max_depth) {
  const JsonOptions rust_options = {
      .allow_trailing_commas =
          (options & base::JSON_ALLOW_TRAILING_COMMAS) != 0,
      .replace_invalid_characters =
          (options & base::JSON_REPLACE_INVALID_CHARACTERS) != 0,
      .allow_comments = (options & base::JSON_ALLOW_COMMENTS) != 0,
      .allow_newlines = (options & base::JSON_ALLOW_NEWLINES_IN_STRINGS) != 0,
      .allow_vert_tab = (options & base::JSON_ALLOW_VERT_TAB) != 0,
      .allow_x_escapes = (options & base::JSON_ALLOW_X_ESCAPES) != 0,
      .max_depth = max_depth,
  };

  base::ListValue list;
  DecodeError error;
  bool ok =
      decode_json(base::StringViewToRustSlice(json), rust_options, list, error);

  if (!ok) {
    return base::unexpected(base::JSONReader::Error{
        .message = std::string(error.message),
        .line = error.line,
        .column = error.column,
    });
  }

  return std::move(list.back());
}

}  // namespace
}  // namespace serde_json_lenient

namespace base {

std::string JSONReader::Error::ToString() const {
  return base::StrCat({"line ", base::NumberToString(line), ", column ",
                       base::NumberToString(column), ": ", message});
}

// static
std::optional<Value> JSONReader::Read(std::string_view json,
                                      int options,
                                      size_t max_depth) {
  JSONReader::Result result =
      serde_json_lenient::DecodeJSONInRust(json, options, max_depth);
  if (!result.has_value()) {
    return std::nullopt;
  }
  return std::move(*result);
}

// static
std::optional<DictValue> JSONReader::ReadDict(std::string_view json,
                                              int options,
                                              size_t max_depth) {
  std::optional<Value> value = Read(json, options, max_depth);
  if (!value || !value->is_dict()) {
    return std::nullopt;
  }
  return std::move(*value).TakeDict();
}

// static
std::optional<ListValue> JSONReader::ReadList(std::string_view json,
                                              int options,
                                              size_t max_depth) {
  std::optional<Value> value = Read(json, options, max_depth);
  if (!value || !value->is_list()) {
    return std::nullopt;
  }
  return std::move(*value).TakeList();
}

// static
JSONReader::Result JSONReader::ReadAndReturnValueWithError(
    std::string_view json,
    int options) {
  return serde_json_lenient::DecodeJSONInRust(json, options,
                                              internal::kAbsoluteMaxDepth);
}

}  // namespace base
