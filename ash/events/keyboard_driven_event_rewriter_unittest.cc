// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/events/keyboard_driven_event_rewriter.h"

#include <stddef.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/test/test_event_rewriter_continuation.h"

namespace ash {

class TestEventRewriterContinuation
    : public ui::test::TestEventRewriterContinuation {
 public:
  TestEventRewriterContinuation() = default;

  TestEventRewriterContinuation(const TestEventRewriterContinuation&) = delete;
  TestEventRewriterContinuation& operator=(
      const TestEventRewriterContinuation&) = delete;

  ~TestEventRewriterContinuation() override = default;

  ui::EventDispatchDetails SendEvent(const ui::Event* event) override {
    passthrough_event = event->Clone();
    return ui::EventDispatchDetails();
  }

  ui::EventDispatchDetails SendEventFinally(const ui::Event* event) override {
    rewritten_event = event->Clone();
    return ui::EventDispatchDetails();
  }

  ui::EventDispatchDetails DiscardEvent() override {
    return ui::EventDispatchDetails();
  }

  std::unique_ptr<ui::Event> passthrough_event;
  std::unique_ptr<ui::Event> rewritten_event;

  base::WeakPtrFactory<TestEventRewriterContinuation> weak_ptr_factory_{this};
};

class KeyboardDrivenEventRewriterTest : public testing::Test {
 public:
  KeyboardDrivenEventRewriterTest() = default;

  KeyboardDrivenEventRewriterTest(const KeyboardDrivenEventRewriterTest&) =
      delete;
  KeyboardDrivenEventRewriterTest& operator=(
      const KeyboardDrivenEventRewriterTest&) = delete;

  ~KeyboardDrivenEventRewriterTest() override = default;

 protected:
  std::string GetRewrittenEventAsString(ui::KeyboardCode ui_keycode,
                                        int ui_flags,
                                        ui::EventType ui_type) {
    TestEventRewriterContinuation continuation;
    ui::KeyEvent keyevent(ui_type, ui_keycode, ui_flags);
    rewriter_.RewriteForTesting(keyevent,
                                continuation.weak_ptr_factory_.GetWeakPtr());

    std::string result = "No event is sent by RewriteEvent.";
    if (continuation.passthrough_event) {
      result = base::StringPrintf("PassThrough ui_flags=%d",
                                  continuation.passthrough_event->flags());
    } else if (continuation.rewritten_event) {
      result = base::StringPrintf("Rewritten ui_flags=%d",
                                  continuation.rewritten_event->flags());
    }
    return result;
  }

  KeyboardDrivenEventRewriter rewriter_;
};

TEST_F(KeyboardDrivenEventRewriterTest, PassThrough) {
  struct {
    ui::KeyboardCode ui_keycode;
    int ui_flags;
  } kTests[] = {
    { ui::VKEY_A, ui::EF_NONE },
    { ui::VKEY_A, ui::EF_CONTROL_DOWN },
    { ui::VKEY_A, ui::EF_ALT_DOWN },
    { ui::VKEY_A, ui::EF_SHIFT_DOWN },
    { ui::VKEY_A, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN },
    { ui::VKEY_A, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN },

    { ui::VKEY_LEFT, ui::EF_NONE },
    { ui::VKEY_LEFT, ui::EF_CONTROL_DOWN },
    { ui::VKEY_LEFT, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN },

    { ui::VKEY_RIGHT, ui::EF_NONE },
    { ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN },
    { ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN },

    { ui::VKEY_UP, ui::EF_NONE },
    { ui::VKEY_UP, ui::EF_CONTROL_DOWN },
    { ui::VKEY_UP, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN },

    { ui::VKEY_DOWN, ui::EF_NONE },
    { ui::VKEY_DOWN, ui::EF_CONTROL_DOWN },
    { ui::VKEY_DOWN, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN },

    { ui::VKEY_RETURN, ui::EF_NONE },
    { ui::VKEY_RETURN, ui::EF_CONTROL_DOWN },
    { ui::VKEY_RETURN, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN },
  };

  for (size_t i = 0; i < std::size(kTests); ++i) {
    EXPECT_EQ(
        base::StringPrintf("PassThrough ui_flags=%d", kTests[i].ui_flags),
        GetRewrittenEventAsString(kTests[i].ui_keycode, kTests[i].ui_flags,
                                  ui::EventType::kKeyPressed))
        << "Test case " << i;
  }
}

TEST_F(KeyboardDrivenEventRewriterTest, Rewrite) {
  const int kModifierMask = ui::EF_SHIFT_DOWN;

  struct {
    ui::KeyboardCode ui_keycode;
    int ui_flags;
  } kTests[] = {
    { ui::VKEY_LEFT, kModifierMask },
    { ui::VKEY_RIGHT, kModifierMask },
    { ui::VKEY_UP, kModifierMask },
    { ui::VKEY_DOWN, kModifierMask },
    { ui::VKEY_RETURN, kModifierMask },
    { ui::VKEY_F6, kModifierMask },
  };

  for (size_t i = 0; i < std::size(kTests); ++i) {
    EXPECT_EQ(
        "Rewritten ui_flags=0",
        GetRewrittenEventAsString(kTests[i].ui_keycode, kTests[i].ui_flags,
                                  ui::EventType::kKeyPressed))
        << "Test case " << i;
  }
}

}  // namespace ash
