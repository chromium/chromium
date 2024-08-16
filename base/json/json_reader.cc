// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"

#include <string_view>
#include <utility>

#include "base/features.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/rust_buildflags.h"

#if BUILDFLAG(BUILD_RUST_JSON_READER)
#include "base/strings/string_view_rust.h"
#include "third_party/rust/serde_json_lenient/v0_2/wrapper/functions.h"
#include "third_party/rust/serde_json_lenient/v0_2/wrapper/lib.rs.h"
#endif  // BUILDFLAG(BUILD_RUST_JSON_READER)
#include "base/json/json_parser.h"

namespace base {

#if BUILDFLAG(BUILD_RUST_JSON_READER)

namespace {
using serde_json_lenient::ContextPointer;

const char kSecurityJsonParsingTime[] = "Security.JSONParser.ParsingTime";

ContextPointer& ListAppendList(ContextPointer& ctx, size_t reserve) {
  auto& value = reinterpret_cast<base::Value&>(ctx);
  value.GetList().reserve(reserve);
  value.GetList().Append(base::Value::List());
  return reinterpret_cast<ContextPointer&>(value.GetList().back());
}

ContextPointer& ListAppendDict(ContextPointer& ctx) {
  auto& value = reinterpret_cast<base::Value&>(ctx);
  value.GetList().Append(base::Value::Dict());
  return reinterpret_cast<ContextPointer&>(value.GetList().back());
}

void ListAppendNone(ContextPointer& ctx) {
  auto& value = reinterpret_cast<base::Value&>(ctx);
  value.GetList().Append(base::Value());
}

template <class T, class As = T>
void ListAppendValue(ContextPointer& ctx, T v) {
  auto& value = reinterpret_cast<base::Value&>(ctx);
  value.GetList().Append(As{v});
}

ContextPointer& DictSetList(ContextPointer& ctx,
                            rust::Str key,
                            size_t reserve) {
  auto& dict = reinterpret_cast<base::Value&>(ctx).GetDict();
  base::Value::List list;
  list.reserve(reserve);
  dict.Set(base::RustStrToStringView(key), std::move(list));
  return reinterpret_cast<ContextPointer&>(
      *dict.Find(base::RustStrToStringView(key)));
}

ContextPointer& DictSetDict(ContextPointer& ctx, rust::Str key) {
  auto& dict = reinterpret_cast<base::Value&>(ctx).GetDict();
  dict.Set(base::RustStrToStringView(key), base::Value(base::Value::Dict()));
  return reinterpret_cast<ContextPointer&>(
      *dict.Find(base::RustStrToStringView(key)));
}

void DictSetNone(ContextPointer& ctx, rust::Str key) {
  auto& dict = reinterpret_cast<base::Value&>(ctx).GetDict();
  dict.Set(base::RustStrToStringView(key), base::Value());
}

template <class T, class As = T>
void DictSetValue(ContextPointer& ctx, rust::Str key, T v) {
  auto& dict = reinterpret_cast<base::Value&>(ctx).GetDict();
  dict.Set(base::RustStrToStringView(key), base::Value(As{v}));
}

JSONReader::Result DecodeJSONInRust(std::string_view json,
                                    int options,
                                    size_t max_depth) {
  const serde_json_lenient::JsonOptions rust_options = {
      .allow_trailing_commas =
          (options & base::JSON_ALLOW_TRAILING_COMMAS) != 0,
      .replace_invalid_characters =
          (options & base::JSON_REPLACE_INVALID_CHARACTERS) != 0,
      .allow_comments = (options & base::JSON_ALLOW_COMMENTS) != 0,
      .allow_newlines = (options & base::JSON_ALLOW_NEWLINES_IN_STRINGS) != 0,
      .allow_control_chars = (options & base::JSON_ALLOW_CONTROL_CHARS) != 0,
      .allow_vert_tab = (options & base::JSON_ALLOW_VERT_TAB) != 0,
      .allow_x_escapes = (options & base::JSON_ALLOW_X_ESCAPES) != 0,
      .max_depth = max_depth,
  };
  static constexpr serde_json_lenient::Functions functions = {
      .list_append_none_fn = ListAppendNone,
      .list_append_bool_fn = ListAppendValue<bool>,
      .list_append_i32_fn = ListAppendValue<int32_t>,
      .list_append_f64_fn = ListAppendValue<double>,
      .list_append_str_fn = ListAppendValue<rust::Str, std::string>,
      .list_append_list_fn = ListAppendList,
      .list_append_dict_fn = ListAppendDict,
      .dict_set_none_fn = DictSetNone,
      .dict_set_bool_fn = DictSetValue<bool>,
      .dict_set_i32_fn = DictSetValue<int32_t>,
      .dict_set_f64_fn = DictSetValue<double>,
      .dict_set_str_fn = DictSetValue<rust::Str, std::string>,
      .dict_set_list_fn = DictSetList,
      .dict_set_dict_fn = DictSetDict,
  };

  base::Value value(base::Value::Type::LIST);
  auto& ctx = reinterpret_cast<ContextPointer&>(value);
  serde_json_lenient::DecodeError error;
  bool ok = serde_json_lenient::decode_json(
      base::StringViewToRustSlice(json), rust_options, functions, ctx, error);

  if (!ok) {
    return base::unexpected(base::JSONReader::Error{
        .message = std::string(error.message),
        .line = error.line,
        .column = error.column,
    });
  }

  return std::move(std::move(value.GetList()).back());
}

}  // anonymous namespace

#endif  // BUILDFLAG(BUILD_RUST_JSON_READER)

// static
std::optional<Value> JSONReader::Read(std::string_view json,
                                      int options,
                                      size_t max_depth) {
#if BUILDFLAG(BUILD_RUST_JSON_READER)
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(kSecurityJsonParsingTime);
  if (UsingRust()) {
    JSONReader::Result result = DecodeJSONInRust(json, options, max_depth);
    if (!result.has_value()) {
      return std::nullopt;
    }
    return std::move(*result);
  } else {
    internal::JSONParser parser(options, max_depth);
    return parser.Parse(json);
  }
#else   // BUILDFLAG(BUILD_RUST_JSON_READER)
  internal::JSONParser parser(options, max_depth);
  return parser.Parse(json);
#endif  // BUILDFLAG(BUILD_RUST_JSON_READER)
}

// static
std::optional<Value::Dict> JSONReader::ReadDict(std::string_view json,
                                                int options,
                                                size_t max_depth) {
  std::optional<Value> value = Read(json, options, max_depth);
  if (!value || !value->is_dict()) {
    return std::nullopt;
  }
  return std::move(*value).TakeDict();
}

// static
JSONReader::Result JSONReader::ReadAndReturnValueWithError(
    std::string_view json,
    int options) {
#if BUILDFLAG(BUILD_RUST_JSON_READER)
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(kSecurityJsonParsingTime);
  if (UsingRust()) {
    return DecodeJSONInRust(json, options, internal::kAbsoluteMaxDepth);
  } else {
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
  }
#else   // BUILDFLAG(BUILD_RUST_JSON_READER)
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
#endif  // BUILDFLAG(BUILD_RUST_JSON_READER)
}

// static
bool JSONReader::UsingRust() {
  // If features have not yet been enabled, we cannot check the feature, so fall
  // back to the C++ parser. In practice, this seems to apply to
  // `ReadPrefsFromDisk()`, which is parsing trusted JSON.
  if (!base::FeatureList::GetInstance()) {
    return false;
  }
#if BUILDFLAG(BUILD_RUST_JSON_READER)
  return base::FeatureList::IsEnabled(base::features::kUseRustJsonParser);
#else   // BUILDFLAG(BUILD_RUST_JSON_READER)
  return false;
#endif  // BUILDFLAG(BUILD_RUST_JSON_READER)
}

}  // namespace base
