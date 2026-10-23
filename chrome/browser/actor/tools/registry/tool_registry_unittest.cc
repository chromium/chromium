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
#include "chrome/browser/actor/tools/autofill/attempt_form_filling/attempt_form_filling_tool_request.h"
#include "chrome/browser/actor/tools/bookmark_management_tool_request.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/drag_and_release_tool_request.h"
#include "chrome/browser/actor/tools/file_upload_tool_request.h"
#include "chrome/browser/actor/tools/find_and_highlight_tool_request.h"
#include "chrome/browser/actor/tools/gemini_panel_tool_request.h"
#include "chrome/browser/actor/tools/history_tool_request.h"
#include "chrome/browser/actor/tools/media_control_tool_request.h"
#include "chrome/browser/actor/tools/move_mouse_tool_request.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/tools/one_time_token/attempt_otp_filling_tool_request.h"
#include "chrome/browser/actor/tools/open_known_page_tool_request.h"
#include "chrome/browser/actor/tools/open_url_tool_request.h"
#include "chrome/browser/actor/tools/password_manager/attempt_login/attempt_login_tool_request.h"
#include "chrome/browser/actor/tools/perform_search_tool_request.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_test_util.h"
#include "chrome/browser/actor/tools/script_tool_request.h"
#include "chrome/browser/actor/tools/scroll_to_tool_request.h"
#include "chrome/browser/actor/tools/scroll_tool_request.h"
#include "chrome/browser/actor/tools/select_tool_request.h"
#include "chrome/browser/actor/tools/switch_tab_tool_request.h"
#include "chrome/browser/actor/tools/tab_management_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/translate_page_tool_request.h"
#include "chrome/browser/actor/tools/type_tool_request.h"
#include "chrome/browser/actor/tools/wait_tool_request.h"
#include "chrome/browser/actor/tools/window_management_tool_request.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#include "chrome/browser/actor/tools/load_and_extract_content_tool_request.h"
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

TEST(ToolRegistryTest, AttemptFormFillingToolDefinition) {
  const std::optional<ToolDefinition> definition =
      AttemptFormFillingToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kAttemptFormFilling);
  EXPECT_EQ(definition->name, AttemptFormFillingToolRequest::kModelFacingName);
  EXPECT_THAT(*definition,
              HasParamOfType(AttemptFormFillingToolRequest::kDomNodeIdParam,
                             "integer"));
  EXPECT_THAT(*definition,
              RequiresParam(AttemptFormFillingToolRequest::kDomNodeIdParam));
  EXPECT_THAT(*definition,
              HasParamOfType(AttemptFormFillingToolRequest::kRequestedDataParam,
                             "string"));
  EXPECT_THAT(
      *definition,
      RequiresParam(AttemptFormFillingToolRequest::kRequestedDataParam));

  const base::DictValue* properties =
      definition->parameters_json_schema.FindDict("properties");
  ASSERT_TRUE(properties);
  const base::DictValue* requested_data_prop =
      properties->FindDict(AttemptFormFillingToolRequest::kRequestedDataParam);
  ASSERT_TRUE(requested_data_prop);
  const base::ListValue* enum_list = requested_data_prop->FindList("enum");
  ASSERT_TRUE(enum_list);
  std::vector<std::string> actual_enum;
  for (const base::Value& item : *enum_list) {
    ASSERT_TRUE(item.is_string());
    actual_enum.push_back(item.GetString());
  }
  EXPECT_THAT(actual_enum,
              testing::ElementsAreArray(
                  AttemptFormFillingToolRequest::kRequestedDataValues));
}

TEST(ToolRegistryTest, AttemptLoginToolDefinition) {
  const std::optional<ToolDefinition> definition =
      AttemptLoginToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kAttemptLogin);
  EXPECT_EQ(definition->name, AttemptLoginToolRequest::kModelFacingName);
  EXPECT_THAT(definition->parameters_json_schema,
              base::test::DictionaryHasValue("properties",
                                             base::Value(base::DictValue())));
}

TEST(ToolRegistryTest, AttemptOtpFillingToolDefinition) {
  const std::optional<ToolDefinition> definition =
      AttemptOtpFillingToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kAttemptOtpFilling);
  EXPECT_EQ(definition->name, AttemptOtpFillingToolRequest::kModelFacingName);
  EXPECT_THAT(
      *definition,
      HasParamOfType(AttemptOtpFillingToolRequest::kTriggerFieldDomNodeIdsParam,
                     "array"));
  EXPECT_THAT(*definition,
              RequiresParam(
                  AttemptOtpFillingToolRequest::kTriggerFieldDomNodeIdsParam));
  EXPECT_THAT(
      *definition,
      HasParamOfType(AttemptOtpFillingToolRequest::kForSigninParam, "boolean"));
  EXPECT_THAT(*definition,
              RequiresParam(AttemptOtpFillingToolRequest::kForSigninParam));
}

TEST(ToolRegistryTest, EnterFullscreenToolDefinition) {
  const std::optional<ToolDefinition> definition =
      EnterFullscreenToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kEnterFullscreen);
  EXPECT_EQ(definition->name, EnterFullscreenToolRequest::kModelFacingName);
  EXPECT_THAT(definition->parameters_json_schema,
              base::test::DictionaryHasValue("properties",
                                             base::Value(base::DictValue())));
}

TEST(ToolRegistryTest, CreateTabToolDefinition) {
  const std::optional<ToolDefinition> definition =
      CreateTabToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kCreateTab);
  EXPECT_EQ(definition->name, CreateTabToolRequest::kModelFacingName);
  EXPECT_THAT(definition->parameters_json_schema,
              base::test::DictionaryHasValue("properties",
                                             base::Value(base::DictValue())));
}

TEST(ToolRegistryTest, ActivateTabToolDefinition) {
  const std::optional<ToolDefinition> definition =
      ActivateTabToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kActivateTab);
  EXPECT_EQ(definition->name, ActivateTabToolRequest::kModelFacingName);
  EXPECT_THAT(*definition,
              HasParamOfType(ActivateTabToolRequest::kTabIdParam, "integer"));
  EXPECT_THAT(*definition, RequiresParam(ActivateTabToolRequest::kTabIdParam));
}
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
TEST(ToolRegistryTest, CreateWindowToolDefinition) {
  const std::optional<ToolDefinition> definition =
      CreateWindowToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kCreateWindow);
  EXPECT_EQ(definition->name, CreateWindowToolRequest::kModelFacingName);
  EXPECT_THAT(definition->parameters_json_schema,
              base::test::DictionaryHasValue("properties",
                                             base::Value(base::DictValue())));
}

TEST(ToolRegistryTest, ActivateWindowToolDefinition) {
  const std::optional<ToolDefinition> definition =
      ActivateWindowToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kActivateWindow);
  EXPECT_EQ(definition->name, ActivateWindowToolRequest::kModelFacingName);
  EXPECT_THAT(
      *definition,
      HasParamOfType(ActivateWindowToolRequest::kWindowIdParam, "integer"));
  EXPECT_THAT(*definition,
              RequiresParam(ActivateWindowToolRequest::kWindowIdParam));
}

TEST(ToolRegistryTest, CloseWindowToolDefinition) {
  const std::optional<ToolDefinition> definition =
      CloseWindowToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kCloseWindow);
  EXPECT_EQ(definition->name, CloseWindowToolRequest::kModelFacingName);
  EXPECT_THAT(
      *definition,
      HasParamOfType(CloseWindowToolRequest::kWindowIdParam, "integer"));
  EXPECT_THAT(*definition,
              RequiresParam(CloseWindowToolRequest::kWindowIdParam));
}

TEST(ToolRegistryTest, LoadAndExtractContentToolDefinition) {
  const std::optional<ToolDefinition> definition =
      LoadAndExtractContentToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kLoadAndExtractContent);
  EXPECT_EQ(definition->name,
            LoadAndExtractContentToolRequest::kModelFacingName);
  EXPECT_THAT(
      *definition,
      HasParamOfType(LoadAndExtractContentToolRequest::kUrlsParam, "array"));
  EXPECT_THAT(*definition,
              RequiresParam(LoadAndExtractContentToolRequest::kUrlsParam));
}
#endif

TEST(ToolRegistryTest, FindAndHighlightToolDefinition) {
  const std::optional<ToolDefinition> definition =
      FindAndHighlightToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kFindAndHighlight);
  EXPECT_EQ(definition->name, FindAndHighlightToolRequest::kModelFacingName);
  EXPECT_THAT(
      *definition,
      HasParamOfType(FindAndHighlightToolRequest::kQueryParam, "string"));
  EXPECT_THAT(*definition,
              RequiresParam(FindAndHighlightToolRequest::kQueryParam));
}

TEST(ToolRegistryTest, CloseTabToolDefinition) {
  const std::optional<ToolDefinition> definition =
      CloseTabToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kCloseTab);
  EXPECT_EQ(definition->name, CloseTabToolRequest::kModelFacingName);
  EXPECT_THAT(definition->parameters_json_schema,
              base::test::DictionaryHasValue("properties",
                                             base::Value(base::DictValue())));
}

TEST(ToolRegistryTest, FileUploadToolDefinition) {
  const std::optional<ToolDefinition> definition =
      FileUploadToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kFileUpload);
  EXPECT_EQ(definition->name, FileUploadToolRequest::kModelFacingName);
  EXPECT_THAT(
      *definition,
      HasParamOfType(FileUploadToolRequest::kDomNodeIdParam, "integer"));
  EXPECT_THAT(*definition,
              RequiresParam(FileUploadToolRequest::kDomNodeIdParam));
  EXPECT_THAT(*definition,
              HasParamOfType(FileUploadToolRequest::kFileUrlsParam, "array"));
  EXPECT_THAT(*definition,
              RequiresParam(FileUploadToolRequest::kFileUrlsParam));
}

TEST(ToolRegistryTest, ScrollToToolDefinition) {
  const std::optional<ToolDefinition> definition =
      ScrollToToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kScrollTo);
  EXPECT_EQ(definition->name, ScrollToToolRequest::kModelFacingName);
  EXPECT_THAT(*definition,
              HasParamOfType(ScrollToToolRequest::kDomNodeIdParam, "integer"));
  EXPECT_THAT(*definition, RequiresParam(ScrollToToolRequest::kDomNodeIdParam));
}

TEST(ToolRegistryTest, AddBookmarkToolDefinition) {
  const std::optional<ToolDefinition> definition =
      AddBookmarkToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kAddBookmark);
  EXPECT_EQ(definition->name, AddBookmarkToolRequest::kModelFacingName);
  EXPECT_THAT(*definition,
              HasParamOfType(AddBookmarkToolRequest::kUrlParam, "string"));
  EXPECT_THAT(*definition, RequiresParam(AddBookmarkToolRequest::kUrlParam));
  EXPECT_THAT(*definition,
              HasParamOfType(AddBookmarkToolRequest::kTitleParam, "string"));
  EXPECT_THAT(*definition, RequiresParam(AddBookmarkToolRequest::kTitleParam));
}

TEST(ToolRegistryTest, RemoveBookmarkToolDefinition) {
  const std::optional<ToolDefinition> definition =
      RemoveBookmarkToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kRemoveBookmark);
  EXPECT_EQ(definition->name, RemoveBookmarkToolRequest::kModelFacingName);
  EXPECT_THAT(*definition,
              HasParamOfType(RemoveBookmarkToolRequest::kUrlParam, "string"));
  EXPECT_THAT(*definition, RequiresParam(RemoveBookmarkToolRequest::kUrlParam));
}

TEST(ToolRegistryTest, ScrollPageToolDefinition) {
  const std::optional<ToolDefinition> definition =
      ScrollPageToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kScrollPage);
  EXPECT_EQ(definition->name, ScrollPageToolRequest::kModelFacingName);
  EXPECT_THAT(
      *definition,
      HasParamOfType(ScrollPageToolRequest::kGranularityParam, "string"));
  EXPECT_THAT(*definition,
              RequiresParam(ScrollPageToolRequest::kGranularityParam));
  EXPECT_THAT(*definition,
              HasParamOfType(ScrollPageToolRequest::kDirectionParam, "string"));
  EXPECT_THAT(*definition,
              RequiresParam(ScrollPageToolRequest::kDirectionParam));
  EXPECT_THAT(
      *definition,
      HasParamOfType(ScrollPageToolRequest::kDomNodeIdParam, "integer"));
  EXPECT_THAT(*definition,
              RequiresParam(ScrollPageToolRequest::kDomNodeIdParam));
}

TEST(ToolRegistryTest, MoveMouseToolDefinition) {
  const std::optional<ToolDefinition> definition =
      MoveMouseToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kMoveMouse);
  EXPECT_EQ(definition->name, MoveMouseToolRequest::kModelFacingName);
  EXPECT_THAT(*definition,
              HasParamOfType(MoveMouseToolRequest::kDomNodeIdParam, "integer"));
  EXPECT_THAT(*definition,
              RequiresParam(MoveMouseToolRequest::kDomNodeIdParam));
}
TEST(ToolRegistryTest, DragAndReleaseToolDefinition) {
  const std::optional<ToolDefinition> definition =
      DragAndReleaseToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kDragAndRelease);
  EXPECT_EQ(definition->name, DragAndReleaseToolRequest::kModelFacingName);
  EXPECT_THAT(*definition,
              HasParamOfType(DragAndReleaseToolRequest::kFromDomNodeIdParam,
                             "integer"));
  EXPECT_THAT(*definition,
              RequiresParam(DragAndReleaseToolRequest::kFromDomNodeIdParam));
  EXPECT_THAT(
      *definition,
      HasParamOfType(DragAndReleaseToolRequest::kToDomNodeIdParam, "integer"));
  EXPECT_THAT(*definition,
              RequiresParam(DragAndReleaseToolRequest::kToDomNodeIdParam));
}
TEST(ToolRegistryTest, WaitToolDefinition) {
  const std::optional<ToolDefinition> definition =
      WaitToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kWait);
  EXPECT_EQ(definition->name, WaitToolRequest::kModelFacingName);
  EXPECT_THAT(*definition,
              HasParamOfType(WaitToolRequest::kWaitTimeMsParam, "integer"));
  EXPECT_THAT(*definition, RequiresParam(WaitToolRequest::kWaitTimeMsParam));
}
TEST(ToolRegistryTest, ScriptToolDefinition) {
  const std::optional<ToolDefinition> definition =
      ScriptToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kScript);
  EXPECT_EQ(definition->name, ScriptToolRequest::kModelFacingName);
  EXPECT_THAT(
      *definition,
      HasParamOfType(ScriptToolRequest::kDocumentIdentifierParam, "string"));
  EXPECT_THAT(*definition,
              RequiresParam(ScriptToolRequest::kDocumentIdentifierParam));
  EXPECT_THAT(*definition,
              HasParamOfType(ScriptToolRequest::kToolNameParam, "string"));
  EXPECT_THAT(*definition, RequiresParam(ScriptToolRequest::kToolNameParam));
  EXPECT_THAT(
      *definition,
      HasParamOfType(ScriptToolRequest::kInputArgumentsParam, "string"));
  EXPECT_THAT(*definition,
              RequiresParam(ScriptToolRequest::kInputArgumentsParam));
}
TEST(ToolRegistryTest, PromptGeminiPanelToolDefinition) {
  const std::optional<ToolDefinition> definition =
      PromptGeminiPanelToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kPromptGeminiPanel);
  EXPECT_EQ(definition->name, PromptGeminiPanelToolRequest::kModelFacingName);
  EXPECT_THAT(
      *definition,
      HasParamOfType(PromptGeminiPanelToolRequest::kPromptParam, "string"));
  EXPECT_THAT(*definition,
              RequiresParam(PromptGeminiPanelToolRequest::kPromptParam));
}

TEST(ToolRegistryTest, CloseGeminiPanelToolDefinition) {
  const std::optional<ToolDefinition> definition =
      CloseGeminiPanelToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kCloseGeminiPanel);
  EXPECT_EQ(definition->name, CloseGeminiPanelToolRequest::kModelFacingName);
  EXPECT_THAT(definition->parameters_json_schema,
              base::test::DictionaryHasValue("properties",
                                             base::Value(base::DictValue())));
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

TEST(ToolRegistryTest, GetAllToolsContainsSwitchTabTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kSwitchTab, &ToolDefinition::id));
}

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

TEST(ToolRegistryTest, GetAllToolsContainsAttemptFormFillingTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kAttemptFormFilling,
                                    &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsAttemptLoginTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kAttemptLogin,
                                    &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsAttemptOtpFillingTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kAttemptOtpFilling,
                                    &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsFileUploadTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kFileUpload, &ToolDefinition::id));
}

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

TEST(ToolRegistryTest, OpenUrlToolDefinition) {
  const std::optional<ToolDefinition> definition =
      OpenUrlToolRequest::GetToolDefinition();
  ASSERT_TRUE(definition.has_value());
  EXPECT_EQ(definition->id, ToolId::kOpenUrl);
  EXPECT_EQ(definition->name, OpenUrlToolRequest::kModelFacingName);
  EXPECT_THAT(*definition,
              HasParamOfType(OpenUrlToolRequest::kUrlParam, "string"));
  EXPECT_THAT(*definition, RequiresParam(OpenUrlToolRequest::kUrlParam));
  EXPECT_THAT(*definition,
              HasParamOfType(OpenUrlToolRequest::kNewTabParam, "boolean"));
  EXPECT_THAT(*definition, RequiresParam(OpenUrlToolRequest::kNewTabParam));
}

TEST(ToolRegistryTest, GetAllToolsContainsOpenUrlTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  ASSERT_TRUE(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kOpenUrl, &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsEnterFullscreenTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kEnterFullscreen,
                                    &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsCreateTabTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kCreateTab, &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsActivateTabTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kActivateTab, &ToolDefinition::id));
}
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
TEST(ToolRegistryTest, GetAllToolsContainsCreateWindowTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kCreateWindow,
                                    &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsActivateWindowTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kActivateWindow,
                                    &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsCloseWindowTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kCloseWindow, &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsLoadAndExtractContentTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kLoadAndExtractContent,
                                    &ToolDefinition::id));
}
#endif

TEST(ToolRegistryTest, GetAllToolsContainsFindAndHighlightTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kFindAndHighlight,
                                    &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsPromptGeminiPanelTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kPromptGeminiPanel,
                                    &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsCloseGeminiPanelTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kCloseGeminiPanel,
                                    &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsCloseTabTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kCloseTab, &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsScrollToTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kScrollTo, &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsAddBookmarkTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kAddBookmark, &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsRemoveBookmarkTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kRemoveBookmark,
                                    &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsScrollPageTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kScrollPage, &ToolDefinition::id));
}

TEST(ToolRegistryTest, GetAllToolsContainsMoveMouseTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kMoveMouse, &ToolDefinition::id));
}
TEST(ToolRegistryTest, GetAllToolsContainsDragAndReleaseTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kDragAndRelease,
                                    &ToolDefinition::id));
}
TEST(ToolRegistryTest, GetAllToolsContainsWaitTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kWait, &ToolDefinition::id));
}
TEST(ToolRegistryTest, GetAllToolsContainsScriptTool) {
  base::test::ScopedFeatureList scoped_feature_list(features::kGlicActor);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ActorKeyedService* service = ActorKeyedService::Get(&profile);
  CHECK(service);

  EXPECT_TRUE(std::ranges::contains(service->tool_registry().GetAllTools(),
                                    ToolId::kScript, &ToolDefinition::id));
}
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
