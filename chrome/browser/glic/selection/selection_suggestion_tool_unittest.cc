// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/selection_suggestion_tool.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/selection/selection_suggestion.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_prefs.h"
#include "chrome/browser/selection/suggestion.h"
#include "chrome/browser/selection/suggestion_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace glic {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::ReturnRef;

class SelectionSuggestionToolTest : public testing::Test {
 public:
  SelectionSuggestionToolTest() {
    ON_CALL(mock_tab_, GetProfile()).WillByDefault(Return(&profile_));
    ON_CALL(mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
  }
  ~SelectionSuggestionToolTest() override = default;

 protected:
  TestingProfile& profile() { return profile_; }
  tabs::MockTabInterface& mock_tab() { return mock_tab_; }
  ::selection::SuggestionService& service() { return service_; }
  SelectionSuggestionTool& tool() { return tool_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  ui::UnownedUserDataHost user_data_host_;
  tabs::MockTabInterface mock_tab_;
  ::selection::SuggestionService service_{&mock_tab_,
                                          /*remote_model_executor=*/nullptr,
                                          /*google_groups_manager=*/nullptr};
  SelectionSuggestionTool tool_{mock_tab_};
};

// Tests that SelectionSuggestionTool returns the correct ToolId.
TEST_F(SelectionSuggestionToolTest, GetToolId) {
  EXPECT_EQ(tool().GetToolId(),
            optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
}

// Tests that GetCapabilities returns empty when no supported tools are cached.
TEST_F(SelectionSuggestionToolTest, GetCapabilitiesDefaultEmpty) {
  EXPECT_THAT(tool().GetCapabilities(), IsEmpty());
}

// Tests that GetCapabilities returns the supported tools from profile prefs.
TEST_F(SelectionSuggestionToolTest,
       GetCapabilitiesReturnsSupportedToolsFromPrefs) {
  profile().GetPrefs()->SetList(
      contextual_cueing::prefs::kZeroStateSuggestionsSupportedTools,
      base::ListValue().Append("youtube_tool").Append("workspace_tool"));

  EXPECT_THAT(tool().GetCapabilities(),
              ElementsAre("youtube_tool", "workspace_tool"));
}

// Tests that GetCapabilities falls back to profile prefs when the feature is
// enabled without setting the capabilities parameter.
TEST_F(SelectionSuggestionToolTest,
       GetCapabilitiesFeatureEnabledWithoutParamUsesPrefs) {
  base::test::ScopedFeatureList feature_list{
      features::kGlicSelectionSuggestions};
  profile().GetPrefs()->SetList(
      contextual_cueing::prefs::kZeroStateSuggestionsSupportedTools,
      base::ListValue().Append("youtube_tool"));

  EXPECT_THAT(tool().GetCapabilities(), ElementsAre("youtube_tool"));
}

// Tests that GetCapabilities returns the tools from the feature parameter when
// set, overriding profile prefs.
TEST_F(SelectionSuggestionToolTest, GetCapabilitiesFeatureParamOverridesPrefs) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kGlicSelectionSuggestions,
      {{features::kGlicSelectionSuggestionsCapabilities.name,
        "youtube_tool, workspace_tool"}});
  profile().GetPrefs()->SetList(
      contextual_cueing::prefs::kZeroStateSuggestionsSupportedTools,
      base::ListValue().Append("other_tool"));

  EXPECT_THAT(tool().GetCapabilities(),
              ElementsAre("youtube_tool", "workspace_tool"));
}

// Tests that GetCapabilities returns empty when the feature parameter is
// explicitly set to empty, overriding non-empty profile prefs.
TEST_F(SelectionSuggestionToolTest,
       GetCapabilitiesEmptyFeatureParamOverridesPrefs) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kGlicSelectionSuggestions,
      {{features::kGlicSelectionSuggestionsCapabilities.name, ""}});
  profile().GetPrefs()->SetList(
      contextual_cueing::prefs::kZeroStateSuggestionsSupportedTools,
      base::ListValue().Append("youtube_tool"));

  EXPECT_THAT(tool().GetCapabilities(), IsEmpty());
}

// Tests that SelectionSuggestionTool registers itself on construction and
// unregisters itself on destruction.
TEST_F(SelectionSuggestionToolTest, RegistersAndUnregistersWithService) {
  ui::UnownedUserDataHost other_host;
  tabs::MockTabInterface other_tab;
  ON_CALL(other_tab, GetUnownedUserDataHost())
      .WillByDefault(ReturnRef(other_host));
  ::selection::SuggestionService other_service{
      &other_tab, /*remote_model_executor=*/nullptr,
      /*google_groups_manager=*/nullptr};

  {
    const SelectionSuggestionTool scoped_tool{other_tab};
  }

  // Re-registering with the same ToolId succeeds because `scoped_tool`
  // unregistered itself on destruction.
  const SelectionSuggestionTool second_tool{other_tab};
}

// Tests that CreateSuggestion creates a SelectionSuggestion for non-empty
// labels.
TEST_F(SelectionSuggestionToolTest, CreateSuggestionWithValidLabel) {
  const ::selection::AreaOfInterest aoi;
  optimization_guide::proto::SmartSelectionSuggestion server_suggestion;
  server_suggestion.set_label("Ask Glic");

  const std::unique_ptr<::selection::Suggestion> suggestion =
      tool().CreateSuggestion(aoi, server_suggestion);
  ASSERT_NE(suggestion, nullptr);
  EXPECT_EQ(suggestion->GetLabel(), u"Ask Glic");
  EXPECT_EQ(suggestion->GetToolId(),
            optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  const SelectionSuggestion* const selection_suggestion =
      static_cast<const SelectionSuggestion*>(suggestion.get());
  EXPECT_EQ(selection_suggestion->prompt(), "Ask Glic");
}

// Tests that CreateSuggestion returns nullptr when the server label is empty.
TEST_F(SelectionSuggestionToolTest,
       CreateSuggestionWithEmptyLabelReturnsNullptr) {
  const ::selection::AreaOfInterest aoi;
  optimization_guide::proto::SmartSelectionSuggestion server_suggestion;
  server_suggestion.set_label("");

  EXPECT_EQ(tool().CreateSuggestion(aoi, server_suggestion), nullptr);
}

}  // namespace
}  // namespace glic
