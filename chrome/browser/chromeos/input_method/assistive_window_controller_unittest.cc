// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/assistive_window_controller.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/input_method/assistive_window_controller_delegate.h"
#include "chrome/browser/chromeos/input_method/ui/suggestion_details.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/wm/core/window_util.h"

namespace {
const char kAnnounceString[] = "announce string";
}  // namespace

namespace chromeos {
namespace input_method {

class MockDelegate : public AssistiveWindowControllerDelegate {
 public:
  ~MockDelegate() override = default;
  void AssistiveWindowButtonClicked(
      const ui::ime::AssistiveWindowButton& button) const override {}
};

class TestTtsHandler : public TtsHandler {
 public:
  explicit TestTtsHandler(Profile* profile) : TtsHandler(profile) {}

  void VerifyAnnouncement(const std::string& expected_text) {
    EXPECT_EQ(text_, expected_text);
  }

 private:
  void Speak(const std::string& text) override { text_ = text; }

  std::string text_ = "";
};

class AssistiveWindowControllerTest : public ChromeAshTestBase {
 protected:
  AssistiveWindowControllerTest() { ui::IMEBridge::Initialize(); }
  ~AssistiveWindowControllerTest() override { ui::IMEBridge::Shutdown(); }

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    auto tts_handler = std::make_unique<TestTtsHandler>(profile_.get());
    tts_handler_ = tts_handler.get();

    controller_ = std::make_unique<AssistiveWindowController>(
        delegate_.get(), profile_.get(), std::move(tts_handler));
    ui::IMEBridge::Get()->SetAssistiveWindowHandler(controller_.get());

    ChromeAshTestBase::SetUp();
    std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(1));
    wm::ActivateWindow(window.get());

    // TODO(crbug/1102283): Create MockSuggestionWindowView to be independent of
    // SuggestionWindowView's implementation.
    static_cast<views::TestViewsDelegate*>(views::ViewsDelegate::GetInstance())
        ->set_layout_provider(ChromeLayoutProvider::CreateLayoutProvider());
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

  std::unique_ptr<AssistiveWindowController> controller_;
  std::unique_ptr<MockDelegate> delegate_ = std::make_unique<MockDelegate>();
  std::unique_ptr<TestingProfile> profile_;
  const std::u16string suggestion_ = u"test";
  ui::ime::AssistiveWindowButton emoji_button_;
  chromeos::AssistiveWindowProperties emoji_window_;
  TestTtsHandler* tts_handler_;

  void TearDown() override {
    controller_.reset();
    ChromeAshTestBase::TearDown();
  }
};

TEST_F(AssistiveWindowControllerTest, ConfirmedLength0SetsSuggestionViewBound) {
  // Sets up suggestion_view with confirmed_length = 0.
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
  gfx::Rect new_caret_bounds(current_bounds.width() + 1,
                             current_bounds.height());
  Bounds bounds;
  bounds.caret = new_caret_bounds;
  ui::IMEBridge::Get()->GetAssistiveWindowHandler()->SetBounds(bounds);
  EXPECT_EQ(new_caret_bounds, suggestion_view->GetAnchorRect());
}

TEST_F(AssistiveWindowControllerTest,
       ConfirmedLengthNot0DoesNotSetSuggestionViewBound) {
  // Sets up suggestion_view with confirmed_length = 1.
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
  gfx::Rect new_caret_bounds(current_bounds.width() + 1,
                             current_bounds.height());
  Bounds bounds;
  bounds.caret = new_caret_bounds;
  ui::IMEBridge::Get()->GetAssistiveWindowHandler()->SetBounds(bounds);
  EXPECT_EQ(current_bounds, suggestion_view->GetAnchorRect());
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

  tts_handler_->VerifyAnnouncement(kAnnounceString);
}

TEST_F(AssistiveWindowControllerTest,
       DoesNotAnnounceWhenSetButtonHighlightedAndChromeVoxIsOff) {
  profile_->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled, false);
  InitEmojiSuggestionWindow();
  InitEmojiButton();

  ui::IMEBridge::Get()
      ->GetAssistiveWindowHandler()
      ->SetAssistiveWindowProperties(emoji_window_);
  ui::IMEBridge::Get()->GetAssistiveWindowHandler()->SetButtonHighlighted(
      emoji_button_, true);
  task_environment()->RunUntilIdle();

  tts_handler_->VerifyAnnouncement(base::EmptyString());
}

TEST_F(
    AssistiveWindowControllerTest,
    DoesNotAnnounceWhenSetButtonHighlightedInEmojiWindowDoesNotHaveAnnounceString) {
  profile_->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled, true);
  InitEmojiSuggestionWindow();
  InitEmojiButton();
  emoji_button_.announce_string = base::EmptyString();

  ui::IMEBridge::Get()
      ->GetAssistiveWindowHandler()
      ->SetAssistiveWindowProperties(emoji_window_);
  ui::IMEBridge::Get()->GetAssistiveWindowHandler()->SetButtonHighlighted(
      emoji_button_, true);
  task_environment()->RunUntilIdle();

  tts_handler_->VerifyAnnouncement(base::EmptyString());
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

  tts_handler_->VerifyAnnouncement(kAnnounceString);
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

  std::string expected_announcement = base::EmptyString();
  tts_handler_->VerifyAnnouncement(base::EmptyString());
}

}  // namespace input_method
}  // namespace chromeos
