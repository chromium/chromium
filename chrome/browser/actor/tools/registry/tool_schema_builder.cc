// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"

#include <ostream>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"

namespace actor {

namespace {

constexpr std::string_view kKeyType = "type";
constexpr std::string_view kKeyProperties = "properties";
constexpr std::string_view kKeyRequired = "required";
constexpr std::string_view kKeyDescription = "description";
constexpr std::string_view kKeyFormat = "format";
constexpr std::string_view kKeyEnum = "enum";
constexpr std::string_view kKeyAdditionalProperties = "additionalProperties";

constexpr std::string_view kTypeObject = "object";
constexpr std::string_view kTypeString = "string";
constexpr std::string_view kTypeInteger = "integer";
constexpr std::string_view kTypeNumber = "number";
constexpr std::string_view kTypeBoolean = "boolean";

base::DictValue CreateProperty(std::string_view type,
                               std::string_view description) {
  base::DictValue prop;
  prop.Set(kKeyType, type);
  prop.Set(kKeyDescription, description);
  return prop;
}

}  // namespace

ToolSchemaBuilder::ToolSchemaBuilder() = default;
ToolSchemaBuilder::~ToolSchemaBuilder() = default;

ToolSchemaBuilder& ToolSchemaBuilder::AddStringProperty(
    std::string_view name,
    std::string_view description,
    std::optional<std::string_view> format) {
  base::DictValue prop = CreateProperty(kTypeString, description);
  if (format.has_value()) {
    prop.Set(kKeyFormat, *format);
  }
  AddPropertyImpl(name, std::move(prop));
  return *this;
}

ToolSchemaBuilder& ToolSchemaBuilder::AddStringEnumProperty(
    std::string_view name,
    std::string_view description,
    base::span<const std::string_view> enum_values) {
  CHECK(!enum_values.empty()) << "Empty enum_values for parameter: " << name;
  base::ListValue enum_list;
  enum_list.reserve(enum_values.size());
  for (std::string_view value : enum_values) {
    enum_list.Append(value);
  }
  base::DictValue prop = CreateProperty(kTypeString, description);
  prop.Set(kKeyEnum, std::move(enum_list));
  AddPropertyImpl(name, std::move(prop));
  return *this;
}

ToolSchemaBuilder& ToolSchemaBuilder::AddIntegerProperty(
    std::string_view name,
    std::string_view description) {
  AddPropertyImpl(name, CreateProperty(kTypeInteger, description));
  return *this;
}

ToolSchemaBuilder& ToolSchemaBuilder::AddNumberProperty(
    std::string_view name,
    std::string_view description) {
  AddPropertyImpl(name, CreateProperty(kTypeNumber, description));
  return *this;
}

ToolSchemaBuilder& ToolSchemaBuilder::AddBooleanProperty(
    std::string_view name,
    std::string_view description) {
  AddPropertyImpl(name, CreateProperty(kTypeBoolean, description));
  return *this;
}

base::DictValue ToolSchemaBuilder::Build() {
  base::DictValue schema;
  schema.Set(kKeyType, kTypeObject);
  schema.Set(kKeyProperties, std::move(properties_));
  if (!required_.empty()) {
    schema.Set(kKeyRequired, std::move(required_));
  }
  schema.Set(kKeyAdditionalProperties, false);
  return schema;
}

void ToolSchemaBuilder::AddPropertyImpl(std::string_view name,
                                        base::DictValue prop_schema) {
  CHECK(!properties_.contains(name)) << "Duplicate parameter: " << name;
  properties_.Set(name, std::move(prop_schema));
  required_.Append(name);
}

}  // namespace actor
