// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/ui/suggestion_window_view.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/input_method/assistive_window_properties.h"
#include "chrome/browser/ash/input_method/ui/assistive_delegate.h"
#include "chrome/browser/ash/input_method/ui/suggestion_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"

namespace ui {
namespace ime {

class MockAssistiveDelegate : public AssistiveDelegate {
 public:
  ~MockAssistiveDelegate() override = default;
  void AssistiveWindowButtonClicked(
      const ui::ime::AssistiveWindowButton& button) const override {}
};

class SuggestionWindowViewTest
    : public ChromeViewsTestBase,
      public ::testing::WithParamInterface<SuggestionWindowView::Orientation> {
 public:
  SuggestionWindowViewTest() {}

  SuggestionWindowViewTest(const SuggestionWindowViewTest&) = delete;
  SuggestionWindowViewTest& operator=(const SuggestionWindowViewTest&) = delete;

  ~SuggestionWindowViewTest() override {}

 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    InitCandidates();
    window_.candidates = candidates_;

    suggestion_window_view_ =
        SuggestionWindowView::Create(GetContext(), delegate_.get(), GetParam());
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

  absl::optional<int> GetHighlightedIndex() const {
    const auto& children =
        suggestion_window_view_->candidate_area_for_testing()->children();
    const auto it =
        std::find_if(children.cbegin(), children.cend(),
                     [](const views::View* v) { return !!v->background(); });
    return (it == children.cend())
               ? absl::nullopt
               : absl::make_optional(std::distance(children.cbegin(), it));
  }

  SuggestionWindowView* suggestion_window_view_;
  std::unique_ptr<MockAssistiveDelegate> delegate_ =
      std::make_unique<MockAssistiveDelegate>();
  std::vector<std::u16string> candidates_;
  ash::input_method::AssistiveWindowProperties window_;
  AssistiveWindowButton candidate_button_;
  AssistiveWindowButton setting_link_view_;
  AssistiveWindowButton learn_more_button_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    SuggestionWindowViewTest,
    testing::Values(SuggestionWindowView::Orientation::kHorizontal,
                    SuggestionWindowView::Orientation::kVertical),
    // Function to make the test name say ".../kHorizontal" etc.
    [](const testing::TestParamInfo<SuggestionWindowViewTest::ParamType>&
           info) {
      std::string name;
      switch (info.param) {
        case SuggestionWindowView::Orientation::kHorizontal:
          name = "Horizontal";
          break;
        case SuggestionWindowView::Orientation::kVertical:
          name = "Vertical";
          break;
        default:
          name = "UNKNOWN";
          break;
      }
      return name;
    });

TEST_P(SuggestionWindowViewTest, HighlightOneCandidateWhenIndexIsValid) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  for (int index = 0; index < static_cast<int>(candidates_.size()); index++) {
    candidate_button_.index = index;
    suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);

    EXPECT_EQ(1u, GetHighlightedCount());
    EXPECT_EQ(index, GetHighlightedIndex());
  }
}

TEST_P(SuggestionWindowViewTest, HighlightNoCandidateWhenIndexIsInvalid) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  for (int index : {-1, static_cast<int>(candidates_.size())}) {
    candidate_button_.index = index;
    suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);

    EXPECT_EQ(0u, GetHighlightedCount());
    EXPECT_FALSE(GetHighlightedIndex().has_value());
  }
}

TEST_P(SuggestionWindowViewTest, HighlightTheSameCandidateWhenCalledTwice) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  int highlight_index = 0;
  candidate_button_.index = highlight_index;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);

  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(highlight_index, GetHighlightedIndex());
}

TEST_P(SuggestionWindowViewTest,
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

TEST_P(SuggestionWindowViewTest,
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

TEST_P(SuggestionWindowViewTest, UnhighlightCandidateIfCurrentlyHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  candidate_button_.index = 0;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, false);

  EXPECT_EQ(0u, GetHighlightedCount());
  EXPECT_FALSE(GetHighlightedIndex().has_value());
}

TEST_P(SuggestionWindowViewTest,
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

TEST_P(SuggestionWindowViewTest, DoesNotUnhighlightCandidateIfOutOfRange) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  int highlight_index = 0;
  candidate_button_.index = highlight_index;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);

  for (int index : {-1, static_cast<int>(candidates_.size())}) {
    candidate_button_.index = index;
    suggestion_window_view_->SetButtonHighlighted(candidate_button_, false);

    EXPECT_EQ(1u, GetHighlightedCount());
    EXPECT_EQ(highlight_index, GetHighlightedIndex());
  }
}

TEST_P(SuggestionWindowViewTest, HighlightsSettingLinkViewWhenNotHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  suggestion_window_view_->SetButtonHighlighted(setting_link_view_, true);

  EXPECT_TRUE(
      suggestion_window_view_->setting_link_for_testing()->background() !=
      nullptr);
}

TEST_P(SuggestionWindowViewTest,
       HighlightsSettingLinkViewWhenAlreadyHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  suggestion_window_view_->SetButtonHighlighted(setting_link_view_, true);
  suggestion_window_view_->SetButtonHighlighted(setting_link_view_, true);

  EXPECT_TRUE(
      suggestion_window_view_->setting_link_for_testing()->background() !=
      nullptr);
}

TEST_P(SuggestionWindowViewTest, UnhighlightsSettingLinkViewWhenHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  suggestion_window_view_->SetButtonHighlighted(setting_link_view_, false);

  EXPECT_TRUE(
      suggestion_window_view_->setting_link_for_testing()->background() ==
      nullptr);
}

TEST_P(SuggestionWindowViewTest,
       UnhighlightsKeepSettingLinkViewUnhighlightedWhenAlreadyNotHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  suggestion_window_view_->SetButtonHighlighted(setting_link_view_, false);
  suggestion_window_view_->SetButtonHighlighted(setting_link_view_, false);

  EXPECT_TRUE(
      suggestion_window_view_->setting_link_for_testing()->background() ==
      nullptr);
}

TEST_P(SuggestionWindowViewTest, HighlightsLearnMoreButtonWhenNotHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  suggestion_window_view_->SetButtonHighlighted(learn_more_button_, true);

  EXPECT_TRUE(
      suggestion_window_view_->learn_more_button_for_testing()->background() !=
      nullptr);
}

TEST_P(SuggestionWindowViewTest,
       HighlightsLearnMoreButtonWhenAlreadyHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  suggestion_window_view_->SetButtonHighlighted(learn_more_button_, true);
  suggestion_window_view_->SetButtonHighlighted(learn_more_button_, true);

  EXPECT_TRUE(
      suggestion_window_view_->learn_more_button_for_testing()->background() !=
      nullptr);
}

TEST_P(SuggestionWindowViewTest, UnhighlightsLearnMoreButtonWhenHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  suggestion_window_view_->SetButtonHighlighted(learn_more_button_, false);

  EXPECT_TRUE(
      suggestion_window_view_->learn_more_button_for_testing()->background() ==
      nullptr);
}

TEST_P(SuggestionWindowViewTest,
       UnhighlightsKeepLearnMoreButtonUnhighlightedWhenAlreadyNotHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_);
  suggestion_window_view_->SetButtonHighlighted(learn_more_button_, false);
  suggestion_window_view_->SetButtonHighlighted(learn_more_button_, false);

  EXPECT_TRUE(
      suggestion_window_view_->learn_more_button_for_testing()->background() ==
      nullptr);
}

TEST_P(SuggestionWindowViewTest, DisplaysCorrectOrientationLayout) {
  views::BoxLayout::Orientation expected_orientation;
  switch (GetParam()) {
    case SuggestionWindowView::Orientation::kHorizontal:
      expected_orientation = views::BoxLayout::Orientation::kHorizontal;
      break;
    case SuggestionWindowView::Orientation::kVertical:
      expected_orientation = views::BoxLayout::Orientation::kVertical;
      break;
    default:
      abort();
  }
  suggestion_window_view_->ShowMultipleCandidates(window_);
  views::BoxLayout::Orientation layout_orientation =
      static_cast<views::BoxLayout*>(
          suggestion_window_view_->GetLayoutManager())
          ->GetOrientation();
  EXPECT_EQ(layout_orientation, expected_orientation);
}

}  // namespace ime
}  // namespace ui
