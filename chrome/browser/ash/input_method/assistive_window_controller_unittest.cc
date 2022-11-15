// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/assistive_window_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/input_method/assistive_window_controller_delegate.h"
#include "chrome/browser/ash/input_method/ui/assistive_accessibility_view.h"
#include "chrome/browser/ash/input_method/ui/assistive_delegate.h"
#include "chrome/browser/ash/input_method/ui/suggestion_details.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/base/ime/ash/ime_assistive_window_handler_interface.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/wm/core/window_util.h"

namespace {
const char16_t kAnnounceString[] = u"announce string";
}  // namespace

namespace ash {
namespace input_method {

using ::ash::ime::AssistiveSuggestion;
using ::ash::ime::AssistiveSuggestionType;
using ::ash::ime::AssistiveWindow;
using ::ash::ime::AssistiveWindowType;
using ::testing::_;
using ::ui::ime::SuggestionDetails;

constexpr size_t kShowSuggestionDelayMs = 5;

std::vector<AssistiveSuggestion> SingleCandidateList(
    const AssistiveSuggestionType& suggestion_type,
    const std::string& candidate,
    size_t confirmed_length) {
  return std::vector<AssistiveSuggestion>{
      AssistiveSuggestion{
          .type = suggestion_type,
          .text = candidate,
          .confirmed_length = confirmed_length,
      },
  };
}

std::vector<AssistiveSuggestion> MultiCandidateList(
    const AssistiveSuggestionType& suggestion_type,
    const std::vector<std::string>& candidates) {
  std::vector<AssistiveSuggestion> suggestions;
  for (const auto& candidate : candidates) {
    suggestions.push_back(AssistiveSuggestion{
        .type = suggestion_type,
        .text = candidate,
        .confirmed_length = 0,
    });
  }
  return suggestions;
}

std::vector<AssistiveSuggestion> EmptyCandidateList() {
  return {};
}

AssistiveWindowProperties MultiWordWindowProperties(
    const std::u16string& candidate) {
  return AssistiveWindowProperties(
      /*type=*/AssistiveWindowType::kEmojiSuggestion,
      /*suggestion_type=*/AssistiveSuggestionType::kAssistiveEmoji,
      /*visible=*/true,
      /*candidates=*/{candidate});
}

AssistiveWindowProperties PersonalInfoWindowProperties(
    const std::u16string& candidate) {
  return AssistiveWindowProperties(
      /*type=*/AssistiveWindowType::kEmojiSuggestion,
      /*suggestion_type=*/AssistiveSuggestionType::kAssistiveEmoji,
      /*visible=*/true,
      /*candidates=*/{candidate});
}

AssistiveWindowProperties EmojiWindowProperties(
    const std::vector<std::u16string>& candidates) {
  return AssistiveWindowProperties(
      /*type=*/AssistiveWindowType::kEmojiSuggestion,
      /*suggestion_type=*/AssistiveSuggestionType::kAssistiveEmoji,
      /*visible=*/true,
      /*candidates=*/candidates);
}

AssistiveWindowProperties LongpressWindowProperties(
    const std::vector<std::u16string>& candidates) {
  return AssistiveWindowProperties(
      /*type=*/AssistiveWindowType::kLongpressDiacriticsSuggestion,
      /*suggestion_type=*/AssistiveSuggestionType::kLongpressDiacritic,
      /*visible=*/true,
      /*candidates=*/candidates);
}

AssistiveWindowProperties UndoWindowProperties() {
  return AssistiveWindowProperties(
      /*type=*/AssistiveWindowType::kUndoWindow,
      /*visible=*/true,
      /*candidates=*/{});
}

class MockDelegate : public AssistiveWindowControllerDelegate {
 public:
  MOCK_METHOD(void,
              AssistiveWindowButtonClicked,
              (const ui::ime::AssistiveWindowButton& button),
              (const override));
  MOCK_METHOD(void,
              AssistiveWindowChanged,
              (const ash::ime::AssistiveWindow& window),
              (const override));
};

class TestAccessibilityView : public ui::ime::AssistiveAccessibilityView {
 public:
  TestAccessibilityView() = default;
  void VerifyAnnouncement(const std::u16string& expected_text) {
    EXPECT_EQ(text_, expected_text);
  }

  void Announce(const std::u16string& text) override { text_ = text; }

 private:
  std::u16string text_;
};

class AssistiveWindowControllerTest : public ChromeAshTestBase {
 protected:
  AssistiveWindowControllerTest()
      : ChromeAshTestBase(std::make_unique<content::BrowserTaskEnvironment>(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME)) {}

  ~AssistiveWindowControllerTest() override = default;

  void SetUp() override {
    ChromeAshTestBase::SetUp();
    std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(1));
    wm::ActivateWindow(window.get());

    profile_ = std::make_unique<TestingProfile>();
    accessibility_view_ = std::make_unique<TestAccessibilityView>();
    controller_ = std::make_unique<AssistiveWindowController>(
        &delegate_, profile_.get(), accessibility_view_.get());
    ui::IMEBridge::Get()->SetAssistiveWindowHandler(controller_.get());

    // TODO(crbug/1102283): Create MockSuggestionWindowView to be independent of
    // SuggestionWindowView's implementation.
    static_cast<views::TestViewsDelegate*>(views::ViewsDelegate::GetInstance())
        ->set_layout_provider(ChromeLayoutProvider::CreateLayoutProvider());
  }

  void TearDown() override {
    controller_.reset();
    ChromeAshTestBase::TearDown();
  }

  std::vector<std::u16string> Candidates() {
    std::vector<std::u16string> candidates;
    for (int i = 0; i < 3; i++) {
      std::string candidate = base::NumberToString(i);
      candidates.push_back(base::UTF8ToUTF16(candidate));
    }
    return candidates;
  }

  void InitEmojiSuggestionWindow() {
    emoji_window_.type = ash::ime::AssistiveWindowType::kEmojiSuggestion;
    emoji_window_.visible = true;
    emoji_window_.candidates = Candidates();
  }

  void InitEmojiButton() {
    emoji_button_.window_type = ash::ime::AssistiveWindowType::kEmojiSuggestion;
    emoji_button_.announce_string = kAnnounceString;
  }

  void EnableLacros() {
    feature_list_.Reset();
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kLacrosSupport},
        /*disabled_features=*/{});
  }

  void WaitForSuggestionWindowDelay() {
    task_environment()->FastForwardBy(
        base::Milliseconds(kShowSuggestionDelayMs + 1));
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<AssistiveWindowController> controller_;
  testing::StrictMock<MockDelegate> delegate_;
  std::unique_ptr<TestingProfile> profile_;
  const std::u16string suggestion_ = u"test";
  ui::ime::AssistiveWindowButton emoji_button_;
  AssistiveWindowProperties emoji_window_;
  std::unique_ptr<TestAccessibilityView> accessibility_view_;
};

TEST_F(AssistiveWindowControllerTest, ShowSuggestionDelaysWindowDisplay) {
  EXPECT_CALL(delegate_, AssistiveWindowChanged(_));

  ui::ime::SuggestionDetails details;
  details.text = u"asdf";
  details.confirmed_length = 3;

  controller_->ShowSuggestion(details);
  ui::ime::SuggestionWindowView* window_before_delay =
      controller_->GetSuggestionWindowViewForTesting();
  WaitForSuggestionWindowDelay();
  ui::ime::SuggestionWindowView* window_after_delay =
      controller_->GetSuggestionWindowViewForTesting();

  EXPECT_EQ(window_before_delay, nullptr);
  EXPECT_NE(window_after_delay, nullptr);
}

TEST_F(AssistiveWindowControllerTest,
       SetBoundsAfterShowSuggestionCancelsDelay) {
  EXPECT_CALL(delegate_, AssistiveWindowChanged(_));

  ui::ime::SuggestionDetails details;
  details.text = u"asdf";
  details.confirmed_length = 3;
  gfx::Rect caret_bounds(0, 0, 100, 100);

  controller_->ShowSuggestion(details);
  controller_->SetBounds(Bounds{.caret = caret_bounds});
  ui::ime::SuggestionWindowView* suggestion_window_view =
      controller_->GetSuggestionWindowViewForTesting();

  EXPECT_NE(suggestion_window_view, nullptr);
}

TEST_F(AssistiveWindowControllerTest, ShowSuggestionSetsConfirmedLength) {
  ui::ime::SuggestionDetails details;
  details.text = u"asdf";
  details.confirmed_length = 3;

  controller_->ShowSuggestion(details);

  EXPECT_EQ(controller_->GetConfirmedLength(), 3u);
}

TEST_F(AssistiveWindowControllerTest, ConfirmedLength0SetsBoundsToCaretBounds) {
  EXPECT_CALL(delegate_, AssistiveWindowChanged(_));

  ui::ime::SuggestionDetails details;
  details.text = suggestion_;
  details.confirmed_length = 0;

  controller_->ShowSuggestion(details);
  WaitForSuggestionWindowDelay();
  ui::ime::SuggestionWindowView* suggestion_view =
      controller_->GetSuggestionWindowViewForTesting();

  ASSERT_NE(suggestion_view, nullptr);
  gfx::Rect current_bounds = suggestion_view->GetAnchorRect();
  gfx::Rect caret_bounds(0, 0, 100, 100);
  Bounds bounds;
  bounds.caret = caret_bounds;
  controller_->SetBounds(bounds);

  EXPECT_NE(suggestion_view->GetAnchorRect(), current_bounds);
  EXPECT_EQ(suggestion_view->GetAnchorRect(), caret_bounds);
}

TEST_F(AssistiveWindowControllerTest, ConfirmedLengthNSetsBoundsToCaretBounds) {
  EXPECT_CALL(delegate_, AssistiveWindowChanged(_));

  ui::ime::SuggestionDetails details;
  details.text = suggestion_;
  details.confirmed_length = 1;

  controller_->ShowSuggestion(details);
  WaitForSuggestionWindowDelay();
  ui::ime::SuggestionWindowView* suggestion_view =
      controller_->GetSuggestionWindowViewForTesting();

  ASSERT_NE(suggestion_view, nullptr);
  gfx::Rect current_bounds = suggestion_view->GetAnchorRect();
  gfx::Rect caret_bounds(0, 0, 100, 100);
  Bounds bounds;
  bounds.caret = caret_bounds;
  controller_->SetBounds(bounds);

  EXPECT_NE(suggestion_view->GetAnchorRect(), current_bounds);
  EXPECT_EQ(suggestion_view->GetAnchorRect(), caret_bounds);
}

TEST_F(AssistiveWindowControllerTest, WindowTracksCaretBounds) {
  EXPECT_CALL(delegate_, AssistiveWindowChanged(_)).Times(2);

  ui::ime::SuggestionDetails details;
  details.text = suggestion_;
  details.confirmed_length = 0;

  controller_->ShowSuggestion(details);
  WaitForSuggestionWindowDelay();
  ui::ime::SuggestionWindowView* suggestion_view =
      controller_->GetSuggestionWindowViewForTesting();

  ASSERT_NE(suggestion_view, nullptr);
  gfx::Rect current_bounds = suggestion_view->GetAnchorRect();
  gfx::Rect caret_bounds_after_one_key(current_bounds.width() + 1,
                                       current_bounds.height());
  gfx::Rect caret_bounds_after_two_key(current_bounds.width() + 2,
                                       current_bounds.height());

  // One char entered
  controller_->SetBounds(Bounds{.caret = caret_bounds_after_one_key});

  // Mimic tracking the last suggestion
  details.confirmed_length = 1;
  controller_->ShowSuggestion(details);

  // Second char entered to text input
  controller_->SetBounds(Bounds{.caret = caret_bounds_after_two_key});

  // Anchor should track the new caret position.
  EXPECT_EQ(suggestion_view->GetAnchorRect(), caret_bounds_after_two_key);
}

TEST_F(AssistiveWindowControllerTest,
       SuggestionViewBoundIsResetAfterHideSuggestionThenShowAgain) {
  EXPECT_CALL(delegate_, AssistiveWindowChanged(_)).Times(3);

  // Sets up suggestion_view with confirmed_length = 1.
  ui::ime::SuggestionDetails details;
  details.text = suggestion_;
  details.confirmed_length = 1;

  controller_->ShowSuggestion(details);
  WaitForSuggestionWindowDelay();

  gfx::Rect current_bounds =
      controller_->GetSuggestionWindowViewForTesting()->GetAnchorRect();
  controller_->HideSuggestion();

  // Create new suggestion window.
  controller_->SetAssistiveWindowProperties(
      EmojiWindowProperties({u"candidate"}));

  gfx::Rect new_caret_bounds(current_bounds.width() + 1,
                             current_bounds.height());
  Bounds bounds;
  bounds.caret = new_caret_bounds;
  controller_->SetBounds(bounds);
  EXPECT_EQ(controller_->GetSuggestionWindowViewForTesting()->GetAnchorRect(),
            new_caret_bounds);
}

TEST_F(AssistiveWindowControllerTest, SetsUndoWindowAnchorRectCorrectly) {
  EXPECT_CALL(delegate_, AssistiveWindowChanged(_));

  gfx::Rect autocorrect_bounds(1, 1);
  gfx::Rect caret_bounds(2, 2);

  Bounds bounds;
  bounds.caret = caret_bounds;
  bounds.autocorrect = autocorrect_bounds;
  controller_->SetBounds(bounds);
  controller_->SetAssistiveWindowProperties(UndoWindowProperties());

  ASSERT_TRUE(controller_->GetUndoWindowForTesting() != nullptr);
  autocorrect_bounds.Inset(-4);
  EXPECT_EQ(controller_->GetUndoWindowForTesting()->GetAnchorRect(),
            autocorrect_bounds);
}

TEST_F(AssistiveWindowControllerTest, SetsEmojiWindowOrientationVertical) {
  EXPECT_CALL(delegate_, AssistiveWindowChanged(_));

  controller_->SetAssistiveWindowProperties(
      EmojiWindowProperties({u"candidate"}));

  ASSERT_TRUE(controller_->GetSuggestionWindowViewForTesting() != nullptr);
  views::BoxLayout::Orientation layout_orientation =
      static_cast<views::BoxLayout*>(
          controller_->GetSuggestionWindowViewForTesting()
              ->multiple_candidate_area_for_testing()
              ->GetLayoutManager())
          ->GetOrientation();
  EXPECT_EQ(layout_orientation, views::BoxLayout::Orientation::kVertical);
}

TEST_F(AssistiveWindowControllerTest,
       SetsPersonalInfoWindowOrientationVertical) {
  EXPECT_CALL(delegate_, AssistiveWindowChanged(_));

  controller_->SetAssistiveWindowProperties(
      PersonalInfoWindowProperties(u"candidate"));

  ASSERT_TRUE(controller_->GetSuggestionWindowViewForTesting() != nullptr);
  views::BoxLayout::Orientation layout_orientation =
      static_cast<views::BoxLayout*>(
          controller_->GetSuggestionWindowViewForTesting()
              ->multiple_candidate_area_for_testing()
              ->GetLayoutManager())
          ->GetOrientation();
  EXPECT_EQ(layout_orientation, views::BoxLayout::Orientation::kVertical);
}

TEST_F(AssistiveWindowControllerTest, SetsMultiWordWindowOrientationVertical) {
  EXPECT_CALL(delegate_, AssistiveWindowChanged(_));

  controller_->SetAssistiveWindowProperties(
      MultiWordWindowProperties(u"candidate"));

  ASSERT_TRUE(controller_->GetSuggestionWindowViewForTesting() != nullptr);
  views::BoxLayout::Orientation layout_orientation =
      static_cast<views::BoxLayout*>(
          controller_->GetSuggestionWindowViewForTesting()
              ->multiple_candidate_area_for_testing()
              ->GetLayoutManager())
          ->GetOrientation();
  EXPECT_EQ(layout_orientation, views::BoxLayout::Orientation::kVertical);
}

TEST_F(AssistiveWindowControllerTest,
       SetsDiacriticsWindowOrientationHorizontal) {
  EXPECT_CALL(delegate_, AssistiveWindowChanged(_));

  controller_->SetAssistiveWindowProperties(
      LongpressWindowProperties({u"a", u"b", u"c"}));

  ASSERT_TRUE(controller_->GetSuggestionWindowViewForTesting() != nullptr);
  views::BoxLayout::Orientation layout_orientation =
      static_cast<views::BoxLayout*>(
          controller_->GetSuggestionWindowViewForTesting()
              ->multiple_candidate_area_for_testing()
              ->GetLayoutManager())
          ->GetOrientation();
  EXPECT_EQ(layout_orientation, views::BoxLayout::Orientation::kHorizontal);
}

TEST_F(AssistiveWindowControllerTest,
       SetsWindowOrientationHorizontalWhenVerticalWindowAlreadyInitialised) {
  EXPECT_CALL(delegate_, AssistiveWindowChanged(_)).Times(2);

  // Show a vertical window
  controller_->SetAssistiveWindowProperties(
      MultiWordWindowProperties(u"vertical"));
  // Then show a horizontal window
  controller_->SetAssistiveWindowProperties(
      LongpressWindowProperties({u"a", u"b", u"c"}));

  ASSERT_TRUE(controller_->GetSuggestionWindowViewForTesting() != nullptr);
  views::BoxLayout::Orientation layout_orientation =
      static_cast<views::BoxLayout*>(
          controller_->GetSuggestionWindowViewForTesting()
              ->multiple_candidate_area_for_testing()
              ->GetLayoutManager())
          ->GetOrientation();
  EXPECT_EQ(layout_orientation, views::BoxLayout::Orientation::kHorizontal);
}

TEST_F(AssistiveWindowControllerTest,
       SetsWindowOrientationVerticalWhenHorizontalWindowAlreadyInitialised) {
  EXPECT_CALL(delegate_, AssistiveWindowChanged(_)).Times(2);

  // Show a horizontal window
  controller_->SetAssistiveWindowProperties(
      LongpressWindowProperties({u"a", u"b", u"c"}));
  // Then show a vertical window
  controller_->SetAssistiveWindowProperties(
      MultiWordWindowProperties(u"vertical"));

  ASSERT_TRUE(controller_->GetSuggestionWindowViewForTesting() != nullptr);
  views::BoxLayout::Orientation layout_orientation =
      static_cast<views::BoxLayout*>(
          controller_->GetSuggestionWindowViewForTesting()
              ->multiple_candidate_area_for_testing()
              ->GetLayoutManager())
          ->GetOrientation();
  EXPECT_EQ(layout_orientation, views::BoxLayout::Orientation::kVertical);
}
TEST_F(AssistiveWindowControllerTest,
       AnnouncesWhenSetButtonHighlightedInEmojiWindowHasAnnounceString) {
  profile_->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled, true);
  InitEmojiSuggestionWindow();
  InitEmojiButton();

  controller_->SetAssistiveWindowProperties(emoji_window_);
  controller_->SetButtonHighlighted(emoji_button_, true);
  task_environment()->RunUntilIdle();

  accessibility_view_->VerifyAnnouncement(kAnnounceString);
}

TEST_F(
    AssistiveWindowControllerTest,
    DoesNotAnnounceWhenSetButtonHighlightedInEmojiWindowDoesNotHaveAnnounceString) {
  profile_->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled, true);
  InitEmojiSuggestionWindow();
  InitEmojiButton();
  emoji_button_.announce_string = base::EmptyString16();

  controller_->SetAssistiveWindowProperties(emoji_window_);
  controller_->SetButtonHighlighted(emoji_button_, true);
  task_environment()->RunUntilIdle();

  accessibility_view_->VerifyAnnouncement(base::EmptyString16());
}

TEST_F(AssistiveWindowControllerTest,
       AnnouncesWhenSetButtonHighlightedInUndoWindowHasAnnounceString) {
  EXPECT_CALL(delegate_, AssistiveWindowChanged(_));

  profile_->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled, true);
  AssistiveWindowProperties window;
  window.type = ash::ime::AssistiveWindowType::kUndoWindow;
  window.visible = true;
  ui::ime::AssistiveWindowButton button;
  button.window_type = ash::ime::AssistiveWindowType::kUndoWindow;
  button.announce_string = kAnnounceString;

  controller_->SetAssistiveWindowProperties(window);
  controller_->SetButtonHighlighted(button, true);
  task_environment()->RunUntilIdle();

  accessibility_view_->VerifyAnnouncement(kAnnounceString);
}

TEST_F(
    AssistiveWindowControllerTest,
    DoesNotAnnounceWhenSetButtonHighlightedAndSuggestionWindowViewIsNotActive) {
  profile_->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled, true);
  InitEmojiButton();

  controller_->SetButtonHighlighted(emoji_button_, true);
  task_environment()->RunUntilIdle();

  accessibility_view_->VerifyAnnouncement(base::EmptyString16());
}

TEST_F(AssistiveWindowControllerTest,
       NoAssistiveWindowChangedWhenNoWindowShownAndSuggestionHidden) {
  EXPECT_CALL(delegate_, AssistiveWindowChanged(_)).Times(0);

  controller_->HideSuggestion();
}

TEST_F(AssistiveWindowControllerTest,
       NoAssistiveWindowChangedWhenNoWindowShownAndSuggestionAccepted) {
  EXPECT_CALL(delegate_, AssistiveWindowChanged(_)).Times(0);

  controller_->AcceptSuggestion(u"");
}

TEST_F(AssistiveWindowControllerTest,
       NoAssistiveWindowChangedWhenNoWindowShownAndFocusChanged) {
  EXPECT_CALL(delegate_, AssistiveWindowChanged(_)).Times(0);

  controller_->FocusStateChanged();
}

struct ShowSuggestionTestCase {
  std::string test_name;
  AssistiveWindow window;
  SuggestionDetails suggestion_details;
};

class AssistiveWindowChangedAfterShowSuggestion
    : public AssistiveWindowControllerTest,
      public testing::WithParamInterface<ShowSuggestionTestCase> {};

TEST_P(AssistiveWindowChangedAfterShowSuggestion, VerifyChangeEmitted) {
  const ShowSuggestionTestCase& test_case = GetParam();
  EXPECT_CALL(delegate_, AssistiveWindowChanged(test_case.window));

  controller_->ShowSuggestion(test_case.suggestion_details);
  WaitForSuggestionWindowDelay();
}

TEST_P(AssistiveWindowChangedAfterShowSuggestion,
       AndAgainAfterSuggestionIsHidden) {
  const ShowSuggestionTestCase& test_case = GetParam();
  {
    testing::InSequence seq;
    EXPECT_CALL(delegate_, AssistiveWindowChanged(test_case.window));
    EXPECT_CALL(delegate_,
                AssistiveWindowChanged(AssistiveWindow(
                    AssistiveWindowType::kNone, EmptyCandidateList())));
  }

  controller_->ShowSuggestion(test_case.suggestion_details);
  WaitForSuggestionWindowDelay();
  controller_->HideSuggestion();
}

TEST_P(AssistiveWindowChangedAfterShowSuggestion,
       AndAgainAfterSuggestionIsAccepted) {
  const ShowSuggestionTestCase& test_case = GetParam();
  {
    testing::InSequence seq;
    EXPECT_CALL(delegate_, AssistiveWindowChanged(test_case.window));
    EXPECT_CALL(delegate_,
                AssistiveWindowChanged(AssistiveWindow(
                    AssistiveWindowType::kNone, EmptyCandidateList())));
  }

  controller_->ShowSuggestion(test_case.suggestion_details);
  WaitForSuggestionWindowDelay();
  controller_->AcceptSuggestion(u"");
}

TEST_P(AssistiveWindowChangedAfterShowSuggestion,
       AndAgainWhenWidgetIsDestroying) {
  const ShowSuggestionTestCase& test_case = GetParam();
  {
    testing::InSequence seq;
    EXPECT_CALL(delegate_, AssistiveWindowChanged(test_case.window));
    EXPECT_CALL(delegate_,
                AssistiveWindowChanged(AssistiveWindow(
                    AssistiveWindowType::kNone, EmptyCandidateList())));
  }

  controller_->ShowSuggestion(test_case.suggestion_details);
  WaitForSuggestionWindowDelay();
  controller_->OnWidgetDestroying(
      controller_->GetSuggestionWindowViewForTesting()->GetWidget());
}

TEST_P(AssistiveWindowChangedAfterShowSuggestion,
       AndAgainWhenFocusStateChanged) {
  const ShowSuggestionTestCase& test_case = GetParam();
  {
    testing::InSequence seq;
    EXPECT_CALL(delegate_, AssistiveWindowChanged(test_case.window));
    EXPECT_CALL(delegate_,
                AssistiveWindowChanged(AssistiveWindow(
                    AssistiveWindowType::kNone, EmptyCandidateList())));
  }

  controller_->ShowSuggestion(test_case.suggestion_details);
  WaitForSuggestionWindowDelay();
  controller_->FocusStateChanged();
}

INSTANTIATE_TEST_SUITE_P(
    AssistiveWindowControllerTest,
    AssistiveWindowChangedAfterShowSuggestion,
    testing::ValuesIn<ShowSuggestionTestCase>({
        {"MultiwordCandidate",
         AssistiveWindow(
             AssistiveWindowType::kMultiWordSuggestion,
             SingleCandidateList(
                 /*suggestion_type=*/AssistiveSuggestionType::kMultiWord,
                 /*candidate=*/"gday",
                 /*confirmed_length=*/0)),
         SuggestionDetails{.type = AssistiveSuggestionType::kMultiWord,
                           .text = u"gday",
                           .confirmed_length = 0}},
        {"PersonalInfoCandidate",
         AssistiveWindow(AssistiveWindowType::kPersonalInfoSuggestion,
                         SingleCandidateList(
                             /*suggestion_type=*/AssistiveSuggestionType::
                                 kAssistivePersonalInfo,
                             /*candidate=*/"my name is Jack Black",
                             /*confirmed_length=*/0)),
         SuggestionDetails{
             .type = AssistiveSuggestionType::kAssistivePersonalInfo,
             .text = u"my name is Jack Black",
             .confirmed_length = 0}},
    }),
    [](const testing::TestParamInfo<ShowSuggestionTestCase>& info) {
      return info.param.test_name;
    });

struct SetAssistiveWindowPropsTestCase {
  std::string test_name;
  AssistiveWindow window;
  AssistiveWindowProperties window_props;
  AssistiveWindowProperties hidden_props;
};

class AssistiveWindowChangedAfterSetAssistiveWindowProps
    : public AssistiveWindowControllerTest,
      public testing::WithParamInterface<SetAssistiveWindowPropsTestCase> {
 protected:
  views::Widget* GetWidgetForWindowType(
      const AssistiveWindowType& window_type) {
    switch (window_type) {
      case AssistiveWindowType::kMultiWordSuggestion:
      case AssistiveWindowType::kPersonalInfoSuggestion:
      case AssistiveWindowType::kEmojiSuggestion:
      case AssistiveWindowType::kLongpressDiacriticsSuggestion:
        return controller_->GetSuggestionWindowViewForTesting()->GetWidget();
      case AssistiveWindowType::kUndoWindow:
        return controller_->GetUndoWindowForTesting()->GetWidget();
      case AssistiveWindowType::kGrammarSuggestion:
        return controller_->GetGrammarWindowForTesting()->GetWidget();
      default:
        return nullptr;
    }
  }
};

TEST_P(AssistiveWindowChangedAfterSetAssistiveWindowProps,
       VerifyChangeEmitted) {
  const SetAssistiveWindowPropsTestCase& test_case = GetParam();
  EXPECT_CALL(delegate_, AssistiveWindowChanged(test_case.window));

  controller_->SetAssistiveWindowProperties(test_case.window_props);
}

TEST_P(AssistiveWindowChangedAfterSetAssistiveWindowProps,
       AndAgainAfterSuggestionIsHidden) {
  const SetAssistiveWindowPropsTestCase& test_case = GetParam();
  {
    testing::InSequence seq;
    EXPECT_CALL(delegate_, AssistiveWindowChanged(test_case.window));
    EXPECT_CALL(delegate_,
                AssistiveWindowChanged(AssistiveWindow(
                    AssistiveWindowType::kNone, EmptyCandidateList())));
  }

  controller_->SetAssistiveWindowProperties(test_case.window_props);
  controller_->HideSuggestion();
}

TEST_P(AssistiveWindowChangedAfterSetAssistiveWindowProps,
       AndAgainAfterSuggestionIsHiddenByWindowProps) {
  const SetAssistiveWindowPropsTestCase& test_case = GetParam();
  {
    testing::InSequence seq;
    EXPECT_CALL(delegate_, AssistiveWindowChanged(test_case.window));
    EXPECT_CALL(delegate_,
                AssistiveWindowChanged(AssistiveWindow(
                    AssistiveWindowType::kNone, EmptyCandidateList())));
  }

  controller_->SetAssistiveWindowProperties(test_case.window_props);
  controller_->SetAssistiveWindowProperties(test_case.hidden_props);
}

TEST_P(AssistiveWindowChangedAfterSetAssistiveWindowProps,
       AndAgainAfterSuggestionIsAccepted) {
  const SetAssistiveWindowPropsTestCase& test_case = GetParam();
  if (test_case.window.type == AssistiveWindowType::kUndoWindow) {
    // You can't accept a suggestion for the undo window as there's no
    // suggestion to be accepted.
    return;
  }

  {
    testing::InSequence seq;
    EXPECT_CALL(delegate_, AssistiveWindowChanged(test_case.window));
    EXPECT_CALL(delegate_,
                AssistiveWindowChanged(AssistiveWindow(
                    AssistiveWindowType::kNone, EmptyCandidateList())));
  }

  controller_->SetAssistiveWindowProperties(test_case.window_props);
  controller_->AcceptSuggestion(u"");
}

TEST_P(AssistiveWindowChangedAfterSetAssistiveWindowProps,
       AndAgainWhenFocusStateChanged) {
  const SetAssistiveWindowPropsTestCase& test_case = GetParam();
  {
    testing::InSequence seq;
    EXPECT_CALL(delegate_, AssistiveWindowChanged(test_case.window));
    EXPECT_CALL(delegate_,
                AssistiveWindowChanged(AssistiveWindow(
                    AssistiveWindowType::kNone, EmptyCandidateList())));
  }

  controller_->SetAssistiveWindowProperties(test_case.window_props);
  controller_->FocusStateChanged();
}

TEST_P(AssistiveWindowChangedAfterSetAssistiveWindowProps,
       AndAgainWhenWidgetIsDestroying) {
  const SetAssistiveWindowPropsTestCase& test_case = GetParam();
  {
    testing::InSequence seq;
    EXPECT_CALL(delegate_, AssistiveWindowChanged(test_case.window));
    EXPECT_CALL(delegate_,
                AssistiveWindowChanged(AssistiveWindow(
                    AssistiveWindowType::kNone, EmptyCandidateList())));
  }

  controller_->SetAssistiveWindowProperties(test_case.window_props);
  controller_->OnWidgetDestroying(
      GetWidgetForWindowType(test_case.window.type));
}

INSTANTIATE_TEST_SUITE_P(
    AssistiveWindowControllerTest,
    AssistiveWindowChangedAfterSetAssistiveWindowProps,
    testing::ValuesIn<SetAssistiveWindowPropsTestCase>({
        {"EmojiWindow",
         AssistiveWindow(
             AssistiveWindowType::kEmojiSuggestion,
             MultiCandidateList(
                 /*suggestion_type=*/AssistiveSuggestionType::kAssistiveEmoji,
                 /*candidates=*/{":)", ":(", ":|"})),
         AssistiveWindowProperties(
             /*type=*/AssistiveWindowType::kEmojiSuggestion,
             /*suggestion_type=*/AssistiveSuggestionType::kAssistiveEmoji,
             /*visible=*/true,
             /*candidates=*/{u":)", u":(", u":|"}),
         AssistiveWindowProperties(
             /*type=*/AssistiveWindowType::kEmojiSuggestion,
             /*suggestion_type=*/AssistiveSuggestionType::kAssistiveEmoji,
             /*visible=*/false,
             /*candidates=*/{})},
        {"LongpressWindow",
         AssistiveWindow(AssistiveWindowType::kLongpressDiacriticsSuggestion,
                         MultiCandidateList(
                             /*suggestion_type=*/AssistiveSuggestionType::
                                 kLongpressDiacritic,
                             /*candidates=*/{"a", "b", "c"})),
         AssistiveWindowProperties(
             /*type=*/AssistiveWindowType::kLongpressDiacriticsSuggestion,
             /*suggestion_type=*/AssistiveSuggestionType::kLongpressDiacritic,
             /*visible=*/true,
             /*candidates=*/{u"a", u"b", u"c"}),
         AssistiveWindowProperties(
             /*type=*/AssistiveWindowType::kLongpressDiacriticsSuggestion,
             /*suggestion_type=*/AssistiveSuggestionType::kLongpressDiacritic,
             /*visible=*/false,
             /*candidates=*/{})},
        {"UndoWindow",
         AssistiveWindow(AssistiveWindowType::kUndoWindow,
                         EmptyCandidateList()),
         AssistiveWindowProperties(
             /*type=*/AssistiveWindowType::kUndoWindow,
             /*visible=*/true,
             /*candidates=*/{}),
         AssistiveWindowProperties(
             /*type=*/AssistiveWindowType::kUndoWindow,
             /*visible=*/false,
             /*candidates=*/{})},
        {"GrammarWindow",
         AssistiveWindow(
             AssistiveWindowType::kGrammarSuggestion,
             SingleCandidateList(
                 /*suggestion_type=*/AssistiveSuggestionType::kGrammar,
                 /*candidate=*/"they are students",
                 /*confirmed_length=*/0)),
         AssistiveWindowProperties(
             /*type=*/AssistiveWindowType::kGrammarSuggestion,
             /*suggestion_type=*/AssistiveSuggestionType::kGrammar,
             /*visible=*/true,
             /*candidates*/ {u"they are students"}),
         AssistiveWindowProperties(
             /*type=*/AssistiveWindowType::kGrammarSuggestion,
             /*suggestion_type=*/AssistiveSuggestionType::kGrammar,
             /*visible=*/false,
             /*candidates*/ {u"they are students"})},
    }),
    [](const testing::TestParamInfo<SetAssistiveWindowPropsTestCase>& info) {
      return info.param.test_name;
    });

}  // namespace input_method
}  // namespace ash
