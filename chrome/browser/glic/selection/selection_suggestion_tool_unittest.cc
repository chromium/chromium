// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/selection_suggestion_tool.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/test_future.h"
#include "chrome/browser/glic/selection/selection_suggestion.h"
#include "chrome/browser/selection/suggestion.h"
#include "chrome/browser/selection/suggestion_service.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace glic {
namespace {

using ::base::test::TestFuture;
using ::testing::IsEmpty;
using ::testing::ReturnRef;

class SelectionSuggestionToolTest : public testing::Test {
 public:
  SelectionSuggestionToolTest() {
    ON_CALL(mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
  }
  ~SelectionSuggestionToolTest() override = default;

 protected:
  tabs::MockTabInterface& mock_tab() { return mock_tab_; }
  ::selection::SuggestionService& service() { return service_; }
  SelectionSuggestionTool& tool() { return tool_; }

 private:
  ui::UnownedUserDataHost user_data_host_;
  tabs::MockTabInterface mock_tab_;
  ::selection::SuggestionService service_{&mock_tab_,
                                          /*remote_model_executor=*/nullptr};
  SelectionSuggestionTool tool_{mock_tab_};
};

// Tests that SelectionSuggestionTool returns the correct ToolId.
TEST_F(SelectionSuggestionToolTest, GetToolId) {
  EXPECT_EQ(tool().GetToolId(),
            optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
}

// Tests that SelectionSuggestionTool supports server suggestions.
TEST_F(SelectionSuggestionToolTest, SupportsServerSuggestions) {
  EXPECT_TRUE(tool().SupportsServerSuggestions());
}

// Tests that SelectionSuggestionTool registers itself on construction and
// unregisters itself on destruction.
TEST_F(SelectionSuggestionToolTest, RegistersAndUnregistersWithService) {
  ui::UnownedUserDataHost other_host;
  tabs::MockTabInterface other_tab;
  ON_CALL(other_tab, GetUnownedUserDataHost())
      .WillByDefault(ReturnRef(other_host));
  ::selection::SuggestionService other_service{
      &other_tab, /*remote_model_executor=*/nullptr};

  {
    const SelectionSuggestionTool scoped_tool{other_tab};
  }

  // Re-registering with the same ToolId succeeds because `scoped_tool`
  // unregistered itself on destruction.
  const SelectionSuggestionTool second_tool{other_tab};
}

// Tests that SelectionSuggestionTool generates no static suggestions.
TEST_F(SelectionSuggestionToolTest, RequestSuggestionsReturnsEmpty) {
  const ::selection::AreaOfInterest aoi;
  TestFuture<std::vector<std::unique_ptr<::selection::Suggestion>>, bool>
      future;
  tool().RequestSuggestions(aoi, future.GetRepeatingCallback());

  const auto& [suggestions, complete] = future.Get();
  EXPECT_TRUE(complete);
  EXPECT_THAT(suggestions, IsEmpty());
}

// Tests that CreateSuggestion creates a SelectionSuggestion for non-empty
// labels.
TEST_F(SelectionSuggestionToolTest, CreateSuggestionWithValidLabel) {
  optimization_guide::proto::SmartSelectionSuggestion server_suggestion;
  server_suggestion.set_label("Ask Glic");

  const std::unique_ptr<::selection::Suggestion> suggestion =
      tool().CreateSuggestion(server_suggestion);
  ASSERT_NE(suggestion, nullptr);
  EXPECT_EQ(suggestion->GetLabel(), u"Ask Glic");
  const SelectionSuggestion* const selection_suggestion =
      static_cast<const SelectionSuggestion*>(suggestion.get());
  EXPECT_EQ(selection_suggestion->prompt(), "Ask Glic");
}

// Tests that CreateSuggestion returns nullptr when the server label is empty.
TEST_F(SelectionSuggestionToolTest,
       CreateSuggestionWithEmptyLabelReturnsNullptr) {
  optimization_guide::proto::SmartSelectionSuggestion server_suggestion;
  server_suggestion.set_label("");

  EXPECT_EQ(tool().CreateSuggestion(server_suggestion), nullptr);
}

}  // namespace
}  // namespace glic
