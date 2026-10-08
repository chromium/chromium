// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/registry/tool_registry.h"

#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#include "chrome/browser/actor/tools/switch_tab_tool_request.h"
#endif

namespace actor {
namespace {

constexpr ToolId kUnrecognizedToolId = static_cast<ToolId>(999);

TEST(ToolRegistryTest, BaseToolRequestGetToolDefinitionReturnsNullopt) {
  EXPECT_EQ(ToolRequest::GetToolDefinition(), std::nullopt);
}

TEST(ToolRegistryTest, NavigateToolDefinition) {
  const std::optional<ToolDefinition> definition =
      NavigateToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kNavigate);
  EXPECT_EQ(definition->name, NavigateToolRequest::kModelFacingName);
  EXPECT_THAT(*definition,
              HasParamOfType(NavigateToolRequest::kUrlParam, "string"));
  EXPECT_THAT(*definition, RequiresParam(NavigateToolRequest::kUrlParam));
}

TEST(ToolRegistryTest, ClickToolDefinition) {
  const std::optional<ToolDefinition> definition =
      ClickToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kClick);
  EXPECT_EQ(definition->name, ClickToolRequest::kModelFacingName);
  EXPECT_THAT(*definition,
              HasParamOfType(ClickToolRequest::kDomNodeIdParam, "integer"));
  EXPECT_THAT(*definition, RequiresParam(ClickToolRequest::kDomNodeIdParam));
}

#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
TEST(ToolRegistryTest, SwitchTabToolDefinition) {
  const std::optional<ToolDefinition> definition =
      SwitchTabToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kSwitchTab);
  EXPECT_EQ(definition->name, SwitchTabToolRequest::kModelFacingName);
  EXPECT_THAT(*definition,
              HasParamOfType(SwitchTabToolRequest::kQueryParam, "string"));
  EXPECT_THAT(*definition, RequiresParam(SwitchTabToolRequest::kQueryParam));
}
#endif

TEST(ToolRegistryTest, GetAllToolsContainsNavigateTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kNavigate, &ToolDefinition::id));
}

#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
TEST(ToolRegistryTest, GetAllToolsContainsSwitchTabTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kSwitchTab, &ToolDefinition::id));
}
#endif

TEST(ToolRegistryTest, ToolIdToNameReturnsEmptyForUnrecognizedId) {
  EXPECT_TRUE(ToolRegistry::ToolIdToName(kUnrecognizedToolId).empty());
}

TEST(ToolRegistryTest, NameToToolIdReturnsNulloptForUnknownName) {
  EXPECT_EQ(ToolRegistry::NameToToolId("unknown_tool"), std::nullopt);
}

TEST(ToolRegistryTest, GetToolsByIdsReturnsEmptyForUnknownId) {
  ToolRegistry registry;
  EXPECT_TRUE(registry.GetToolsByIds({kUnrecognizedToolId}).empty());
}

TEST(ToolRegistryCompletenessTest, AllRegisteredToolsHaveKnownToolIds) {
  ToolRegistry registry;
  for (const ToolDefinition& tool : registry.GetAllTools()) {
    SCOPED_TRACE(tool.name);
    EXPECT_EQ(ToolRegistry::NameToToolId(tool.name), tool.id);
  }
}

TEST(ToolRegistryCompletenessTest, AllRegisteredToolsRoundTripByName) {
  ToolRegistry registry;
  for (const ToolDefinition& tool : registry.GetAllTools()) {
    SCOPED_TRACE(tool.name);
    EXPECT_EQ(ToolRegistry::NameToToolId(tool.name), tool.id);
    EXPECT_EQ(ToolRegistry::ToolIdToName(tool.id), tool.name);
  }
}

TEST(ToolRegistryCompletenessTest, AllRegisteredToolsHaveUniqueNames) {
  ToolRegistry registry;
  std::set<std::string_view> seen_names;
  for (const ToolDefinition& tool : registry.GetAllTools()) {
    SCOPED_TRACE(tool.name);
    EXPECT_TRUE(seen_names.insert(tool.name).second);
  }
}

TEST(ToolRegistryCompletenessTest, AllRegisteredToolsHaveUniqueIds) {
  ToolRegistry registry;
  std::set<ToolId> seen_ids;
  for (const ToolDefinition& tool : registry.GetAllTools()) {
    SCOPED_TRACE(tool.name);
    EXPECT_TRUE(seen_ids.insert(tool.id).second);
  }
}

TEST(ToolRegistryCompletenessTest, AllRegisteredToolsHaveNonEmptyDescriptions) {
  ToolRegistry registry;
  for (const ToolDefinition& tool : registry.GetAllTools()) {
    SCOPED_TRACE(tool.name);
    EXPECT_FALSE(tool.description.empty());
  }
}

TEST(ToolRegistryCompletenessTest, AllRegisteredToolsHaveObjectSchemas) {
  ToolRegistry registry;
  for (const ToolDefinition& tool : registry.GetAllTools()) {
    SCOPED_TRACE(tool.name);
    const std::string* type = tool.parameters_json_schema.FindString("type");
    ASSERT_TRUE(type);
    EXPECT_EQ(*type, "object");
  }
}

TEST(ToolRegistryCompletenessTest, AllRegisteredToolsRoundTripById) {
  ToolRegistry registry;
  for (const ToolDefinition& tool : registry.GetAllTools()) {
    SCOPED_TRACE(tool.name);
    EXPECT_EQ(registry.GetToolsByIds({tool.id}),
              std::vector<const ToolDefinition*>{&tool});
  }
}

}  // namespace
}  // namespace actor
