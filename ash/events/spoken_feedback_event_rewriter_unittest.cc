// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/spoken_feedback_event_rewriter.h"

#include <memory>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

// An event rewriter that simply records all events that it receives.
class EventRecorder : public ui::EventRewriter {
 public:
  EventRecorder() = default;
  ~EventRecorder() override = default;

  // ui::EventRewriter:
  ui::EventRewriteStatus RewriteEvent(
      const ui::Event& event,
      std::unique_ptr<ui::Event>* new_event) override {
    recorded_event_count_++;
    return ui::EVENT_REWRITE_CONTINUE;
  }
  ui::EventRewriteStatus NextDispatchEvent(
      const ui::Event& last_event,
      std::unique_ptr<ui::Event>* new_event) override {
    NOTREACHED();
    return ui::EVENT_REWRITE_CONTINUE;
  }

  // Count of events sent to the rewriter.
  size_t recorded_event_count_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(EventRecorder);
};

// A test implementation of the spoken feedback delegate interface.
class TestDelegate : public mojom::SpokenFeedbackEventRewriterDelegate {
 public:
  TestDelegate() : binding_(this) {}
  ~TestDelegate() override = default;

  mojom::SpokenFeedbackEventRewriterDelegatePtr BindInterface() {
    mojom::SpokenFeedbackEventRewriterDelegatePtr ptr;
    binding_.Bind(MakeRequest(&ptr));
    return ptr;
  }

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

  // The binding that backs the interface pointer held by the event rewriter.
  mojo::Binding<ash::mojom::SpokenFeedbackEventRewriterDelegate> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

class SpokenFeedbackEventRewriterTest : public ash::AshTestBase {
 public:
  SpokenFeedbackEventRewriterTest() {
    spoken_feedback_event_rewriter_.SetDelegate(delegate_.BindInterface());
  }

  void SetUp() override {
    ash::AshTestBase::SetUp();
    generator_ = AshTestBase::GetEventGenerator();
    CurrentContext()->GetHost()->GetEventSource()->AddEventRewriter(
        &spoken_feedback_event_rewriter_);
    CurrentContext()->GetHost()->GetEventSource()->AddEventRewriter(
        &event_recorder_);
  }

  void TearDown() override {
    CurrentContext()->GetHost()->GetEventSource()->RemoveEventRewriter(
        &event_recorder_);
    CurrentContext()->GetHost()->GetEventSource()->RemoveEventRewriter(
        &spoken_feedback_event_rewriter_);
    generator_ = nullptr;
    ash::AshTestBase::TearDown();
  }

  // Flush any messages to the test delegate and return events it has recorded.
  size_t GetDelegateRecordedEventCount() {
    spoken_feedback_event_rewriter_.get_delegate_for_testing()
        ->FlushForTesting();
    return delegate_.recorded_event_count_;
  }

  size_t GetDelegateCapturedEventCount() {
    spoken_feedback_event_rewriter_.get_delegate_for_testing()
        ->FlushForTesting();
    return delegate_.captured_event_count_;
  }

  void SetDelegateCaptureAllKeys(bool value) {
    spoken_feedback_event_rewriter_.set_capture_all_keys(value);
  }

  void ExpectCounts(size_t expected_recorded_count,
                    size_t expected_delegate_count,
                    size_t expected_captured_count) {
    EXPECT_EQ(expected_recorded_count, event_recorder_.recorded_event_count_);
    EXPECT_EQ(expected_delegate_count, GetDelegateRecordedEventCount());
    EXPECT_EQ(expected_captured_count, GetDelegateCapturedEventCount());
  }

 protected:
  // A test spoken feedback delegate; simulates ChromeVox.
  TestDelegate delegate_;
  // Generates ui::Events from simulated user input.
  ui::test::EventGenerator* generator_ = nullptr;
  // Records events delivered to the next event rewriter after spoken feedback.
  EventRecorder event_recorder_;

  SpokenFeedbackEventRewriter spoken_feedback_event_rewriter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SpokenFeedbackEventRewriterTest);
};

// The delegate should not intercept events when spoken feedback is disabled.
TEST_F(SpokenFeedbackEventRewriterTest, EventsNotConsumedWhenDisabled) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->IsSpokenFeedbackEnabled());

  generator_->PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(1U, event_recorder_.recorded_event_count_);
  EXPECT_EQ(0U, GetDelegateRecordedEventCount());
  generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(2U, event_recorder_.recorded_event_count_);
  EXPECT_EQ(0U, GetDelegateRecordedEventCount());

  generator_->ClickLeftButton();
  EXPECT_EQ(4U, event_recorder_.recorded_event_count_);
  EXPECT_EQ(0U, GetDelegateRecordedEventCount());

  generator_->GestureTapAt(gfx::Point());
  EXPECT_EQ(6U, event_recorder_.recorded_event_count_);
  EXPECT_EQ(0U, GetDelegateRecordedEventCount());
}

// The delegate should intercept key events when spoken feedback is enabled.
TEST_F(SpokenFeedbackEventRewriterTest, KeyEventsConsumedWhenEnabled) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(controller->IsSpokenFeedbackEnabled());

  generator_->PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(1U, event_recorder_.recorded_event_count_);
  EXPECT_EQ(1U, GetDelegateRecordedEventCount());
  EXPECT_EQ(0U, GetDelegateCapturedEventCount());
  generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(2U, event_recorder_.recorded_event_count_);
  EXPECT_EQ(2U, GetDelegateRecordedEventCount());
  EXPECT_EQ(0U, GetDelegateCapturedEventCount());

  generator_->ClickLeftButton();
  EXPECT_EQ(4U, event_recorder_.recorded_event_count_);
  EXPECT_EQ(2U, GetDelegateRecordedEventCount());
  EXPECT_EQ(0U, GetDelegateCapturedEventCount());

  generator_->GestureTapAt(gfx::Point());
  EXPECT_EQ(6U, event_recorder_.recorded_event_count_);
  EXPECT_EQ(2U, GetDelegateRecordedEventCount());
  EXPECT_EQ(0U, GetDelegateCapturedEventCount());
}

// Asynchronously unhandled events should be sent to subsequent rewriters.
TEST_F(SpokenFeedbackEventRewriterTest, UnhandledEventsSentToOtherRewriters) {
  spoken_feedback_event_rewriter_.OnUnhandledSpokenFeedbackEvent(
      std::make_unique<ui::KeyEvent>(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                     ui::EF_NONE));
  EXPECT_EQ(1U, event_recorder_.recorded_event_count_);
  spoken_feedback_event_rewriter_.OnUnhandledSpokenFeedbackEvent(
      std::make_unique<ui::KeyEvent>(ui::ET_KEY_RELEASED, ui::VKEY_A,
                                     ui::EF_NONE));
  EXPECT_EQ(2U, event_recorder_.recorded_event_count_);
}

TEST_F(SpokenFeedbackEventRewriterTest, KeysNotEatenWithChromeVoxDisabled) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->IsSpokenFeedbackEnabled());

  // Send Search+Shift+Right.
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_EQ(1U, event_recorder_.recorded_event_count_);
  generator_->PressKey(ui::VKEY_SHIFT, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(2U, event_recorder_.recorded_event_count_);

  // Mock successful commands lookup and dispatch; shouldn't matter either way.
  generator_->PressKey(ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(3U, event_recorder_.recorded_event_count_);

  // Released keys shouldn't get eaten.
  generator_->ReleaseKey(ui::VKEY_RIGHT,
                         ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  generator_->ReleaseKey(ui::VKEY_SHIFT, ui::EF_COMMAND_DOWN);
  generator_->ReleaseKey(ui::VKEY_LWIN, 0);
  EXPECT_EQ(6U, event_recorder_.recorded_event_count_);

  // Try releasing more keys.
  generator_->ReleaseKey(ui::VKEY_RIGHT, 0);
  generator_->ReleaseKey(ui::VKEY_SHIFT, 0);
  generator_->ReleaseKey(ui::VKEY_LWIN, 0);
  EXPECT_EQ(9U, event_recorder_.recorded_event_count_);

  EXPECT_EQ(0U, GetDelegateRecordedEventCount());
}

TEST_F(SpokenFeedbackEventRewriterTest, KeyEventsCaptured) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(controller->IsSpokenFeedbackEnabled());

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

}  // namespace
}  // namespace ash
