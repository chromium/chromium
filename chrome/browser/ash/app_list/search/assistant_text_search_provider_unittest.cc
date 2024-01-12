// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/assistant_text_search_provider.h"

#include <string>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/assistant/test_support/mock_assistant_controller.h"
#include "ash/public/cpp/assistant/test_support/mock_assistant_state.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_list/app_list_test_util.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace app_list::test {
namespace {

using ::ash::assistant::AssistantAllowedState;

class AssistantTextSearchProviderTest : public AppListTestBase {
 public:
  AssistantTextSearchProviderTest() {
    auto search_provider = std::make_unique<AssistantTextSearchProvider>();
    search_provider_ = search_provider.get();
    search_controller_.AddProvider(std::move(search_provider));

    // This test suite will be deleted after the Launcher Search IPH is enabled.
    feature_list_.InitAndDisableFeature(
        feature_engagement::kIPHLauncherSearchHelpUiFeature);
  }
  AssistantTextSearchProviderTest(const AssistantTextSearchProviderTest&) =
      delete;
  AssistantTextSearchProviderTest& operator=(
      const AssistantTextSearchProviderTest&) = delete;
  ~AssistantTextSearchProviderTest() override = default;

  void SendText(const std::string& text) {
    search_controller_.StartSearch(base::UTF8ToUTF16(text));
  }

  ash::MockAssistantState& assistant_state() { return assistant_state_; }

  testing::NiceMock<ash::MockAssistantController>& assistant_controller() {
    return assistant_controller_;
  }

  const SearchProvider::Results& LastResults() {
    return search_controller_.last_results();
  }

  void VerifyResultAt(size_t index, const std::string& text) {
    EXPECT_LT(index, LastResults().size());
    auto* result = LastResults().at(0).get();
    EXPECT_EQ(result->title(), base::UTF8ToUTF16(text));
    EXPECT_EQ(result->details(), u"Google Assistant");
    EXPECT_EQ(result->id(), "googleassistant_text://" + text);
    EXPECT_EQ(result->accessible_name(),
              base::UTF8ToUTF16(text + ", Google Assistant"));
    EXPECT_EQ(result->result_type(),
              ash::AppListSearchResultType::kAssistantText);
    EXPECT_EQ(result->display_type(), ash::SearchResultDisplayType::kList);
    EXPECT_EQ(result->skip_update_animation(), true);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  ash::MockAssistantState assistant_state_;
  testing::NiceMock<ash::MockAssistantController> assistant_controller_;
  TestSearchController search_controller_;
  raw_ptr<AssistantTextSearchProvider> search_provider_ = nullptr;
};

// Tests -----------------------------------------------------------------------

TEST_F(AssistantTextSearchProviderTest,
       ShouldUpdateResultsWhenAssistantSettingsChange) {
  SendText("testing");
  EXPECT_EQ(LastResults().size(), 1u);

  assistant_state().SetSettingsEnabled(false);
  EXPECT_TRUE(LastResults().empty());

  assistant_state().SetSettingsEnabled(true);
  EXPECT_EQ(LastResults().size(), 1u);
}

TEST_F(AssistantTextSearchProviderTest,
       ShouldUpdateResultsWhenAssistantAllowedStateChanges) {
  SendText("testing");

  // Test all possible Assistant allowed states.
  for (int i = 0; i < static_cast<int>(AssistantAllowedState::MAX_VALUE); ++i) {
    if (i == static_cast<int>(AssistantAllowedState::ALLOWED))
      continue;

    // When Assistant becomes not-allowed, results should be cleared.
    assistant_state().SetAllowedState(static_cast<AssistantAllowedState>(i));
    EXPECT_TRUE(LastResults().empty());

    // When Assistant becomes allowed, we should again have a single result.
    assistant_state().SetAllowedState(AssistantAllowedState::ALLOWED);
    EXPECT_EQ(1u, LastResults().size());
  }
}
TEST_F(AssistantTextSearchProviderTest, ShouldDeepLinkAssistantQuery) {
  SendText("testing query");

  GURL url;
  bool in_background = true;
  bool from_user = true;
  EXPECT_CALL(assistant_controller(), OpenUrl)
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&url),
                               testing::SaveArg<1>(&in_background),
                               testing::SaveArg<2>(&from_user)));

  LastResults().at(0)->Open(/*event_flags=*/0);
  EXPECT_EQ(url, GURL("googleassistant://send-query?q=testing+query"));
  EXPECT_FALSE(in_background);
  EXPECT_FALSE(from_user);
}

}  // namespace
}  // namespace app_list::test
