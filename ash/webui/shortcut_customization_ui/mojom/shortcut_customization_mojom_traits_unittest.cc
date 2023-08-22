// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization_mojom_traits.h"

#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

using ash::shortcut_customization::mojom::SimpleAccelerator;

class ShortcutCustomizationStructTraitsTest : public testing::Test {
 public:
  ShortcutCustomizationStructTraitsTest() = default;
  ~ShortcutCustomizationStructTraitsTest() override = default;
};

TEST_F(ShortcutCustomizationStructTraitsTest,
       SerializeAndDeserializeValidModifiers) {
  ui::Accelerator accelerator(ui::KeyboardCode::VKEY_TAB, ui::EF_CONTROL_DOWN,
                              ui::Accelerator::KeyState::RELEASED);
  ui::Accelerator deserialized;
  ASSERT_TRUE(SimpleAccelerator::Deserialize(
      SimpleAccelerator::Serialize(&accelerator), &deserialized));
  EXPECT_EQ(accelerator, deserialized);
}

TEST_F(ShortcutCustomizationStructTraitsTest,
       SerializeAndDeserializeInvalidModifiers) {
  ui::Accelerator accelerator(ui::KeyboardCode::VKEY_TAB,
                              ui::EF_CONTROL_DOWN | ui::EF_IME_FABRICATED_KEY);

  // `EF_IME_FABRICATED_KEY` is an invalid modifier, it verify it was stripped
  // off after serializing/deserializing.
  ui::Accelerator expected_accelerator(
      ui::KeyboardCode::VKEY_TAB,
      ui::EF_CONTROL_DOWN | ui::EF_IME_FABRICATED_KEY);
  ui::Accelerator deserialized;
  ASSERT_TRUE(SimpleAccelerator::Deserialize(
      SimpleAccelerator::Serialize(&accelerator), &deserialized));
  EXPECT_EQ(expected_accelerator, deserialized);
}
}  // namespace ash
