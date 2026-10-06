// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/registry/tool_definition_test_util.h"

#include <string_view>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace actor {

testing::Matcher<const ToolDefinition&> HasParamOfType(std::string_view param,
                                                       std::string_view type) {
  return testing::Field(
      "parameters_json_schema", &ToolDefinition::parameters_json_schema,
      base::test::IsSupersetOfValue(base::DictValue().Set(
          "properties",
          base::DictValue().Set(param, base::DictValue().Set("type", type)))));
}

testing::Matcher<const ToolDefinition&> RequiresParam(std::string_view param) {
  return testing::Field("parameters_json_schema",
                        &ToolDefinition::parameters_json_schema,
                        base::test::IsSupersetOfValue(base::DictValue().Set(
                            "required", base::ListValue().Append(param))));
}

}  // namespace actor
