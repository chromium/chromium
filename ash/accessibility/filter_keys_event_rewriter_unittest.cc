// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/filter_keys_event_rewriter.h"

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

class FilterKeysEventRewriterTest : public AshTestBase {
 public:
  FilterKeysEventRewriterTest() = default;
  FilterKeysEventRewriterTest(const FilterKeysEventRewriterTest&) = delete;
  FilterKeysEventRewriterTest& operator=(const FilterKeysEventRewriterTest&) =
      delete;
  ~FilterKeysEventRewriterTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    event_rewriter_ = std::make_unique<FilterKeysEventRewriter>();
    generator_ = AshTestBase::GetEventGenerator();
    GetContext()->GetHost()->GetEventSource()->AddEventRewriter(
        event_rewriter());
    GetContext()->GetHost()->GetEventSource()->AddEventRewriter(
        &event_recorder_);
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
  FilterKeysEventRewriter* event_rewriter() { return event_rewriter_.get(); }

 private:
  // Generates ui::Events to simulate user input.
  raw_ptr<ui::test::EventGenerator> generator_ = nullptr;
  // Records events delivered to the next event rewriter after
  // FilterKeysEventRewriter.
  TestEventRecorder event_recorder_;
  // The FilterKeysEventRewriter instance.
  std::unique_ptr<FilterKeysEventRewriter> event_rewriter_;
};

TEST_F(FilterKeysEventRewriterTest, KeyboardEventsNotCanceledIfDisabled) {
  event_rewriter()->SetBounceKeysEnabled(false);
  generator()->PressKey(ui::VKEY_A, ui::EF_NONE);
  ASSERT_EQ(1U, event_recorder()->events().size());
  ASSERT_EQ(ui::EventType::kKeyPressed,
            event_recorder()->events().back()->type());
  generator()->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  ASSERT_EQ(2u, event_recorder()->events().size());
  ASSERT_EQ(ui::EventType::kKeyReleased,
            event_recorder()->events().back()->type());
}

TEST_F(FilterKeysEventRewriterTest, MouseButtonsNotCanceledIfDisabled) {
  event_rewriter()->SetBounceKeysEnabled(false);
  generator()->PressLeftButton();
  EXPECT_EQ(1U, event_recorder()->events().size());
  EXPECT_EQ(ui::EventType::kMousePressed,
            event_recorder()->events().back()->type());
  generator()->ReleaseLeftButton();
  EXPECT_EQ(2U, event_recorder()->events().size());
  EXPECT_EQ(ui::EventType::kMouseReleased,
            event_recorder()->events().back()->type());
}

TEST_F(FilterKeysEventRewriterTest, KeyboardEventsNotCanceled) {
  event_rewriter()->SetBounceKeysEnabled(true);
  generator()->PressKey(ui::VKEY_A, ui::EF_NONE);
  ASSERT_EQ(1U, event_recorder()->events().size());
  ASSERT_EQ(ui::EventType::kKeyPressed,
            event_recorder()->events().back()->type());
  generator()->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  ASSERT_EQ(2u, event_recorder()->events().size());
  ASSERT_EQ(ui::EventType::kKeyReleased,
            event_recorder()->events().back()->type());
}

TEST_F(FilterKeysEventRewriterTest, MouseButtonsNotCanceled) {
  event_rewriter()->SetBounceKeysEnabled(true);
  generator()->PressLeftButton();
  EXPECT_EQ(1U, event_recorder()->events().size());
  EXPECT_EQ(ui::EventType::kMousePressed,
            event_recorder()->events().back()->type());
  generator()->ReleaseLeftButton();
  EXPECT_EQ(2U, event_recorder()->events().size());
  EXPECT_EQ(ui::EventType::kMouseReleased,
            event_recorder()->events().back()->type());
}

}  // namespace ash
