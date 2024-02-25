// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_writer.h"

#include <stdint.h>

#include <cmath>
#include <limits>
#include <string_view>

#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"

namespace base {

#if BUILDFLAG(IS_WIN)
const char kPrettyPrintLineEnding[] = "\r\n";
#else
const char kPrettyPrintLineEnding[] = "\n";
#endif

// static
bool JSONWriter::Write(ValueView node, std::string* json, size_t max_depth) {
  return WriteWithOptions(node, 0, json, max_depth);
}

// static
bool JSONWriter::WriteWithOptions(ValueView node,
                                  int options,
                                  std::string* json,
                                  size_t max_depth) {
  json->clear();
  // Is there a better way to estimate the size of the output?
  if (json->capacity() < 1024) {
    json->reserve(1024);
  }

  JSONWriter writer(options, json, max_depth);
  bool result = node.Visit([&writer](const auto& member) {
    return writer.BuildJSONString(member, 0);
  });

  if (options & OPTIONS_PRETTY_PRINT) {
    json->append(kPrettyPrintLineEnding);
  }

  return result;
}

JSONWriter::JSONWriter(int options, std::string* json, size_t max_depth)
    : omit_binary_values_((options & OPTIONS_OMIT_BINARY_VALUES) != 0),
      omit_double_type_preservation_(
          (options & OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION) != 0),
      pretty_print_((options & OPTIONS_PRETTY_PRINT) != 0),
      json_string_(json),
      max_depth_(max_depth),
      stack_depth_(0) {
  DCHECK(json);
  CHECK_LE(max_depth, internal::kAbsoluteMaxDepth);
}

bool JSONWriter::BuildJSONString(absl::monostate node, size_t depth) {
  json_string_->append("null");
  return true;
}

bool JSONWriter::BuildJSONString(bool node, size_t depth) {
  json_string_->append(node ? "true" : "false");
  return true;
}

bool JSONWriter::BuildJSONString(int node, size_t depth) {
  json_string_->append(NumberToString(node));
  return true;
}

bool JSONWriter::BuildJSONString(double node, size_t depth) {
  if (omit_double_type_preservation_ &&
      IsValueInRangeForNumericType<int64_t>(node) && std::floor(node) == node) {
    json_string_->append(NumberToString(static_cast<int64_t>(node)));
    return true;
  }

  std::string real = NumberToString(node);
  // Ensure that the number has a .0 if there's no decimal or 'e'.  This
  // makes sure that when we read the JSON back, it's interpreted as a
  // real rather than an int.
  if (real.find_first_of(".eE") == std::string::npos) {
    real.append(".0");
  }

  // The JSON spec requires that non-integer values in the range (-1,1)
  // have a zero before the decimal point - ".52" is not valid, "0.52" is.
  if (real[0] == '.') {
    real.insert(0, 1, '0');
  } else if (real.length() > 1 && real[0] == '-' && real[1] == '.') {
    // "-.1" bad "-0.1" good
    real.insert(1, 1, '0');
  }
  json_string_->append(real);
  return true;
}

bool JSONWriter::BuildJSONString(std::string_view node, size_t depth) {
  EscapeJSONString(node, true, json_string_);
  return true;
}

bool JSONWriter::BuildJSONString(const Value::BlobStorage& node, size_t depth) {
  // Successful only if we're allowed to omit it.
  DLOG_IF(ERROR, !omit_binary_values_) << "Cannot serialize binary value.";
  return omit_binary_values_;
}

bool JSONWriter::BuildJSONString(const Value::Dict& node, size_t depth) {
  internal::StackMarker depth_check(max_depth_, &stack_depth_);

  if (depth_check.IsTooDeep()) {
    return false;
  }

  json_string_->push_back('{');
  if (pretty_print_) {
    json_string_->append(kPrettyPrintLineEnding);
  }

  bool first_value_has_been_output = false;
  bool result = true;
  for (const auto [key, value] : node) {
    if (omit_binary_values_ && value.type() == Value::Type::BINARY) {
      continue;
    }

    if (first_value_has_been_output) {
      json_string_->push_back(',');
      if (pretty_print_) {
        json_string_->append(kPrettyPrintLineEnding);
      }
    }

    if (pretty_print_) {
      IndentLine(depth + 1U);
    }

    EscapeJSONString(key, true, json_string_);
    json_string_->push_back(':');
    if (pretty_print_) {
      json_string_->push_back(' ');
    }

    result &= value.Visit([this, depth = depth + 1](const auto& member) {
      return BuildJSONString(member, depth);
    });

    first_value_has_been_output = true;
  }

  if (pretty_print_) {
    if (first_value_has_been_output) {
      json_string_->append(kPrettyPrintLineEnding);
    }
    IndentLine(depth);
  }

  json_string_->push_back('}');
  return result;
}

bool JSONWriter::BuildJSONString(const Value::List& node, size_t depth) {
  internal::StackMarker depth_check(max_depth_, &stack_depth_);

  if (depth_check.IsTooDeep()) {
    return false;
  }

  json_string_->push_back('[');
  if (pretty_print_) {
    json_string_->push_back(' ');
  }

  bool first_value_has_been_output = false;
  bool result = true;
  for (const auto& value : node) {
    if (omit_binary_values_ && value.type() == Value::Type::BINARY) {
      continue;
    }

    if (first_value_has_been_output) {
      json_string_->push_back(',');
      if (pretty_print_) {
        json_string_->push_back(' ');
      }
    }

    result &= value.Visit([this, depth](const auto& member) {
      return BuildJSONString(member, depth);
    });

    first_value_has_been_output = true;
  }

  if (pretty_print_) {
    json_string_->push_back(' ');
  }
  json_string_->push_back(']');
  return result;
}

void JSONWriter::IndentLine(size_t depth) {
  json_string_->append(depth * 3U, ' ');
}

std::optional<std::string> WriteJson(ValueView node, size_t max_depth) {
  std::string result;
  if (!JSONWriter::Write(node, &result, max_depth)) {
    return std::nullopt;
  }
  return result;
}

std::optional<std::string> WriteJsonWithOptions(ValueView node,
                                                uint32_t options,
                                                size_t max_depth) {
  std::string result;
  if (!JSONWriter::WriteWithOptions(node, static_cast<int>(options), &result,
                                    max_depth)) {
    return std::nullopt;
  }
  return result;
}

}  // namespace base
