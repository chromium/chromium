// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ui/suggestion_window_view.h"

#include <string>

#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/input_method/assistive_window_properties.h"
#include "chrome/browser/chromeos/input_method/ui/assistive_delegate.h"
#include "chrome/browser/chromeos/input_method/ui/suggestion_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/link.h"

namespace ui {
namespace ime {

class MockAssistiveDelegate : public AssistiveDelegate {
 public:
  ~MockAssistiveDelegate() override = default;
  void AssistiveWindowButtonClicked(
      const ui::ime::AssistiveWindowButton& button) const override {}
};

class SuggestionWindowViewTest : public ChromeViewsTestBase {
 public:
  SuggestionWindowViewTest() {}
  ~SuggestionWindowViewTest() override {}

 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    InitCandidates();
    window_.candidates = candidates_;

    suggestion_window_view_ =
        SuggestionWindowView::Create(GetContext(), delegate_.get());
    candidate_button_.id = ButtonId::kSuggestion;
    setting_link_view_.id = ButtonId::kSmartInputsSettingLink;
    learn_more_button_.id = ButtonId::kLearnMore;
  }

  void TearDown() override {
    suggestion_window_view_->GetWidget()->CloseNow();
    ChromeViewsTestBase::TearDown();
  }

  void InitCandidates() {
    for (int i = 0; i < 3; i++) {
      std::string candidate = base::NumberToString(i);
      candidates_.push_back(base::UTF8ToUTF16(candidate));
    }
  }

  size_t GetHighlightedCount() const {
    const auto& children =
        suggestion_window_view_->candidate_area_for_testing()->children();
    return std::count_if(
        children.cbegin(), children.cend(),
        [](const views::View* v) { return !!v->background(); });
  }

  base::Optional<int> GetHighlightedIndex() const {
    const auto& children =
        suggestion_window_view_->candidate_area_for_testing()->children();
    const auto it =
        std::find_if(children.cbegin(), children.cend(),
                     [](const views::View* v) { return !!v->background(); });
    return (it == children.cend())
               ? base::nullopt
               : base::make_optional(std::distance(children.cbegin(), it));
  }

  SuggestionWindowView* suggestion_window_view_;
  std::unique_ptr<MockAssistiveDelegate> delegate_ =
      std::make_unique<MockAssistiveDelegate>();
  std::vector<std::u16string> candidates_;
  chromeos::AssistiveWindowProperties window_;
  AssistiveWindowButton candidate_button_;
  AssistiveWindowButton setting_link_view_;
  AssistiveWindowButton learn_more_button_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionWindowViewTest);
};

TEST_F(SuggestionWindowViewTest, HighlightOneCandidateWhenIndexIsValid) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  for (int index = 0; index < static_cast<int>(candidates_.size()); index++) {
    candidate_button_.index = index;
    suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);

    EXPECT_EQ(1u, GetHighlightedCount());
    EXPECT_EQ(index, GetHighlightedIndex());
  }
}

TEST_F(SuggestionWindowViewTest, HighlightNoCandidateWhenIndexIsInvalid) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  for (int index : {-1, int{candidates_.size()}}) {
    candidate_button_.index = index;
    suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);

    EXPECT_EQ(0u, GetHighlightedCount());
    EXPECT_FALSE(GetHighlightedIndex().has_value());
  }
}

TEST_F(SuggestionWindowViewTest, HighlightTheSameCandidateWhenCalledTwice) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  int highlight_index = 0;
  candidate_button_.index = highlight_index;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);

  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(highlight_index, GetHighlightedIndex());
}

TEST_F(SuggestionWindowViewTest,
       HighlightValidCandidateAfterGivingInvalidIndexThenValidIndex) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  int valid_index = 0;
  candidate_button_.index = candidates_.size();
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);
  candidate_button_.index = valid_index;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);

  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(valid_index, GetHighlightedIndex());
}

TEST_F(SuggestionWindowViewTest,
       KeepHighlightingValidCandidateWhenGivingValidThenInvalidIndex) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  int valid_index = 0;
  candidate_button_.index = valid_index;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);
  candidate_button_.index = candidates_.size();
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);

  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(valid_index, GetHighlightedIndex());
}

TEST_F(SuggestionWindowViewTest, UnhighlightCandidateIfCurrentlyHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  candidate_button_.index = 0;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, false);

  EXPECT_EQ(0u, GetHighlightedCount());
  EXPECT_FALSE(GetHighlightedIndex().has_value());
}

TEST_F(SuggestionWindowViewTest,
       DoesNotUnhighlightCandidateIfNotCurrentlyHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  int highlight_index = 0;
  candidate_button_.index = highlight_index;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);
  candidate_button_.index = -1;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, false);

  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(highlight_index, GetHighlightedIndex());
}

TEST_F(SuggestionWindowViewTest, DoesNotUnhighlightCandidateIfOutOfRange) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  int highlight_index = 0;
  candidate_button_.index = highlight_index;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);

  for (int index : {-1, int{candidates_.size()}}) {
    candidate_button_.index = index;
    suggestion_window_view_->SetButtonHighlighted(candidate_button_, false);

    EXPECT_EQ(1u, GetHighlightedCount());
    EXPECT_EQ(highlight_index, GetHighlightedIndex());
  }
}

TEST_F(SuggestionWindowViewTest, HighlightsSettingLinkViewWhenNotHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  suggestion_window_view_->SetButtonHighlighted(setting_link_view_, true);

  EXPECT_TRUE(
      suggestion_window_view_->setting_link_for_testing()->background() !=
      nullptr);
}

TEST_F(SuggestionWindowViewTest,
       HighlightsSettingLinkViewWhenAlreadyHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  suggestion_window_view_->SetButtonHighlighted(setting_link_view_, true);
  suggestion_window_view_->SetButtonHighlighted(setting_link_view_, true);

  EXPECT_TRUE(
      suggestion_window_view_->setting_link_for_testing()->background() !=
      nullptr);
}

TEST_F(SuggestionWindowViewTest, UnhighlightsSettingLinkViewWhenHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  suggestion_window_view_->SetButtonHighlighted(setting_link_view_, false);

  EXPECT_TRUE(
      suggestion_window_view_->setting_link_for_testing()->background() ==
      nullptr);
}

TEST_F(SuggestionWindowViewTest,
       UnhighlightsKeepSettingLinkViewUnhighlightedWhenAlreadyNotHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  suggestion_window_view_->SetButtonHighlighted(setting_link_view_, false);
  suggestion_window_view_->SetButtonHighlighted(setting_link_view_, false);

  EXPECT_TRUE(
      suggestion_window_view_->setting_link_for_testing()->background() ==
      nullptr);
}

TEST_F(SuggestionWindowViewTest, HighlightsLearnMoreButtonWhenNotHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  suggestion_window_view_->SetButtonHighlighted(learn_more_button_, true);

  EXPECT_TRUE(
      suggestion_window_view_->learn_more_button_for_testing()->background() !=
      nullptr);
}

TEST_F(SuggestionWindowViewTest,
       HighlightsLearnMoreButtonWhenAlreadyHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  suggestion_window_view_->SetButtonHighlighted(learn_more_button_, true);
  suggestion_window_view_->SetButtonHighlighted(learn_more_button_, true);

  EXPECT_TRUE(
      suggestion_window_view_->learn_more_button_for_testing()->background() !=
      nullptr);
}

TEST_F(SuggestionWindowViewTest, UnhighlightsLearnMoreButtonWhenHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  suggestion_window_view_->SetButtonHighlighted(learn_more_button_, false);

  EXPECT_TRUE(
      suggestion_window_view_->learn_more_button_for_testing()->background() ==
      nullptr);
}

TEST_F(SuggestionWindowViewTest,
       UnhighlightsKeepLearnMoreButtonUnhighlightedWhenAlreadyNotHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  suggestion_window_view_->SetButtonHighlighted(learn_more_button_, false);
  suggestion_window_view_->SetButtonHighlighted(learn_more_button_, false);

  EXPECT_TRUE(
      suggestion_window_view_->learn_more_button_for_testing()->background() ==
      nullptr);
}

}  // namespace ime
}  // namespace ui
