// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/disable_trackpad_event_rewriter.h"

#include <memory>
#include <vector>

#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"

namespace {

class EventCapturer : public ui::EventRewriter {
 public:
  EventCapturer() = default;
  EventCapturer(const EventCapturer&) = delete;
  EventCapturer& operator=(const EventCapturer&) = delete;
  ~EventCapturer() override = default;

  // ui::EventRewriter:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override {
    events_.push_back(event.Clone());
    return SendEvent(continuation, &event);
  }

  const std::vector<std::unique_ptr<ui::Event>>& events() { return events_; }

 private:
  std::vector<std::unique_ptr<ui::Event>> events_;
};

}  // namespace

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
        &event_capturer_);
    event_rewriter()->SetEnabled(true);
  }

  void TearDown() override {
    GetContext()->GetHost()->GetEventSource()->RemoveEventRewriter(
        &event_capturer_);
    GetContext()->GetHost()->GetEventSource()->RemoveEventRewriter(
        event_rewriter());
    event_rewriter_.reset();
    generator_ = nullptr;
    AshTestBase::TearDown();
  }

  ui::test::EventGenerator* generator() { return generator_; }
  EventCapturer* event_capturer() { return &event_capturer_; }
  DisableTrackpadEventRewriter* event_rewriter() {
    return event_rewriter_.get();
  }

 private:
  // Generates ui::Events to simulate user input.
  raw_ptr<ui::test::EventGenerator> generator_ = nullptr;
  // Records events delivered to the next event rewriter after
  // DisableTrackpadEventRewriter.
  EventCapturer event_capturer_;
  // The DisableTrackpadEventRewriter instance.
  std::unique_ptr<DisableTrackpadEventRewriter> event_rewriter_;
};

TEST_F(DisableTrackpadEventRewriterTest, KeyboardEventsNotCanceledIfDisabled) {
  event_rewriter()->SetEnabled(false);
  generator()->PressKey(ui::VKEY_A, ui::EF_NONE);
  ASSERT_EQ(1U, event_capturer()->events().size());
  ASSERT_EQ(ui::EventType::kKeyPressed,
            event_capturer()->events().back()->type());
  generator()->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  ASSERT_EQ(2u, event_capturer()->events().size());
  ASSERT_EQ(ui::EventType::kKeyReleased,
            event_capturer()->events().back()->type());
}

TEST_F(DisableTrackpadEventRewriterTest, MouseButtonsNotCanceledIfDisabled) {
  event_rewriter()->SetEnabled(false);
  generator()->PressLeftButton();
  EXPECT_EQ(1U, event_capturer()->events().size());
  EXPECT_EQ(ui::EventType::kMousePressed,
            event_capturer()->events().back()->type());
  generator()->ReleaseLeftButton();
  EXPECT_EQ(2U, event_capturer()->events().size());
  EXPECT_EQ(ui::EventType::kMouseReleased,
            event_capturer()->events().back()->type());
}

TEST_F(DisableTrackpadEventRewriterTest, KeyboardEventsNotCanceled) {
  generator()->PressKey(ui::VKEY_A, ui::EF_NONE);
  ASSERT_EQ(1U, event_capturer()->events().size());
  ASSERT_EQ(ui::EventType::kKeyPressed,
            event_capturer()->events().back()->type());
  generator()->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  ASSERT_EQ(2u, event_capturer()->events().size());
  ASSERT_EQ(ui::EventType::kKeyReleased,
            event_capturer()->events().back()->type());
}

TEST_F(DisableTrackpadEventRewriterTest, MouseButtonsNotCanceled) {
  generator()->PressLeftButton();
  EXPECT_EQ(1U, event_capturer()->events().size());
  EXPECT_EQ(ui::EventType::kMousePressed,
            event_capturer()->events().back()->type());
  generator()->ReleaseLeftButton();
  EXPECT_EQ(2U, event_capturer()->events().size());
  EXPECT_EQ(ui::EventType::kMouseReleased,
            event_capturer()->events().back()->type());
}

}  // namespace ash
