// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/autoclick/autoclick_drag_event_rewriter.h"

#include <memory>

#include "ash/accessibility/test_event_recorder.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/event_sink.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/test_event_source.h"

namespace ash {

namespace {

// EventSink that saves a copy of the most recent event.
class CopyingSink : public ui::EventSink {
 public:
  CopyingSink() {}
  ~CopyingSink() override = default;
  ui::Event* last_event() const { return last_event_.get(); }

  // EventSink override:
  ui::EventDispatchDetails OnEventFromSource(ui::Event* event) override {
    last_event_ = event->Clone();
    ui::Event::DispatcherApi(last_event_.get()).set_target(event->target());
    return ui::EventDispatchDetails();
  }

 private:
  std::unique_ptr<ui::Event> last_event_;
};

}  // anonymous namespace

class AutoclickDragEventRewriterTest : public AshTestBase {
 public:
  AutoclickDragEventRewriterTest() = default;

  AutoclickDragEventRewriterTest(const AutoclickDragEventRewriterTest&) =
      delete;
  AutoclickDragEventRewriterTest& operator=(
      const AutoclickDragEventRewriterTest&) = delete;

  ~AutoclickDragEventRewriterTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    generator_ = AshTestBase::GetEventGenerator();
    GetContext()->GetHost()->GetEventSource()->AddEventRewriter(
        &drag_event_rewriter_);
    GetContext()->GetHost()->GetEventSource()->AddEventRewriter(
        &event_recorder_);
  }

  void TearDown() override {
    GetContext()->GetHost()->GetEventSource()->RemoveEventRewriter(
        &event_recorder_);
    GetContext()->GetHost()->GetEventSource()->RemoveEventRewriter(
        &drag_event_rewriter_);
    generator_ = nullptr;
    AshTestBase::TearDown();
  }

 protected:
  // Generates ui::Events from simulated user input.
  raw_ptr<ui::test::EventGenerator> generator_ = nullptr;
  // Records events delivered to the next event rewriter after
  // AutoclickDragEventRewriter.
  TestEventRecorder event_recorder_;

  AutoclickDragEventRewriter drag_event_rewriter_;
};

TEST_F(AutoclickDragEventRewriterTest, EventsNotConsumedWhenDisabled) {
  drag_event_rewriter_.SetEnabled(false);
  // Events are not consume.
  generator_->PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(1U, event_recorder_.recorded_event_count());
  generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(2U, event_recorder_.recorded_event_count());
  generator_->PressLeftButton();
  EXPECT_EQ(3U, event_recorder_.recorded_event_count());
  EXPECT_EQ(ui::EventType::kMousePressed,
            event_recorder_.last_recorded_event_type());
  generator_->MoveMouseTo(gfx::Point(200, 200), 1);
  EXPECT_EQ(4U, event_recorder_.recorded_event_count());
  EXPECT_EQ(ui::EventType::kMouseDragged,
            event_recorder_.last_recorded_event_type());
  generator_->ReleaseLeftButton();
  EXPECT_EQ(5U, event_recorder_.recorded_event_count());
  EXPECT_EQ(ui::EventType::kMouseReleased,
            event_recorder_.last_recorded_event_type());

  // Move events are not consumed either.
  generator_->MoveMouseTo(gfx::Point(100, 100), 1);
  EXPECT_EQ(6U, event_recorder_.recorded_event_count());
  EXPECT_EQ(ui::EventType::kMouseMoved,
            event_recorder_.last_recorded_event_type());
}

TEST_F(AutoclickDragEventRewriterTest, OnlyMouseMoveEventsConsumedWhenEnabled) {
  drag_event_rewriter_.SetEnabled(true);
  // Most events are still not consumed.
  generator_->PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(1U, event_recorder_.recorded_event_count());
  generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(2U, event_recorder_.recorded_event_count());
  generator_->PressLeftButton();
  EXPECT_EQ(3U, event_recorder_.recorded_event_count());
  EXPECT_EQ(ui::EventType::kMousePressed,
            event_recorder_.last_recorded_event_type());
  generator_->MoveMouseTo(gfx::Point(200, 200), 1);
  EXPECT_EQ(4U, event_recorder_.recorded_event_count());
  EXPECT_EQ(ui::EventType::kMouseDragged,
            event_recorder_.last_recorded_event_type());
  generator_->ReleaseLeftButton();
  EXPECT_EQ(5U, event_recorder_.recorded_event_count());
  EXPECT_EQ(ui::EventType::kMouseReleased,
            event_recorder_.last_recorded_event_type());

  // Mouse move events are consumed and changed into drag events.
  generator_->MoveMouseTo(gfx::Point(100, 100), 1);
  EXPECT_EQ(5U, event_recorder_.recorded_event_count());
  generator_->MoveMouseTo(gfx::Point(150, 150), 1);
  EXPECT_EQ(5U, event_recorder_.recorded_event_count());
}

TEST_F(AutoclickDragEventRewriterTest, RewritesMouseMovesToDrags) {
  drag_event_rewriter_.SetEnabled(true);
  base::TimeTicks time_stamp = ui::EventTimeForNow();
  gfx::Point location(100, 100);
  gfx::Point root_location(150, 150);
  int flags = ui::EF_SHIFT_DOWN;                        // Set a random flag.
  int changed_button_flags = ui::EF_LEFT_MOUSE_BUTTON;  // Set a random flag.
  ui::MouseEvent event(ui::EventType::kMouseMoved, location, root_location,
                       time_stamp, flags, changed_button_flags);

  auto window = CreateToplevelTestWindow(gfx::Rect(50, 50, 400, 300),
                                         /*shell_window_id=*/0);
  ASSERT_NE(window.get(), nullptr);
  ui::Event::DispatcherApi(&event).set_target(window.get());

  CopyingSink sink;
  ui::test::TestEventSource source(&sink);
  source.AddEventRewriter(&drag_event_rewriter_);
  source.Send(&event);
  source.RemoveEventRewriter(&drag_event_rewriter_);
  ui::Event* rewritten_event = sink.last_event();

  // The type should be a drag.
  ASSERT_EQ(ui::EventType::kMouseDragged, rewritten_event->type());

  // Flags should include left mouse button.
  EXPECT_EQ(flags | ui::EF_LEFT_MOUSE_BUTTON, rewritten_event->flags());

  // Original event target should be honored.
  EXPECT_EQ(window.get(), rewritten_event->target());

  // Everything else should be the same as the original.
  ui::MouseEvent* rewritten_mouse_event = rewritten_event->AsMouseEvent();
  EXPECT_EQ(location, rewritten_mouse_event->location());
  EXPECT_EQ(root_location, rewritten_mouse_event->root_location());
  EXPECT_EQ(time_stamp, rewritten_mouse_event->time_stamp());
  EXPECT_EQ(changed_button_flags,
            rewritten_mouse_event->changed_button_flags());
}

}  // namespace ash
