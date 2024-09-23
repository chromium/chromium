// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/ime/key_event_result_receiver.h"

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace arc {

class KeyEventResultReceiverTest : public testing::Test {
 public:
  KeyEventResultReceiverTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        receiver_() {}
  ~KeyEventResultReceiverTest() override = default;

  KeyEventResultReceiver* receiver() { return &receiver_; }

  void ForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  base::test::TaskEnvironment task_environment_;

  KeyEventResultReceiver receiver_;
};

TEST_F(KeyEventResultReceiverTest, ExpireCallback) {
  std::optional<bool> result;
  auto callback =
      base::BindLambdaForTesting([&result](bool res) { result = res; });
  ui::KeyEvent event = ui::KeyEvent::FromCharacter(
      'a', ui::VKEY_A, ui::DomCode::NONE, ui::EF_NONE);

  receiver()->SetCallback(std::move(callback), &event);
  EXPECT_FALSE(result.has_value());

  ForwardBy(base::Seconds(1));

  EXPECT_TRUE(result.has_value());
  EXPECT_FALSE(result.value());
}

TEST_F(KeyEventResultReceiverTest, EventStoppedPropagation) {
  std::optional<bool> result;
  auto callback =
      base::BindLambdaForTesting([&result](bool res) { result = res; });
  ui::KeyEvent event = ui::KeyEvent::FromCharacter(
      'a', ui::VKEY_A, ui::DomCode::NONE, ui::EF_NONE);

  receiver()->SetCallback(std::move(callback), &event);
  EXPECT_FALSE(result.has_value());

  event.StopPropagation();
  receiver()->DispatchKeyEventPostIME(&event);

  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result.value());
}

TEST_F(KeyEventResultReceiverTest, EventConsumedByIME) {
  std::optional<bool> result;
  auto callback =
      base::BindLambdaForTesting([&result](bool res) { result = res; });
  ui::KeyEvent event{ui::EventType::kKeyPressed, ui::VKEY_PROCESSKEY,
                     ui::DomCode::NONE,          ui::EF_IS_SYNTHESIZED,
                     ui::DomKey::PROCESS,        ui::EventTimeForNow()};

  receiver()->SetCallback(std::move(callback), &event);
  EXPECT_FALSE(result.has_value());

  receiver()->DispatchKeyEventPostIME(&event);

  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result.value());
}

TEST_F(KeyEventResultReceiverTest, EventNotCharacter) {
  std::optional<bool> result;
  ui::KeyEvent event{ui::EventType::kKeyPressed, ui::VKEY_LEFT,
                     ui::DomCode::ARROW_LEFT,    ui::EF_NONE,
                     ui::DomKey::ARROW_LEFT,     ui::EventTimeForNow()};
  auto callback =
      base::BindLambdaForTesting([&result](bool res) { result = res; });

  receiver()->SetCallback(std::move(callback), &event);
  EXPECT_FALSE(result.has_value());

  receiver()->DispatchKeyEventPostIME(&event);

  // A KeyEvent with no character is sent to ARC.
  EXPECT_TRUE(result.has_value());
  EXPECT_FALSE(result.value());
}

TEST_F(KeyEventResultReceiverTest, UnmodifiedEnterAndBackspace) {
  std::optional<bool> result;
  auto callback =
      base::BindLambdaForTesting([&result](bool res) { result = res; });

  ui::KeyEvent event{ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                     ui::DomCode::ENTER,         ui::EF_NONE,
                     ui::DomKey::ENTER,          ui::EventTimeForNow()};

  receiver()->SetCallback(std::move(callback), &event);
  EXPECT_FALSE(result.has_value());

  receiver()->DispatchKeyEventPostIME(&event);

  // An Enter key event without modifiers is sent to ARC.
  EXPECT_TRUE(result.has_value());
  EXPECT_FALSE(result.value());

  result.reset();

  ui::KeyEvent event2{ui::EventType::kKeyPressed, ui::VKEY_BACK,
                      ui::DomCode::BACKSPACE,     ui::EF_NONE,
                      ui::DomKey::BACKSPACE,      ui::EventTimeForNow()};
  auto callback2 =
      base::BindLambdaForTesting([&result](bool res) { result = res; });
  receiver()->SetCallback(std::move(callback2), &event2);

  receiver()->DispatchKeyEventPostIME(&event2);

  // A Backspace key event without modifiers is sent to ARC as well.
  EXPECT_TRUE(result.has_value());
  EXPECT_FALSE(result.value());
}

TEST_F(KeyEventResultReceiverTest, ControlCharacters) {
  std::optional<bool> result;
  ui::KeyEvent event = ui::KeyEvent::FromCharacter(
      'a', ui::VKEY_A, ui::DomCode::NONE, ui::EF_CONTROL_DOWN);
  auto callback =
      base::BindLambdaForTesting([&result](bool res) { result = res; });

  receiver()->SetCallback(std::move(callback), &event);
  EXPECT_FALSE(result.has_value());

  receiver()->DispatchKeyEventPostIME(&event);

  // Ctrl-A should be sent to the proxy IME.
  EXPECT_TRUE(result.has_value());
  EXPECT_FALSE(result.value());
}

TEST_F(KeyEventResultReceiverTest, EventWithSystemModifier) {
  std::optional<bool> result;
  ui::KeyEvent event = ui::KeyEvent::FromCharacter(
      'a', ui::VKEY_A, ui::DomCode::NONE, ui::EF_ALT_DOWN);
  auto callback =
      base::BindLambdaForTesting([&result](bool res) { result = res; });

  receiver()->SetCallback(std::move(callback), &event);
  EXPECT_FALSE(result.has_value());

  receiver()->DispatchKeyEventPostIME(&event);

  // Alt-A should be sent to the proxy IME.
  EXPECT_TRUE(result.has_value());
  EXPECT_FALSE(result.value());
}

TEST_F(KeyEventResultReceiverTest, NormalCharacters) {
  std::optional<bool> result;
  ui::KeyEvent event = ui::KeyEvent::FromCharacter(
      'a', ui::VKEY_A, ui::DomCode::NONE, ui::EF_NONE);
  auto callback =
      base::BindLambdaForTesting([&result](bool res) { result = res; });

  receiver()->SetCallback(std::move(callback), &event);
  EXPECT_FALSE(result.has_value());

  receiver()->DispatchKeyEventPostIME(&event);

  // 'A' key should be sent to the proxy IME.
  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result.value());
}

TEST_F(KeyEventResultReceiverTest, DifferentEvent) {
  std::optional<bool> result;
  ui::KeyEvent event = ui::KeyEvent::FromCharacter(
      'a', ui::VKEY_A, ui::DomCode::NONE, ui::EF_NONE);
  ui::KeyEvent event2 = ui::KeyEvent::FromCharacter(
      'b', ui::VKEY_B, ui::DomCode::NONE, ui::EF_NONE);
  auto callback =
      base::BindLambdaForTesting([&result](bool res) { result = res; });

  receiver()->SetCallback(std::move(callback), &event);
  EXPECT_FALSE(result.has_value());

  receiver()->DispatchKeyEventPostIME(&event2);

  // The callback should not be called with a different event.
  EXPECT_FALSE(result.has_value());

  receiver()->DispatchKeyEventPostIME(&event);

  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result.value());
}

TEST_F(KeyEventResultReceiverTest, ProcessedKey) {
  std::optional<bool> result;
  ui::KeyEvent event = ui::KeyEvent::FromCharacter(
      'a', ui::VKEY_A, ui::DomCode::NONE, ui::EF_NONE);
  ui::KeyEvent event2 = ui::KeyEvent::FromCharacter(
      'b', ui::VKEY_PROCESSKEY, ui::DomCode::NONE, ui::EF_NONE);
  auto callback =
      base::BindLambdaForTesting([&result](bool res) { result = res; });

  receiver()->SetCallback(std::move(callback), &event);
  EXPECT_FALSE(result.has_value());

  receiver()->DispatchKeyEventPostIME(&event2);

  // The callback should be called with a VKEY_PROCESSKEY event.
  EXPECT_TRUE(result.has_value());
}

}  // namespace arc
