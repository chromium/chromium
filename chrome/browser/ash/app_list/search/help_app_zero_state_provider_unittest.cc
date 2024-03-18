// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/help_app_zero_state_provider.h"

#include <memory>
#include <string>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/app_list_notifier_impl.h"
#include "chrome/browser/ash/app_list/app_list_test_util.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "chrome/browser/ash/app_list/test/test_app_list_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace app_list::test {

namespace {

constexpr char kReleaseNotesResultId[] = "help-app://updates";

void ExpectReleaseNotesChip(ChromeSearchResult* result,
                            int title_id,
                            ash::SearchResultDisplayType display_type) {
  EXPECT_EQ(kReleaseNotesResultId, result->id());
  EXPECT_EQ(l10n_util::GetStringUTF16(title_id), result->title());
  EXPECT_EQ(ash::AppListSearchResultType::kZeroStateHelpApp,
            result->result_type());
  EXPECT_EQ(display_type, result->display_type());
}

}  // namespace

class HelpAppZeroStateProviderTest : public AppListTestBase {
 public:
  HelpAppZeroStateProviderTest() = default;
  ~HelpAppZeroStateProviderTest() override = default;

  void SetUp() override {
    AppListTestBase::SetUp();

    app_list_notifier_ =
        std::make_unique<AppListNotifierImpl>(&app_list_controller_);

    auto provider = std::make_unique<HelpAppZeroStateProvider>(
        profile(), app_list_notifier_.get());
    provider_ = provider.get();
    search_controller_.AddProvider(std::move(provider));
  }

  void StartZeroStateSearch() {
    search_controller_.StartZeroState(base::DoNothing(), base::TimeDelta());
  }

  const app_list::Results& GetLatestResults() {
    return search_controller_.last_results();
  }

  ::test::TestAppListController* app_list_controller() {
    return &app_list_controller_;
  }

  ash::AppListNotifier* app_list_notifier() { return app_list_notifier_.get(); }

 private:
  ::test::TestAppListController app_list_controller_;
  std::unique_ptr<ash::AppListNotifier> app_list_notifier_;
  TestSearchController search_controller_;
  raw_ptr<HelpAppZeroStateProvider> provider_ = nullptr;
};

// Test for empty query.
TEST_F(HelpAppZeroStateProviderTest,
       HasNoResultsForEmptyQueryIfTimesLeftToShowIsZero) {
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 0);

  StartZeroStateSearch();

  EXPECT_TRUE(GetLatestResults().empty());
}

TEST_F(HelpAppZeroStateProviderTest,
       ReturnsReleaseNotesChipForEmptyQueryIfTimesLeftIsPositive) {
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 1);

  StartZeroStateSearch();

  ASSERT_EQ(1u, GetLatestResults().size());
  ChromeSearchResult* result = GetLatestResults().at(0).get();
  ExpectReleaseNotesChip(result, IDS_HELP_APP_WHATS_NEW_CONTINUE_TASK_TITLE,
                         ash::SearchResultDisplayType::kContinue);
}

TEST_F(HelpAppZeroStateProviderTest,
       DecrementsTimesLeftToShowReleaseNotesChipUponShowing) {
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 3);

  StartZeroStateSearch();

  ASSERT_EQ(1u, GetLatestResults().size());

  ChromeSearchResult* result = GetLatestResults().at(0).get();
  ExpectReleaseNotesChip(result, IDS_HELP_APP_WHATS_NEW_CONTINUE_TASK_TITLE,
                         ash::SearchResultDisplayType::kContinue);

  app_list_controller()->ShowAppList(ash::AppListShowSource::kSearchKey);
  EXPECT_EQ(3, profile()->GetPrefs()->GetInteger(
                   prefs::kReleaseNotesSuggestionChipTimesLeftToShow));

  app_list_notifier()->NotifyResultsUpdated(
      ash::SearchResultDisplayType::kContinue,
      {ash::AppListNotifier::Result(kReleaseNotesResultId,
                                    ash::HELP_APP_UPDATES, std::nullopt)});
  EXPECT_FALSE(app_list_notifier()->FireImpressionTimerForTesting(
      ash::AppListNotifier::Location::kContinue));

  app_list_notifier()->NotifyContinueSectionVisibilityChanged(
      ash::AppListNotifier::Location::kContinue, true);

  EXPECT_EQ(3, profile()->GetPrefs()->GetInteger(
                   prefs::kReleaseNotesSuggestionChipTimesLeftToShow));

  ASSERT_TRUE(app_list_notifier()->FireImpressionTimerForTesting(
      ash::SearchResultDisplayType::kContinue));

  EXPECT_EQ(2, profile()->GetPrefs()->GetInteger(
                   prefs::kReleaseNotesSuggestionChipTimesLeftToShow));
}

TEST_F(HelpAppZeroStateProviderTest,
       ClickingReleaseNotesChipStopsItFromShowing) {
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 3);

  StartZeroStateSearch();

  ChromeSearchResult* result = GetLatestResults().at(0).get();
  result->Open(/*event_flags=*/0);

  EXPECT_EQ(0, profile()->GetPrefs()->GetInteger(
                   prefs::kReleaseNotesSuggestionChipTimesLeftToShow));
}

}  // namespace app_list::test
