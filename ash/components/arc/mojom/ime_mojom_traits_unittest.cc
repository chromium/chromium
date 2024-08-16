// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/components/arc/mojom/ime.mojom.h"

#include <linux/input.h>

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace mojo {

namespace {

void ExpectKeyEventsEqual(const ui::KeyEvent& expected,
                          const ui::KeyEvent& actual) {
  EXPECT_EQ(expected.type(), actual.type());
  EXPECT_EQ(expected.key_code(), actual.key_code());
  EXPECT_EQ(expected.code(), actual.code());
  EXPECT_EQ(expected.IsShiftDown(), actual.IsShiftDown());
  EXPECT_EQ(expected.IsAltDown(), actual.IsAltDown());
  EXPECT_EQ(expected.IsAltGrDown(), actual.IsAltGrDown());
  EXPECT_EQ(expected.IsControlDown(), actual.IsControlDown());
  EXPECT_EQ(expected.IsCapsLockOn(), actual.IsCapsLockOn());
  EXPECT_EQ(expected.is_repeat(), actual.is_repeat());
}

}  // namespace

TEST(KeyEventStructTraitsTest, Convert) {
  const ui::KeyEvent kTestData[] = {
      {ui::EventType::kKeyPressed, ui::VKEY_A, ui::DomCode::US_A,
       ui::EF_CONTROL_DOWN},
      {ui::EventType::kKeyPressed, ui::VKEY_B, ui::DomCode::US_B,
       ui::EF_ALT_DOWN},
      {ui::EventType::kKeyReleased, ui::VKEY_B, ui::DomCode::US_B,
       ui::EF_SHIFT_DOWN},
      {ui::EventType::kKeyPressed, ui::VKEY_A, ui::DomCode::US_A,
       ui::EF_CAPS_LOCK_ON},
      {ui::EventType::kKeyPressed, ui::VKEY_C, ui::DomCode::US_C,
       ui::EF_ALTGR_DOWN},
      {ui::EventType::kKeyPressed, ui::VKEY_A, ui::DomCode::US_A,
       ui::EF_IS_REPEAT},
  };
  for (size_t idx = 0; idx < std::size(kTestData); ++idx) {
    auto copy = std::make_unique<ui::KeyEvent>(kTestData[idx]);
    std::unique_ptr<ui::KeyEvent> output;
    mojo::test::SerializeAndDeserialize<arc::mojom::KeyEventData>(copy, output);
    ExpectKeyEventsEqual(*copy, *output);
  }
}

TEST(KeyEventStructTraitsTest, UseScancodeIfAvailable) {
  auto original = std::make_unique<ui::KeyEvent>(
      ui::EventType::kKeyPressed, ui::VKEY_UNKNOWN, ui::DomCode::NONE,
      ui::EF_NONE);
  original->set_scan_code(KEY_A);
  std::unique_ptr<ui::KeyEvent> output;
  mojo::test::SerializeAndDeserialize<arc::mojom::KeyEventData>(original,
                                                                output);
  EXPECT_EQ(original->type(), output->type());
  EXPECT_EQ(ui::DomCode::US_A, output->code());
  EXPECT_EQ(ui::VKEY_A, output->key_code());
}

}  // namespace mojo
