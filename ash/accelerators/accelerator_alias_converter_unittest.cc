// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_alias_converter.h"

#include <vector>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/chromeos/events/keyboard_capability.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

using AcceleratorAliasConverterTest = AshTestBase;

TEST_F(AcceleratorAliasConverterTest, TestCreateTopRowAlias) {
  AcceleratorAliasConverter accelerator_alias_converter_;

  // Top row keys not fKeys prevents remapping.
  EXPECT_FALSE(Shell::Get()->keyboard_capability()->TopRowKeysAreFKeys());
  const ui::Accelerator accelerator1{ui::VKEY_ZOOM, ui::EF_ALT_DOWN};
  std::vector<ui::Accelerator> accelerator_aliases1 =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator1);

  EXPECT_EQ(1u, accelerator_aliases1.size());
  EXPECT_EQ(accelerator1, accelerator_aliases1[0]);

  // Enable top row keys as fKeys.
  Shell::Get()->keyboard_capability()->SetTopRowKeysAsFKeysEnabledForTesting(
      true);
  EXPECT_TRUE(Shell::Get()->keyboard_capability()->TopRowKeysAreFKeys());

  // [Search] as original modifier prevents remapping.
  const ui::Accelerator accelerator2{ui::VKEY_ZOOM, ui::EF_COMMAND_DOWN};
  std::vector<ui::Accelerator> accelerator_aliases2 =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator2);

  EXPECT_EQ(1u, accelerator_aliases2.size());
  EXPECT_EQ(accelerator2, accelerator_aliases2[0]);

  // key_code not as a top row key prevents remapping.
  const ui::Accelerator accelerator3{ui::VKEY_TAB, ui::EF_ALT_DOWN};
  std::vector<ui::Accelerator> accelerator_aliases3 =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator3);

  EXPECT_EQ(1u, accelerator_aliases3.size());
  EXPECT_EQ(accelerator3, accelerator_aliases3[0]);

  // Valid remapping. [Alt] + [Zoom] -> [Alt] + [Search] + [F3].
  // TopRowKeysAreFKeys() remains true.
  const ui::Accelerator accelerator4{ui::VKEY_ZOOM, ui::EF_ALT_DOWN};
  const ui::Accelerator expected_accelerator4{
      ui::VKEY_F3, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN};
  std::vector<ui::Accelerator> accelerator_aliases4 =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator4);

  EXPECT_EQ(1u, accelerator_aliases4.size());
  EXPECT_EQ(expected_accelerator4, accelerator_aliases4[0]);

  // Valid remapping. [Alt] + [Shift] + [Back] -> [Alt] + [Shift] + [Search] +
  // [F1]. TopRowKeysAreFKeys() remains true.
  const ui::Accelerator accelerator5{ui::VKEY_BROWSER_BACK,
                                     ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN};
  const ui::Accelerator expected_accelerator5{
      ui::VKEY_F1, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN};
  std::vector<ui::Accelerator> accelerator_aliases5 =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator5);

  EXPECT_EQ(1u, accelerator_aliases5.size());
  EXPECT_EQ(expected_accelerator5, accelerator_aliases5[0]);
}

TEST_F(AcceleratorAliasConverterTest, TestCreateSixPackAlias) {
  AcceleratorAliasConverter accelerator_alias_converter_;

  // [Search] as original modifier prevents remapping.
  const ui::Accelerator accelerator1{ui::VKEY_ZOOM, ui::EF_COMMAND_DOWN};
  std::vector<ui::Accelerator> accelerator_aliases1 =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator1);

  EXPECT_EQ(1u, accelerator_aliases1.size());
  EXPECT_EQ(accelerator1, accelerator_aliases1[0]);

  // key_code not as six pack key prevents remapping.
  const ui::Accelerator accelerator2{ui::VKEY_TAB, ui::EF_ALT_DOWN};
  std::vector<ui::Accelerator> accelerator_aliases2 =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator2);

  EXPECT_EQ(1u, accelerator_aliases2.size());
  EXPECT_EQ(accelerator2, accelerator_aliases2[0]);

  // [Shift] + [Delete] should not be remapped.
  const ui::Accelerator accelerator3{ui::VKEY_DELETE, ui::EF_SHIFT_DOWN};
  std::vector<ui::Accelerator> accelerator_aliases3 =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator3);

  EXPECT_EQ(1u, accelerator_aliases3.size());
  EXPECT_EQ(accelerator3, accelerator_aliases3[0]);

  // [Shift] + [Insert] should not be remapped.
  const ui::Accelerator accelerator4{ui::VKEY_INSERT, ui::EF_SHIFT_DOWN};
  std::vector<ui::Accelerator> accelerator_aliases4 =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator4);

  EXPECT_EQ(1u, accelerator_aliases4.size());
  EXPECT_EQ(accelerator4, accelerator_aliases4[0]);

  // For Insert: [modifiers] = [Search] + [Shift] +
  // [original_modifiers].
  const ui::Accelerator accelerator5{ui::VKEY_INSERT, ui::EF_ALT_DOWN};
  const ui::Accelerator expected_accelerator5{
      ui::VKEY_BACK, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN};
  std::vector<ui::Accelerator> accelerator_aliases5 =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator5);

  EXPECT_EQ(2u, accelerator_aliases5.size());
  EXPECT_EQ(expected_accelerator5, accelerator_aliases5[0]);
  EXPECT_EQ(accelerator5, accelerator_aliases5[1]);

  // For other six-pack-keys: [modifiers] = [Search] +
  // [original_modifiers].
  const ui::Accelerator accelerator6{ui::VKEY_DELETE, ui::EF_ALT_DOWN};
  const ui::Accelerator expected_accelerator6{
      ui::VKEY_BACK, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN};
  std::vector<ui::Accelerator> accelerator_aliases6 =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator6);

  EXPECT_EQ(2u, accelerator_aliases6.size());
  EXPECT_EQ(expected_accelerator6, accelerator_aliases6[0]);
  EXPECT_EQ(accelerator6, accelerator_aliases6[1]);
}

TEST_F(AcceleratorAliasConverterTest, TestCreateReversedSixPackAlias) {
  AcceleratorAliasConverter accelerator_alias_converter_;

  // [Search] not in modifiers prevents remapping.
  const ui::Accelerator accelerator1{ui::VKEY_LEFT, ui::EF_ALT_DOWN};
  std::vector<ui::Accelerator> accelerator_alias1 =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator1);

  EXPECT_EQ(1u, accelerator_alias1.size());
  EXPECT_EQ(accelerator1, accelerator_alias1[0]);

  // key_code not as reversed six pack key prevent remapping.
  const ui::Accelerator accelerator2{ui::VKEY_ZOOM, ui::EF_COMMAND_DOWN};
  std::vector<ui::Accelerator> accelerator_alias2 =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator2);

  EXPECT_EQ(1u, accelerator_alias2.size());
  EXPECT_EQ(accelerator2, accelerator_alias2[0]);

  // [Search] as the only modifier prevents remapping.
  const ui::Accelerator accelerator3{ui::VKEY_BACK, ui::EF_COMMAND_DOWN};
  std::vector<ui::Accelerator> accelerator_alias3 =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator3);

  EXPECT_EQ(1u, accelerator_alias3.size());
  EXPECT_EQ(accelerator3, accelerator_alias3[0]);

  // [Back] + [Shift] + [Search] only prevents remapping, which is just the
  // reverse of [Insert].
  const ui::Accelerator accelerator4{ui::VKEY_BACK,
                                     ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN};
  std::vector<ui::Accelerator> accelerator_alias4 =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator4);

  EXPECT_EQ(1u, accelerator_alias4.size());
  EXPECT_EQ(accelerator4, accelerator_alias4[0]);

  // [Back] + [Shift] + [Search] + [Alt] maps back to [Insert] + [Alt].
  const ui::Accelerator accelerator5{
      ui::VKEY_BACK, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN};
  const ui::Accelerator expected_accelerator_alias5{ui::VKEY_INSERT,
                                                    ui::EF_ALT_DOWN};
  std::vector<ui::Accelerator> accelerator_alias5 =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator5);

  EXPECT_EQ(2u, accelerator_alias5.size());
  EXPECT_EQ(expected_accelerator_alias5, accelerator_alias5[0]);
  EXPECT_EQ(accelerator5, accelerator_alias5[1]);

  // [Back] + [Search] + [Alt] maps back to [Delete] + [Alt].
  const ui::Accelerator accelerator6{ui::VKEY_BACK,
                                     ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN};
  const ui::Accelerator expected_accelerator_alias6{ui::VKEY_DELETE,
                                                    ui::EF_ALT_DOWN};
  std::vector<ui::Accelerator> accelerator_alias6 =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator6);

  EXPECT_EQ(2u, accelerator_alias6.size());
  EXPECT_EQ(expected_accelerator_alias6, accelerator_alias6[0]);
  EXPECT_EQ(accelerator6, accelerator_alias6[1]);

  // [Left] + [Search] + [Alt] maps back to [Home] + [Alt].
  const ui::Accelerator accelerator7{ui::VKEY_LEFT,
                                     ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN};
  const ui::Accelerator expected_accelerator_alias7{ui::VKEY_HOME,
                                                    ui::EF_ALT_DOWN};
  std::vector<ui::Accelerator> accelerator_alias7 =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator7);

  EXPECT_EQ(2u, accelerator_alias7.size());
  EXPECT_EQ(expected_accelerator_alias7, accelerator_alias7[0]);
  EXPECT_EQ(accelerator7, accelerator_alias7[1]);

  // [Left] + [Search] + [Shift] + [Alt] maps back to [Home] + [Shift] + [Alt].
  const ui::Accelerator accelerator8{
      ui::VKEY_LEFT, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN};
  const ui::Accelerator expected_accelerator_alias8{
      ui::VKEY_HOME, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN};
  std::vector<ui::Accelerator> accelerator_alias8 =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator8);

  EXPECT_EQ(2u, accelerator_alias8.size());
  EXPECT_EQ(expected_accelerator_alias8, accelerator_alias8[0]);
  EXPECT_EQ(accelerator8, accelerator_alias8[1]);
}

}  // namespace ash
