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
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/history_tool_request.h"
#include "chrome/browser/actor/tools/media_control_tool_request.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/tools/perform_search_tool_request.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_test_util.h"
#include "chrome/browser/actor/tools/scroll_tool_request.h"
#include "chrome/browser/actor/tools/select_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/translate_page_tool_request.h"
#include "chrome/browser/actor/tools/type_tool_request.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#include "chrome/browser/actor/tools/open_known_page_tool_request.h"
#include "chrome/browser/actor/tools/switch_tab_tool_request.h"
#include "chrome/browser/actor/tools/window_management_tool_request.h"
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

TEST(ToolRegistryTest, ScrollToolDefinition) {
  const std::optional<ToolDefinition> definition =
      ScrollToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kScroll);
  EXPECT_EQ(definition->name, ScrollToolRequest::kModelFacingName);
  EXPECT_THAT(*definition,
              HasParamOfType(ScrollToolRequest::kDirectionParam, "string"));
  EXPECT_THAT(*definition, RequiresParam(ScrollToolRequest::kDirectionParam));
  EXPECT_THAT(*definition,
              HasParamOfType(ScrollToolRequest::kDistanceParam, "number"));
  EXPECT_THAT(*definition, RequiresParam(ScrollToolRequest::kDistanceParam));
  EXPECT_THAT(*definition,
              HasParamOfType(ScrollToolRequest::kDomNodeIdParam, "integer"));
  EXPECT_THAT(*definition, RequiresParam(ScrollToolRequest::kDomNodeIdParam));
}
TEST(ToolRegistryTest, SelectOptionToolDefinition) {
  const std::optional<ToolDefinition> definition =
      SelectToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kSelectOption);
  EXPECT_EQ(definition->name, SelectToolRequest::kModelFacingName);
  EXPECT_THAT(*definition,
              HasParamOfType(SelectToolRequest::kDomNodeIdParam, "integer"));
  EXPECT_THAT(*definition, RequiresParam(SelectToolRequest::kDomNodeIdParam));
  EXPECT_THAT(*definition,
              HasParamOfType(SelectToolRequest::kValueParam, "string"));
  EXPECT_THAT(*definition, RequiresParam(SelectToolRequest::kValueParam));
}

TEST(ToolRegistryTest, TypeToolDefinition) {
  const std::optional<ToolDefinition> definition =
      TypeToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kType);
  EXPECT_EQ(definition->name, TypeToolRequest::kModelFacingName);
  EXPECT_THAT(*definition,
              HasParamOfType(TypeToolRequest::kDomNodeIdParam, "integer"));
  EXPECT_THAT(*definition, RequiresParam(TypeToolRequest::kDomNodeIdParam));
  EXPECT_THAT(*definition,
              HasParamOfType(TypeToolRequest::kTextParam, "string"));
  EXPECT_THAT(*definition, RequiresParam(TypeToolRequest::kTextParam));
  EXPECT_THAT(*definition,
              HasParamOfType(TypeToolRequest::kFollowByEnterParam, "boolean"));
  EXPECT_THAT(*definition, RequiresParam(TypeToolRequest::kFollowByEnterParam));
}

TEST(ToolRegistryTest, TranslatePageToolDefinition) {
  const std::optional<ToolDefinition> definition =
      TranslatePageToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kTranslatePage);
  EXPECT_EQ(definition->name, TranslatePageToolRequest::kModelFacingName);
  EXPECT_THAT(
      *definition,
      HasParamOfType(TranslatePageToolRequest::kTargetLanguageParam, "string"));
  EXPECT_THAT(*definition,
              RequiresParam(TranslatePageToolRequest::kTargetLanguageParam));
}

TEST(ToolRegistryTest, HistoryBackToolDefinition) {
  const std::optional<ToolDefinition> definition =
      HistoryBackToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kGoBack);
  EXPECT_EQ(definition->name, HistoryBackToolRequest::kModelFacingName);
  EXPECT_THAT(definition->parameters_json_schema,
              base::test::DictionaryHasValue("properties",
                                             base::Value(base::DictValue())));
}

TEST(ToolRegistryTest, HistoryForwardToolDefinition) {
  const std::optional<ToolDefinition> definition =
      HistoryForwardToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kGoForward);
  EXPECT_EQ(definition->name, HistoryForwardToolRequest::kModelFacingName);
  EXPECT_THAT(definition->parameters_json_schema,
              base::test::DictionaryHasValue("properties",
                                             base::Value(base::DictValue())));
}

TEST(ToolRegistryTest, ReloadPageToolDefinition) {
  const std::optional<ToolDefinition> definition =
      ReloadPageToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kReloadPage);
  EXPECT_EQ(definition->name, ReloadPageToolRequest::kModelFacingName);
  EXPECT_THAT(definition->parameters_json_schema,
              base::test::DictionaryHasValue("properties",
                                             base::Value(base::DictValue())));
}

TEST(ToolRegistryTest, SeekMediaToolDefinition) {
  const std::optional<ToolDefinition> definition =
      SeekMediaToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kSeekToTimestamp);
  EXPECT_EQ(definition->name, SeekMediaToolRequest::kModelFacingName);
  EXPECT_THAT(*definition,
              HasParamOfType(SeekMediaToolRequest::kTimecodeParam, "string"));
  EXPECT_THAT(*definition, RequiresParam(SeekMediaToolRequest::kTimecodeParam));
}

TEST(ToolRegistryTest, PauseMediaToolDefinition) {
  const std::optional<ToolDefinition> definition =
      PauseMediaToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kPauseVideo);
  EXPECT_EQ(definition->name, PauseMediaToolRequest::kModelFacingName);
  EXPECT_THAT(definition->parameters_json_schema,
              base::test::DictionaryHasValue("properties",
                                             base::Value(base::DictValue())));
}

TEST(ToolRegistryTest, PlayMediaToolDefinition) {
  const std::optional<ToolDefinition> definition =
      PlayMediaToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kPlayVideo);
  EXPECT_EQ(definition->name, PlayMediaToolRequest::kModelFacingName);
  EXPECT_THAT(definition->parameters_json_schema,
              base::test::DictionaryHasValue("properties",
                                             base::Value(base::DictValue())));
}

TEST(ToolRegistryTest, PerformSearchToolDefinition) {
  const std::optional<ToolDefinition> definition =
      PerformSearchToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kPerformSearch);
  EXPECT_EQ(definition->name, PerformSearchToolRequest::kModelFacingName);
  EXPECT_THAT(*definition,
              HasParamOfType(PerformSearchToolRequest::kQueryParam, "string"));
  EXPECT_THAT(*definition,
              RequiresParam(PerformSearchToolRequest::kQueryParam));
}

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

TEST(ToolRegistryTest, GetAllToolsContainsScrollTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kScroll, &ToolDefinition::id));
}
TEST(ToolRegistryTest, GetAllToolsContainsSelectOptionTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kSelectOption,
                                    &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsTypeTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kType, &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsTranslatePageTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kTranslatePage,
                                    &ToolDefinition::id));
}
TEST(ToolRegistryTest, GetAllToolsContainsHistoryBackTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kGoBack, &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsHistoryForwardTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kGoForward, &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsReloadPageTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kReloadPage, &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsSeekMediaTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kSeekToTimestamp,
                                    &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsPauseMediaTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kPauseVideo, &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsPlayMediaTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kPlayVideo, &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsPerformSearchTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kPerformSearch,
                                    &ToolDefinition::id));
}

#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
TEST(ToolRegistryTest, ExitFullscreenToolDefinition) {
  const std::optional<ToolDefinition> definition =
      ExitFullscreenToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kExitFullscreen);
  EXPECT_EQ(definition->name, ExitFullscreenToolRequest::kModelFacingName);
  EXPECT_THAT(definition->parameters_json_schema,
              base::test::DictionaryHasValue("properties",
                                             base::Value(base::DictValue())));
}

TEST(ToolRegistryTest, GetAllToolsContainsExitFullscreenTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kExitFullscreen,
                                    &ToolDefinition::id));
}

TEST(ToolRegistryTest, OpenKnownPageToolDefinition) {
  const std::optional<ToolDefinition> definition =
      OpenKnownPageToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kOpenKnownPage);
  EXPECT_EQ(definition->name, OpenKnownPageToolRequest::kModelFacingName);
  EXPECT_THAT(*definition,
              HasParamOfType(OpenKnownPageToolRequest::kQueryParam, "string"));
  EXPECT_THAT(*definition,
              RequiresParam(OpenKnownPageToolRequest::kQueryParam));
}

TEST(ToolRegistryTest, GetAllToolsContainsOpenKnownPageTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kOpenKnownPage,
                                    &ToolDefinition::id));
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
