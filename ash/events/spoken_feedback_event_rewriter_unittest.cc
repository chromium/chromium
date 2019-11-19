// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/spoken_feedback_event_rewriter.h"

#include <memory>
#include <vector>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/spoken_feedback_event_rewriter_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/chromeos/events/event_rewriter_chromeos.h"
#include "ui/chromeos/events/modifier_key.h"
#include "ui/chromeos/events/pref_names.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/test_event_rewriter.h"

namespace ash {
namespace {

// A test implementation of the spoken feedback delegate interface.
class TestDelegate : public SpokenFeedbackEventRewriterDelegate {
 public:
  TestDelegate() = default;
  ~TestDelegate() override = default;

  // Count of events sent to the delegate.
  size_t recorded_event_count_ = 0;

  // Count of captured events sent to the delegate.
  size_t captured_event_count_ = 0;

 private:
  // SpokenFeedbackEventRewriterDelegate:
  void DispatchKeyEventToChromeVox(std::unique_ptr<ui::Event> event,
                                   bool capture) override {
    recorded_event_count_++;
    if (capture)
      captured_event_count_++;
  }
  void DispatchMouseEventToChromeVox(
      std::unique_ptr<ui::Event> event) override {
    recorded_event_count_++;
  }

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

class SpokenFeedbackEventRewriterTest
    : public ash::AshTestBase,
      public ui::EventRewriterChromeOS::Delegate {
 public:
  SpokenFeedbackEventRewriterTest() {
    event_rewriter_chromeos_ =
        std::make_unique<ui::EventRewriterChromeOS>(this, nullptr);
    spoken_feedback_event_rewriter_ =
        std::make_unique<SpokenFeedbackEventRewriter>(
            event_rewriter_chromeos_.get());
  }

  void SetUp() override {
    ash::AshTestBase::SetUp();
    generator_ = AshTestBase::GetEventGenerator();
    spoken_feedback_event_rewriter_->set_delegate(&delegate_);
    CurrentContext()->GetHost()->GetEventSource()->AddEventRewriter(
        spoken_feedback_event_rewriter_.get());
    CurrentContext()->GetHost()->GetEventSource()->AddEventRewriter(
        &event_recorder_);
  }

  void TearDown() override {
    CurrentContext()->GetHost()->GetEventSource()->RemoveEventRewriter(
        &event_recorder_);
    CurrentContext()->GetHost()->GetEventSource()->RemoveEventRewriter(
        spoken_feedback_event_rewriter_.get());
    spoken_feedback_event_rewriter_->set_delegate(nullptr);
    generator_ = nullptr;
    ash::AshTestBase::TearDown();
  }

  size_t delegate_recorded_event_count() {
    return delegate_.recorded_event_count_;
  }

  size_t delegate_captured_event_count() {
    return delegate_.captured_event_count_;
  }

  void SetDelegateCaptureAllKeys(bool value) {
    spoken_feedback_event_rewriter_->set_capture_all_keys(value);
  }

  void ExpectCounts(size_t expected_recorded_count,
                    size_t expected_delegate_count,
                    size_t expected_captured_count) {
    EXPECT_EQ(expected_recorded_count,
              static_cast<size_t>(event_recorder_.events_seen()));
    EXPECT_EQ(expected_delegate_count, delegate_recorded_event_count());
    EXPECT_EQ(expected_captured_count, delegate_captured_event_count());
  }

  void SetModifierRemapping(const std::string& pref_name,
                            ui::chromeos::ModifierKey value) {
    modifier_remapping_[pref_name] = static_cast<int>(value);
  }

 protected:
  // A test spoken feedback delegate; simulates ChromeVox.
  TestDelegate delegate_;
  // Generates ui::Events from simulated user input.
  ui::test::EventGenerator* generator_ = nullptr;
  // Records events delivered to the next event rewriter after spoken feedback.
  ui::test::TestEventRewriter event_recorder_;

  std::unique_ptr<SpokenFeedbackEventRewriter> spoken_feedback_event_rewriter_;

  std::unique_ptr<ui::EventRewriterChromeOS> event_rewriter_chromeos_;

 private:
  // ui::EventRewriterChromeOS::Delegate:
  bool RewriteModifierKeys() override { return true; }

  bool GetKeyboardRemappedPrefValue(const std::string& pref_name,
                                    int* value) const override {
    auto it = modifier_remapping_.find(pref_name);
    if (it == modifier_remapping_.end())
      return false;

    *value = it->second;
    return true;
  }

  bool TopRowKeysAreFunctionKeys() const override { return false; }

  bool IsExtensionCommandRegistered(ui::KeyboardCode key_code,
                                    int flags) const override {
    return false;
  }

  bool IsSearchKeyAcceleratorReserved() const override { return false; }

  std::map<std::string, int> modifier_remapping_;

  DISALLOW_COPY_AND_ASSIGN(SpokenFeedbackEventRewriterTest);
};

// The delegate should not intercept events when spoken feedback is disabled.
TEST_F(SpokenFeedbackEventRewriterTest, EventsNotConsumedWhenDisabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->spoken_feedback_enabled());

  generator_->PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(1, event_recorder_.events_seen());
  EXPECT_EQ(0U, delegate_recorded_event_count());
  generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(2, event_recorder_.events_seen());
  EXPECT_EQ(0U, delegate_recorded_event_count());

  generator_->ClickLeftButton();
  EXPECT_EQ(4, event_recorder_.events_seen());
  EXPECT_EQ(0U, delegate_recorded_event_count());

  generator_->GestureTapAt(gfx::Point());
  EXPECT_EQ(6, event_recorder_.events_seen());
  EXPECT_EQ(0U, delegate_recorded_event_count());
}

// The delegate should intercept key events when spoken feedback is enabled.
TEST_F(SpokenFeedbackEventRewriterTest, KeyEventsConsumedWhenEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(controller->spoken_feedback_enabled());

  generator_->PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(1, event_recorder_.events_seen());
  EXPECT_EQ(1U, delegate_recorded_event_count());
  EXPECT_EQ(0U, delegate_captured_event_count());
  generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(2, event_recorder_.events_seen());
  EXPECT_EQ(2U, delegate_recorded_event_count());
  EXPECT_EQ(0U, delegate_captured_event_count());

  generator_->ClickLeftButton();
  EXPECT_EQ(4, event_recorder_.events_seen());
  EXPECT_EQ(2U, delegate_recorded_event_count());
  EXPECT_EQ(0U, delegate_captured_event_count());

  generator_->GestureTapAt(gfx::Point());
  EXPECT_EQ(6, event_recorder_.events_seen());
  EXPECT_EQ(2U, delegate_recorded_event_count());
  EXPECT_EQ(0U, delegate_captured_event_count());
}

// Asynchronously unhandled events should be sent to subsequent rewriters.
TEST_F(SpokenFeedbackEventRewriterTest, UnhandledEventsSentToOtherRewriters) {
  // Before it can forward unhandled events, SpokenFeedbackEventRewriter
  // must have seen at least one event in the first place.
  generator_->PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(1, event_recorder_.events_seen());
  generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(2, event_recorder_.events_seen());

  spoken_feedback_event_rewriter_->OnUnhandledSpokenFeedbackEvent(
      std::make_unique<ui::KeyEvent>(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                     ui::EF_NONE));
  EXPECT_EQ(3, event_recorder_.events_seen());
  spoken_feedback_event_rewriter_->OnUnhandledSpokenFeedbackEvent(
      std::make_unique<ui::KeyEvent>(ui::ET_KEY_RELEASED, ui::VKEY_A,
                                     ui::EF_NONE));
  EXPECT_EQ(4, event_recorder_.events_seen());
}

TEST_F(SpokenFeedbackEventRewriterTest, KeysNotEatenWithChromeVoxDisabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->spoken_feedback_enabled());

  // Send Search+Shift+Right.
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_EQ(1, event_recorder_.events_seen());
  generator_->PressKey(ui::VKEY_SHIFT, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(2, event_recorder_.events_seen());

  // Mock successful commands lookup and dispatch; shouldn't matter either way.
  generator_->PressKey(ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(3, event_recorder_.events_seen());

  // Released keys shouldn't get eaten.
  generator_->ReleaseKey(ui::VKEY_RIGHT,
                         ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  generator_->ReleaseKey(ui::VKEY_SHIFT, ui::EF_COMMAND_DOWN);
  generator_->ReleaseKey(ui::VKEY_LWIN, 0);
  EXPECT_EQ(6, event_recorder_.events_seen());

  // Try releasing more keys.
  generator_->ReleaseKey(ui::VKEY_RIGHT, 0);
  generator_->ReleaseKey(ui::VKEY_SHIFT, 0);
  generator_->ReleaseKey(ui::VKEY_LWIN, 0);
  EXPECT_EQ(9, event_recorder_.events_seen());

  EXPECT_EQ(0U, delegate_recorded_event_count());
}

TEST_F(SpokenFeedbackEventRewriterTest, KeyEventsCaptured) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(controller->spoken_feedback_enabled());

  // Initialize expected counts as variables for easier maintaiblity.
  size_t recorded_count = 0;
  size_t delegate_count = 0;
  size_t captured_count = 0;

  // Anything with Search gets captured.
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);

  // Tab never gets captured.
  generator_->PressKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
  generator_->ReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);

  // A client requested capture of all keys.
  SetDelegateCaptureAllKeys(true);
  generator_->PressKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);
  generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);

  // Tab never gets captured even with explicit client request for all keys.
  generator_->PressKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
  generator_->ReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);

  // A client requested to not capture all keys.
  SetDelegateCaptureAllKeys(false);
  generator_->PressKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
  generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
}

TEST_F(SpokenFeedbackEventRewriterTest,
       KeyEventsCapturedWithModifierRemapping) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(controller->spoken_feedback_enabled());

  // Initialize expected counts as variables for easier maintaiblity.
  size_t recorded_count = 0;
  size_t delegate_count = 0;
  size_t captured_count = 0;

  // Map Control key to Search.
  SetModifierRemapping(prefs::kLanguageRemapControlKeyTo,
                       ui::chromeos::ModifierKey::kSearchKey);

  // Anything with Search gets captured.
  generator_->PressKey(ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);
  // EventRewriterChromeOS actually omits the modifier flag.
  generator_->ReleaseKey(ui::VKEY_CONTROL, 0);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);

  // Search itself should also work.
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);
  generator_->ReleaseKey(ui::VKEY_LWIN, 0);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);

  // Remapping should have no effect on all other expectations.

  // Tab never gets captured.
  generator_->PressKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
  generator_->ReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);

  // A client requested capture of all keys.
  SetDelegateCaptureAllKeys(true);
  generator_->PressKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);
  generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);

  // Tab never gets captured even with explicit client request for all keys.
  generator_->PressKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
  generator_->ReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);

  // A client requested to not capture all keys.
  SetDelegateCaptureAllKeys(false);
  generator_->PressKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
  generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
}

}  // namespace
}  // namespace ash
