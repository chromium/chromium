// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/disable_trackpad_event_rewriter.h"

#include <memory>
#include <vector>

#include "ash/accessibility/test_event_recorder.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"

namespace ash {

class DisableTrackpadEventRewriterTest : public AshTestBase {
 public:
  DisableTrackpadEventRewriterTest() = default;
  DisableTrackpadEventRewriterTest(const DisableTrackpadEventRewriterTest&) =
      delete;
  DisableTrackpadEventRewriterTest& operator=(
      const DisableTrackpadEventRewriterTest&) = delete;
  ~DisableTrackpadEventRewriterTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    event_rewriter_ = std::make_unique<DisableTrackpadEventRewriter>();
    generator_ = AshTestBase::GetEventGenerator();
    GetContext()->GetHost()->GetEventSource()->AddEventRewriter(
        event_rewriter());
    GetContext()->GetHost()->GetEventSource()->AddEventRewriter(
        &event_recorder_);
    event_rewriter()->SetEnabled(true);
  }

  void TearDown() override {
    GetContext()->GetHost()->GetEventSource()->RemoveEventRewriter(
        &event_recorder_);
    GetContext()->GetHost()->GetEventSource()->RemoveEventRewriter(
        event_rewriter());
    event_rewriter_.reset();
    generator_ = nullptr;
    AshTestBase::TearDown();
  }

  ui::test::EventGenerator* generator() { return generator_; }
  TestEventRecorder* event_recorder() { return &event_recorder_; }
  DisableTrackpadEventRewriter* event_rewriter() {
    return event_rewriter_.get();
  }

 private:
  // Generates ui::Events to simulate user input.
  raw_ptr<ui::test::EventGenerator> generator_ = nullptr;
  // Records events delivered to the next event rewriter after
  // DisableTrackpadEventRewriter.
  TestEventRecorder event_recorder_;
  // The DisableTrackpadEventRewriter instance.
  std::unique_ptr<DisableTrackpadEventRewriter> event_rewriter_;
};

TEST_F(DisableTrackpadEventRewriterTest, KeyboardEventsNotCanceledIfDisabled) {
  event_rewriter()->SetEnabled(false);
  generator()->PressKey(ui::VKEY_A, ui::EF_NONE);
  ASSERT_EQ(1U, event_recorder()->events().size());
  ASSERT_EQ(ui::EventType::kKeyPressed,
            event_recorder()->events().back()->type());
  generator()->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  ASSERT_EQ(2u, event_recorder()->events().size());
  ASSERT_EQ(ui::EventType::kKeyReleased,
            event_recorder()->events().back()->type());
}

TEST_F(DisableTrackpadEventRewriterTest, MouseButtonsNotCanceledIfDisabled) {
  event_rewriter()->SetEnabled(false);
  generator()->PressLeftButton();
  EXPECT_EQ(1U, event_recorder()->events().size());
  EXPECT_EQ(ui::EventType::kMousePressed,
            event_recorder()->events().back()->type());
  generator()->ReleaseLeftButton();
  EXPECT_EQ(2U, event_recorder()->events().size());
  EXPECT_EQ(ui::EventType::kMouseReleased,
            event_recorder()->events().back()->type());
}

TEST_F(DisableTrackpadEventRewriterTest, KeyboardEventsNotCanceled) {
  generator()->PressKey(ui::VKEY_A, ui::EF_NONE);
  ASSERT_EQ(1U, event_recorder()->events().size());
  ASSERT_EQ(ui::EventType::kKeyPressed,
            event_recorder()->events().back()->type());
  generator()->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  ASSERT_EQ(2u, event_recorder()->events().size());
  ASSERT_EQ(ui::EventType::kKeyReleased,
            event_recorder()->events().back()->type());
}

TEST_F(DisableTrackpadEventRewriterTest, MouseButtonsCanceledWhenEnabled) {
  event_rewriter()->SetEnabled(true);
  generator()->PressLeftButton();
  EXPECT_EQ(0U, event_recorder()->events().size());
  generator()->ReleaseLeftButton();
  EXPECT_EQ(0U, event_recorder()->events().size());
}

TEST_F(DisableTrackpadEventRewriterTest, DisableAfterFiveControlKeyPresses) {
  event_rewriter()->SetEnabled(true);

  int controlKeyPressCount = 0;

  // Simulate pressing and releasing the control key 5 times.
  for (int i = 0; i < 5; ++i) {
    generator()->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
    ++controlKeyPressCount;
    generator()->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);

    // After the 5th press, check if the rewriter is disabled.
    if (controlKeyPressCount == 5) {
      EXPECT_FALSE(event_rewriter()->IsEnabled());
    } else {
      EXPECT_TRUE(event_rewriter()->IsEnabled());
    }
  }
}

}  // namespace ash
