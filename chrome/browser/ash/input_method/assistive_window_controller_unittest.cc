// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/assistive_window_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "base/feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/input_method/assistive_window_controller_delegate.h"
#include "chrome/browser/ui/ash/input_method/announcement_view.h"
#include "chrome/browser/ui/ash/input_method/suggestion_details.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "content/public/test/browser_task_environment.h"
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

constexpr size_t kShowSuggestionDelay = 5;

class MockDelegate : public AssistiveWindowControllerDelegate {
 public:
  ~MockDelegate() override = default;
  void AssistiveWindowButtonClicked(
      const ui::ime::AssistiveWindowButton& button) const override {}
  void AssistiveWindowChanged(
      const ash::ime::AssistiveWindow& window) const override {}
};

class TestAnnouncementView : public ui::ime::AnnouncementView {
 public:
  TestAnnouncementView() = default;
  void VerifyAnnouncement(const std::u16string& expected_text) {
    EXPECT_EQ(text_, expected_text);
  }

  void Announce(const std::u16string& text) override { text_ = text; }
  void AnnounceAfterDelay(const std::u16string& text,
                          base::TimeDelta delay) override {
    text_ = text;
  }

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
    announcement_view_ = std::make_unique<TestAnnouncementView>();
    controller_ = std::make_unique<AssistiveWindowController>(
        delegate_.get(), profile_.get(), announcement_view_.get());
    IMEBridge::Get()->SetAssistiveWindowHandler(controller_.get());

    // TODO(crbug.com/40138718): Create MockSuggestionWindowView to be
    // independent of SuggestionWindowView's implementation.
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

  void WaitForSuggestionWindowDelay() {
    task_environment()->FastForwardBy(
        base::Milliseconds(kShowSuggestionDelay + 1));
  }

  std::unique_ptr<AssistiveWindowController> controller_;
  std::unique_ptr<MockDelegate> delegate_ = std::make_unique<MockDelegate>();
  std::unique_ptr<TestingProfile> profile_;
  const std::u16string suggestion_ = u"test";
  ui::ime::AssistiveWindowButton emoji_button_;
  AssistiveWindowProperties emoji_window_;
  std::unique_ptr<TestAnnouncementView> announcement_view_;
};

TEST_F(AssistiveWindowControllerTest, ShowSuggestionDelaysWindowDisplay) {
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
  AssistiveWindowProperties properties;
  properties.type = ash::ime::AssistiveWindowType::kEmojiSuggestion;
  properties.visible = true;
  properties.candidates = std::vector<std::u16string>({u"candidate"});
  controller_->SetAssistiveWindowProperties(properties);

  gfx::Rect new_caret_bounds(current_bounds.width() + 1,
                             current_bounds.height());
  Bounds bounds;
  bounds.caret = new_caret_bounds;
  controller_->SetBounds(bounds);
  EXPECT_EQ(controller_->GetSuggestionWindowViewForTesting()->GetAnchorRect(),
            new_caret_bounds);
}

TEST_F(AssistiveWindowControllerTest, SetsUndoWindowAnchorRectCorrectly) {
  gfx::Rect autocorrect_bounds(1, 1);
  gfx::Rect caret_bounds(2, 2);

  Bounds bounds;
  bounds.caret = caret_bounds;
  bounds.autocorrect = autocorrect_bounds;
  controller_->SetBounds(bounds);

  AssistiveWindowProperties window;
  window.type = ash::ime::AssistiveWindowType::kUndoWindow;
  window.visible = true;
  controller_->SetAssistiveWindowProperties(window);

  ASSERT_TRUE(controller_->GetUndoWindowForTesting() != nullptr);
  autocorrect_bounds.Inset(-4);
  EXPECT_EQ(controller_->GetUndoWindowForTesting()->GetAnchorRect(),
            autocorrect_bounds);
}

TEST_F(AssistiveWindowControllerTest, SetsEmojiWindowOrientationVertical) {
  // Create new suggestion window.
  AssistiveWindowProperties properties;
  properties.type = ash::ime::AssistiveWindowType::kEmojiSuggestion;
  properties.visible = true;
  properties.candidates = std::vector<std::u16string>({u"candidate"});
  controller_->SetAssistiveWindowProperties(properties);

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
  // Create new suggestion window.
  AssistiveWindowProperties properties;
  properties.type = ash::ime::AssistiveWindowType::kPersonalInfoSuggestion;
  properties.visible = true;
  properties.candidates = std::vector<std::u16string>({u"candidate"});
  controller_->SetAssistiveWindowProperties(properties);

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
  // Create new suggestion window.
  AssistiveWindowProperties properties;
  properties.type = ash::ime::AssistiveWindowType::kMultiWordSuggestion;
  properties.visible = true;
  properties.candidates = std::vector<std::u16string>({u"candidate"});
  controller_->SetAssistiveWindowProperties(properties);

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
  // Create new suggestion window.
  AssistiveWindowProperties properties;
  properties.type =
      ash::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion;
  properties.visible = true;
  properties.candidates = std::vector<std::u16string>({u"candidate"});
  controller_->SetAssistiveWindowProperties(properties);

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
  AssistiveWindowProperties init_vertical_properties;
  init_vertical_properties.type =
      ash::ime::AssistiveWindowType::kMultiWordSuggestion;
  init_vertical_properties.visible = true;
  init_vertical_properties.candidates =
      std::vector<std::u16string>({u"vertical"});
  controller_->SetAssistiveWindowProperties(init_vertical_properties);

  AssistiveWindowProperties horizontal_properties;
  horizontal_properties.type =
      ash::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion;
  horizontal_properties.visible = true;
  horizontal_properties.candidates =
      std::vector<std::u16string>({u"candidate"});
  controller_->SetAssistiveWindowProperties(horizontal_properties);

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
  AssistiveWindowProperties init_horizontal_properties;
  init_horizontal_properties.type =
      ash::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion;
  init_horizontal_properties.visible = true;
  init_horizontal_properties.candidates =
      std::vector<std::u16string>({u"horizontal"});
  controller_->SetAssistiveWindowProperties(init_horizontal_properties);

  AssistiveWindowProperties vertical_properties;
  vertical_properties.type =
      ash::ime::AssistiveWindowType::kMultiWordSuggestion;
  vertical_properties.visible = true;
  vertical_properties.candidates = std::vector<std::u16string>({u"candidate"});
  controller_->SetAssistiveWindowProperties(vertical_properties);

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

  announcement_view_->VerifyAnnouncement(kAnnounceString);
}

TEST_F(
    AssistiveWindowControllerTest,
    DoesNotAnnounceWhenSetButtonHighlightedInEmojiWindowDoesNotHaveAnnounceString) {
  profile_->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled, true);
  InitEmojiSuggestionWindow();
  InitEmojiButton();
  emoji_button_.announce_string.clear();

  controller_->SetAssistiveWindowProperties(emoji_window_);
  controller_->SetButtonHighlighted(emoji_button_, true);
  task_environment()->RunUntilIdle();

  announcement_view_->VerifyAnnouncement(std::u16string());
}

TEST_F(AssistiveWindowControllerTest,
       AnnouncesWhenSetButtonHighlightedInUndoWindowHasAnnounceString) {
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

  announcement_view_->VerifyAnnouncement(kAnnounceString);
}

TEST_F(
    AssistiveWindowControllerTest,
    DoesNotAnnounceWhenSetButtonHighlightedAndSuggestionWindowViewIsNotActive) {
  profile_->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled, true);
  InitEmojiButton();

  controller_->SetButtonHighlighted(emoji_button_, true);
  task_environment()->RunUntilIdle();

  announcement_view_->VerifyAnnouncement(std::u16string());
}

}  // namespace input_method
}  // namespace ash
