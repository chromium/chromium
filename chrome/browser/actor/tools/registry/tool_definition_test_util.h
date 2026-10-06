// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_REGISTRY_TOOL_DEFINITION_TEST_UTIL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_REGISTRY_TOOL_DEFINITION_TEST_UTIL_H_

#include <string_view>

#include "testing/gmock/include/gmock/gmock.h"

namespace actor {

struct ToolDefinition;

// Matches a `ToolDefinition` whose `parameters_json_schema` declares `param`
// under "properties" with the JSON Schema `type` (e.g. "string", "integer").
// Extra properties and extra keys on the parameter are ignored.
//
// Example:
//   EXPECT_THAT(FooToolRequest::GetToolDefinition(),
//               testing::Optional(HasParamOfType("url", "string")));
testing::Matcher<const ToolDefinition&> HasParamOfType(std::string_view param,
                                                       std::string_view type);

// Matches a `ToolDefinition` whose `parameters_json_schema` lists `param` in
// its "required" array. Other required parameters are ignored.
//
// Example:
//   EXPECT_THAT(FooToolRequest::GetToolDefinition(),
//               testing::Optional(RequiresParam("url")));
testing::Matcher<const ToolDefinition&> RequiresParam(std::string_view param);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_REGISTRY_TOOL_DEFINITION_TEST_UTIL_H_
