// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_alias_converter.h"

#include <vector>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/chromeos/events/keyboard_capability.h"

namespace ash {

namespace {

struct AcceleratorAliasConverterTestData {
  ui::Accelerator accelerator_;
  absl::optional<ui::Accelerator> expected_accelerator_;
};

}  // namespace

using AcceleratorAliasConverterTest = AshTestBase;

TEST_F(AcceleratorAliasConverterTest, CheckTopRowAlias) {
  AcceleratorAliasConverter accelerator_alias_converter_;

  // Top row keys not fKeys prevents remapping.
  Shell::Get()->keyboard_capability()->SetTopRowKeysAsFKeysEnabledForTesting(
      false);
  EXPECT_FALSE(Shell::Get()->keyboard_capability()->TopRowKeysAreFKeys());
  const ui::Accelerator accelerator{ui::VKEY_ZOOM, ui::EF_ALT_DOWN};
  std::vector<ui::Accelerator> accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);

  EXPECT_EQ(1u, accelerator_aliases.size());
  EXPECT_EQ(accelerator, accelerator_aliases[0]);
}

class TopRowAliasTest
    : public AcceleratorAliasConverterTest,
      public testing::WithParamInterface<AcceleratorAliasConverterTestData> {
  void SetUp() override {
    AcceleratorAliasConverterTest::SetUp();
    // Enable top row keys as fKeys.
    Shell::Get()->keyboard_capability()->SetTopRowKeysAsFKeysEnabledForTesting(
        true);
    AcceleratorAliasConverterTestData test_data = GetParam();
    accelerator_ = test_data.accelerator_;
    expected_accelerator_ = test_data.expected_accelerator_;
  }

 protected:
  ui::Accelerator accelerator_;
  absl::optional<ui::Accelerator> expected_accelerator_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    TopRowAliasTest,
    testing::ValuesIn(std::vector<AcceleratorAliasConverterTestData>{
        // [Search] as original modifier prevents remapping.
        {ui::Accelerator{ui::VKEY_ZOOM, ui::EF_COMMAND_DOWN}, absl::nullopt},
        // key_code not as a top row key prevents remapping.
        {ui::Accelerator{ui::VKEY_TAB, ui::EF_ALT_DOWN}, absl::nullopt},
        // [Alt] + [Zoom] -> [Alt] + [Search] + [F3].
        // TopRowKeysAreFKeys() remains true.
        {ui::Accelerator{ui::VKEY_ZOOM, ui::EF_ALT_DOWN},
         ui::Accelerator{ui::VKEY_F3, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}},
        // [Alt] + [Shift] + [Back] -> [Alt] + [Shift] + [Search] + [F1].
        // TopRowKeysAreFKeys() remains true.
        {ui::Accelerator{ui::VKEY_BROWSER_BACK,
                         ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN},
         ui::Accelerator{ui::VKEY_F1, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN |
                                          ui::EF_SHIFT_DOWN}}}));

TEST_P(TopRowAliasTest, CheckTopRowAlias) {
  AcceleratorAliasConverter accelerator_alias_converter_;

  std::vector<ui::Accelerator> accelerator_alias =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator_);

  EXPECT_TRUE(Shell::Get()->keyboard_capability()->TopRowKeysAreFKeys());
  EXPECT_EQ(1u, accelerator_alias.size());
  if (expected_accelerator_.has_value()) {
    EXPECT_EQ(expected_accelerator_, accelerator_alias[0]);
  } else {
    EXPECT_EQ(accelerator_, accelerator_alias[0]);
  }
}

class SixPackAliasTest
    : public AcceleratorAliasConverterTest,
      public testing::WithParamInterface<AcceleratorAliasConverterTestData> {
  void SetUp() override {
    AcceleratorAliasConverterTest::SetUp();
    AcceleratorAliasConverterTestData test_data = GetParam();
    accelerator_ = test_data.accelerator_;
    expected_accelerator_ = test_data.expected_accelerator_;
  }

 protected:
  ui::Accelerator accelerator_;
  absl::optional<ui::Accelerator> expected_accelerator_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    SixPackAliasTest,
    testing::ValuesIn(std::vector<AcceleratorAliasConverterTestData>{
        // [Search] as original modifier prevents remapping.
        {ui::Accelerator{ui::VKEY_ZOOM, ui::EF_COMMAND_DOWN}, absl::nullopt},
        // key_code not as six pack key prevents remapping.
        {ui::Accelerator{ui::VKEY_TAB, ui::EF_ALT_DOWN}, absl::nullopt},
        // [Shift] + [Delete] should not be remapped.
        {ui::Accelerator{ui::VKEY_DELETE, ui::EF_SHIFT_DOWN}, absl::nullopt},
        // [Shift] + [Insert] should not be remapped.
        {ui::Accelerator{ui::VKEY_INSERT, ui::EF_SHIFT_DOWN}, absl::nullopt},
        // For Insert: [modifiers] -> [Search] + [Shift] + [original_modifiers].
        {ui::Accelerator{ui::VKEY_INSERT, ui::EF_ALT_DOWN},
         ui::Accelerator{ui::VKEY_BACK, ui::EF_COMMAND_DOWN |
                                            ui::EF_SHIFT_DOWN |
                                            ui::EF_ALT_DOWN}},
        // For other six-pack-keys: [modifiers] -> [Search] +
        // [original_modifiers].
        {ui::Accelerator{ui::VKEY_DELETE, ui::EF_ALT_DOWN},
         ui::Accelerator{ui::VKEY_BACK, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}},

        // Below are tests for reversed six pack alias.
        // [Search] not in modifiers prevents remapping.
        {ui::Accelerator{ui::VKEY_LEFT, ui::EF_ALT_DOWN}, absl::nullopt},
        // key_code not as reversed six pack key prevent remapping.
        {ui::Accelerator{ui::VKEY_ZOOM, ui::EF_COMMAND_DOWN}, absl::nullopt},
        // [Search] as the only modifier prevents remapping.
        {ui::Accelerator{ui::VKEY_BACK, ui::EF_COMMAND_DOWN}, absl::nullopt},
        // [Back] + [Shift] + [Search] only prevents remapping, which is just
        // the reverse of [Insert].
        {ui::Accelerator{ui::VKEY_BACK,
                         ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN},
         absl::nullopt},
        // [Back] + [Shift] + [Search] + [Alt] -> [Insert] + [Alt].
        {ui::Accelerator{ui::VKEY_BACK, ui::EF_COMMAND_DOWN |
                                            ui::EF_SHIFT_DOWN |
                                            ui::EF_ALT_DOWN},
         ui::Accelerator{ui::VKEY_INSERT, ui::EF_ALT_DOWN}},
        // [Back] + [Search] + [Alt] -> [Delete] + [Alt].
        {ui::Accelerator{ui::VKEY_BACK, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN},
         ui::Accelerator{ui::VKEY_DELETE, ui::EF_ALT_DOWN}},
        // [Left] + [Search] + [Alt] -> [Home] + [Alt].
        {ui::Accelerator{ui::VKEY_LEFT, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN},
         ui::Accelerator{ui::VKEY_HOME, ui::EF_ALT_DOWN}},
        // [Left] + [Search] + [Shift] + [Alt] -> [Home] + [Shift] + [Alt].
        {ui::Accelerator{ui::VKEY_LEFT, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN |
                                            ui::EF_SHIFT_DOWN},
         ui::Accelerator{ui::VKEY_HOME,
                         ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN}}}));

TEST_P(SixPackAliasTest, CheckSixPackAlias) {
  AcceleratorAliasConverter accelerator_alias_converter_;

  std::vector<ui::Accelerator> accelerator_alias =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator_);

  if (expected_accelerator_.has_value()) {
    // Accelerator has valid a remapping.
    EXPECT_EQ(2u, accelerator_alias.size());
    EXPECT_EQ(expected_accelerator_, accelerator_alias[0]);
    EXPECT_EQ(accelerator_, accelerator_alias[1]);
  } else {
    // Accelerator doesn't have a valid remapping.
    EXPECT_EQ(1u, accelerator_alias.size());
    EXPECT_EQ(accelerator_, accelerator_alias[0]);
  }
}

}  // namespace ash
