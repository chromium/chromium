// Copyright (c) 2020 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/ash/input_method/ui/suggestion_details.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/base/ime/ash/ime_assistive_window_handler_interface.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/wm/core/window_util.h"

namespace {
const char16_t kAnnounceString[] = u"announce string";
}  // namespace

namespace ash {
namespace input_method {

class MockDelegate : public AssistiveWindowControllerDelegate {
 public:
  ~MockDelegate() override = default;
  void AssistiveWindowButtonClicked(
      const ui::ime::AssistiveWindowButton& button) const override {}
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
  AssistiveWindowControllerTest() = default;
  ~AssistiveWindowControllerTest() override = default;

  void SetUp() override {
    ChromeAshTestBase::SetUp();
    std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(1));
    wm::ActivateWindow(window.get());

    profile_ = std::make_unique<TestingProfile>();
    accessibility_view_ = std::make_unique<TestAccessibilityView>();
    controller_ = std::make_unique<AssistiveWindowController>(
        delegate_.get(), profile_.get(), accessibility_view_.get());
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
    emoji_window_.type = ui::ime::AssistiveWindowType::kEmojiSuggestion;
    emoji_window_.visible = true;
    emoji_window_.candidates = Candidates();
  }

  void InitEmojiButton() {
    emoji_button_.window_type = ui::ime::AssistiveWindowType::kEmojiSuggestion;
    emoji_button_.announce_string = kAnnounceString;
  }

  void EnableLacros() {
    feature_list_.Reset();
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kLacrosSupport},
        /*disabled_features=*/{});
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<AssistiveWindowController> controller_;
  std::unique_ptr<MockDelegate> delegate_ = std::make_unique<MockDelegate>();
  std::unique_ptr<TestingProfile> profile_;
  const std::u16string suggestion_ = u"test";
  ui::ime::AssistiveWindowButton emoji_button_;
  AssistiveWindowProperties emoji_window_;
  std::unique_ptr<TestAccessibilityView> accessibility_view_;
};

TEST_F(AssistiveWindowControllerTest, ConfirmedLength0SetsBoundsToCaretBounds) {
  ui::ime::SuggestionDetails details;
  details.text = suggestion_;
  details.confirmed_length = 0;
  ui::IMEBridge::Get()->GetAssistiveWindowHandler()->ShowSuggestion(details);
  ui::ime::SuggestionWindowView* suggestion_view =
      controller_->GetSuggestionWindowViewForTesting();
  EXPECT_EQ(
      0u,
      ui::IMEBridge::Get()->GetAssistiveWindowHandler()->GetConfirmedLength());

  gfx::Rect current_bounds = suggestion_view->GetAnchorRect();
  gfx::Rect caret_bounds(0, 0, 100, 100);
  gfx::Rect composition_bounds(0, 0, 90, 100);
  Bounds bounds;
  bounds.caret = caret_bounds;
  bounds.composition_text = composition_bounds;
  ui::IMEBridge::Get()->GetAssistiveWindowHandler()->SetBounds(bounds);

  EXPECT_NE(current_bounds, suggestion_view->GetAnchorRect());
  EXPECT_EQ(caret_bounds, suggestion_view->GetAnchorRect());
}

TEST_F(AssistiveWindowControllerTest,
       ConfirmedLengthNSetsBoundsToCompositionTextBounds) {
  ui::ime::SuggestionDetails details;
  details.text = suggestion_;
  details.confirmed_length = 1;
  ui::IMEBridge::Get()->GetAssistiveWindowHandler()->ShowSuggestion(details);
  ui::ime::SuggestionWindowView* suggestion_view =
      controller_->GetSuggestionWindowViewForTesting();
  EXPECT_EQ(
      1u,
      ui::IMEBridge::Get()->GetAssistiveWindowHandler()->GetConfirmedLength());

  gfx::Rect current_bounds = suggestion_view->GetAnchorRect();
  gfx::Rect caret_bounds(0, 0, 100, 100);
  gfx::Rect composition_bounds(0, 0, 90, 100);
  Bounds bounds;
  bounds.caret = caret_bounds;
  bounds.composition_text = composition_bounds;
  ui::IMEBridge::Get()->GetAssistiveWindowHandler()->SetBounds(bounds);

  EXPECT_NE(current_bounds, suggestion_view->GetAnchorRect());
  EXPECT_EQ(composition_bounds, suggestion_view->GetAnchorRect());
}

TEST_F(AssistiveWindowControllerTest,
       ConfirmedLengthNSetsBoundsToCaretBoundsWithLacrosEnabled) {
  EnableLacros();
  ui::ime::SuggestionDetails details;
  details.text = suggestion_;
  details.confirmed_length = 1;
  ui::IMEBridge::Get()->GetAssistiveWindowHandler()->ShowSuggestion(details);
  ui::ime::SuggestionWindowView* suggestion_view =
      controller_->GetSuggestionWindowViewForTesting();
  EXPECT_EQ(
      1u,
      ui::IMEBridge::Get()->GetAssistiveWindowHandler()->GetConfirmedLength());

  gfx::Rect current_bounds = suggestion_view->GetAnchorRect();
  gfx::Rect caret_bounds(0, 0, 100, 100);
  gfx::Rect composition_bounds(0, 0, 90, 100);
  Bounds bounds;
  bounds.caret = caret_bounds;
  bounds.composition_text = composition_bounds;
  ui::IMEBridge::Get()->GetAssistiveWindowHandler()->SetBounds(bounds);

  EXPECT_NE(current_bounds, suggestion_view->GetAnchorRect());
  EXPECT_EQ(caret_bounds, suggestion_view->GetAnchorRect());
}

TEST_F(AssistiveWindowControllerTest,
       ShowingSuggestionForFirstTimeShouldRepositionWindow) {
  ui::ime::SuggestionDetails details;
  details.text = suggestion_;
  details.confirmed_length = 0;
  IMEAssistiveWindowHandlerInterface* assistive_window =
      ui::IMEBridge::Get()->GetAssistiveWindowHandler();
  assistive_window->ShowSuggestion(details);
  ui::ime::SuggestionWindowView* suggestion_view =
      controller_->GetSuggestionWindowViewForTesting();

  gfx::Rect current_bounds = suggestion_view->GetAnchorRect();
  gfx::Rect caret_bounds_after_one_key(current_bounds.width() + 1,
                                       current_bounds.height());
  gfx::Rect caret_bounds_after_two_key(current_bounds.width() + 2,
                                       current_bounds.height());

  // One char entered
  assistive_window->SetBounds(Bounds{.caret = caret_bounds_after_one_key});

  // Mimic tracking the last suggestion
  details.confirmed_length = 1;
  assistive_window->ShowSuggestion(details);

  // Second char entered to text input
  assistive_window->SetBounds(Bounds{.caret = caret_bounds_after_two_key});

  // Second `SetBounds` should be ignored
  EXPECT_EQ(caret_bounds_after_one_key, suggestion_view->GetAnchorRect());
}

TEST_F(AssistiveWindowControllerTest,
       SuggestionViewBoundIsResetAfterHideSuggestionThenShowAgain) {
  // Sets up suggestion_view with confirmed_length = 1.
  ui::ime::SuggestionDetails details;
  details.text = suggestion_;
  details.confirmed_length = 1;
  ui::IMEBridge::Get()->GetAssistiveWindowHandler()->ShowSuggestion(details);
  EXPECT_EQ(
      1u,
      ui::IMEBridge::Get()->GetAssistiveWindowHandler()->GetConfirmedLength());

  gfx::Rect current_bounds =
      controller_->GetSuggestionWindowViewForTesting()->GetAnchorRect();
  ui::IMEBridge::Get()->GetAssistiveWindowHandler()->HideSuggestion();

  // Create new suggestion window.
  AssistiveWindowProperties properties;
  properties.type = ui::ime::AssistiveWindowType::kEmojiSuggestion;
  properties.visible = true;
  properties.candidates = std::vector<std::u16string>({u"candidate"});
  ui::IMEBridge::Get()
      ->GetAssistiveWindowHandler()
      ->SetAssistiveWindowProperties(properties);

  gfx::Rect new_caret_bounds(current_bounds.width() + 1,
                             current_bounds.height());
  Bounds bounds;
  bounds.caret = new_caret_bounds;
  ui::IMEBridge::Get()->GetAssistiveWindowHandler()->SetBounds(bounds);
  EXPECT_EQ(new_caret_bounds,
            controller_->GetSuggestionWindowViewForTesting()->GetAnchorRect());
}

TEST_F(AssistiveWindowControllerTest, SetsUndoWindowAnchorRectCorrectly) {
  gfx::Rect autocorrect_bounds(1, 1);
  gfx::Rect caret_bounds(2, 2);

  Bounds bounds;
  bounds.caret = caret_bounds;
  bounds.autocorrect = autocorrect_bounds;
  ui::IMEBridge::Get()->GetAssistiveWindowHandler()->SetBounds(bounds);

  AssistiveWindowProperties window;
  window.type = ui::ime::AssistiveWindowType::kUndoWindow;
  window.visible = true;
  ui::IMEBridge::Get()
      ->GetAssistiveWindowHandler()
      ->SetAssistiveWindowProperties(window);

  ASSERT_TRUE(controller_->GetUndoWindowForTesting() != nullptr);
  autocorrect_bounds.Inset(-4);
  EXPECT_EQ(autocorrect_bounds,
            controller_->GetUndoWindowForTesting()->GetAnchorRect());
}

TEST_F(AssistiveWindowControllerTest,
       AnnouncesWhenSetButtonHighlightedInEmojiWindowHasAnnounceString) {
  profile_->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled, true);
  InitEmojiSuggestionWindow();
  InitEmojiButton();

  ui::IMEBridge::Get()
      ->GetAssistiveWindowHandler()
      ->SetAssistiveWindowProperties(emoji_window_);
  ui::IMEBridge::Get()->GetAssistiveWindowHandler()->SetButtonHighlighted(
      emoji_button_, true);
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

  ui::IMEBridge::Get()
      ->GetAssistiveWindowHandler()
      ->SetAssistiveWindowProperties(emoji_window_);
  ui::IMEBridge::Get()->GetAssistiveWindowHandler()->SetButtonHighlighted(
      emoji_button_, true);
  task_environment()->RunUntilIdle();

  accessibility_view_->VerifyAnnouncement(base::EmptyString16());
}

TEST_F(AssistiveWindowControllerTest,
       AnnouncesWhenSetButtonHighlightedInUndoWindowHasAnnounceString) {
  profile_->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled, true);
  AssistiveWindowProperties window;
  window.type = ui::ime::AssistiveWindowType::kUndoWindow;
  window.visible = true;
  ui::ime::AssistiveWindowButton button;
  button.window_type = ui::ime::AssistiveWindowType::kUndoWindow;
  button.announce_string = kAnnounceString;

  ui::IMEBridge::Get()
      ->GetAssistiveWindowHandler()
      ->SetAssistiveWindowProperties(window);
  ui::IMEBridge::Get()->GetAssistiveWindowHandler()->SetButtonHighlighted(
      button, true);
  task_environment()->RunUntilIdle();

  accessibility_view_->VerifyAnnouncement(kAnnounceString);
}

TEST_F(
    AssistiveWindowControllerTest,
    DoesNotAnnounceWhenSetButtonHighlightedAndSuggestionWindowViewIsNotActive) {
  profile_->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled, true);
  InitEmojiButton();

  ui::IMEBridge::Get()->GetAssistiveWindowHandler()->SetButtonHighlighted(
      emoji_button_, true);
  task_environment()->RunUntilIdle();

  accessibility_view_->VerifyAnnouncement(base::EmptyString16());
}

}  // namespace input_method
}  // namespace ash
