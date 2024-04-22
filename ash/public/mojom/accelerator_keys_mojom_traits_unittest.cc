// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/mojom/accelerator_keys_mojom_traits.h"
#include "ash/public/mojom/accelerator_keys.mojom.h"

#include <algorithm>

#include "base/containers/fixed_flat_map.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

using mojom_vkey = ash::mojom::VKey;

namespace {

template <typename MojoEnum, typename KeyboardCodeEnum, size_t N>
void TestKeyboardCodeToMojo(
    const base::fixed_flat_map<MojoEnum, KeyboardCodeEnum, N>& enums) {
  for (auto enum_pair : enums) {
    EXPECT_EQ(enum_pair.first,
              (mojo::EnumTraits<MojoEnum, KeyboardCodeEnum>::ToMojom(
                  enum_pair.second)))
        << "enum " << enum_pair.first << " != " << enum_pair.second;
  }
}

template <typename MojoEnum, typename KeyboardCodeEnum, size_t N>
void TestMojoToKeyboardCode(
    const base::fixed_flat_map<MojoEnum, KeyboardCodeEnum, N>& enums) {
  for (auto enum_pair : enums) {
    KeyboardCodeEnum mojo_to_code;
    EXPECT_TRUE((mojo::EnumTraits<MojoEnum, KeyboardCodeEnum>::FromMojom(
        enum_pair.first, &mojo_to_code)));
    EXPECT_EQ(mojo_to_code, enum_pair.second)
        << "enum " << enum_pair.first << " != " << enum_pair.second;
  }
}

}  // namespace

TEST(AcceleratorKeysTraitsTest, SerializeAndDeserialize) {
  // Spot check random VKey's and confirm that they were able to serialize to
  // mojom and then deserialize back to VKey.
  const auto enums = base::MakeFixedFlatMap<mojom_vkey, ui::KeyboardCode>(
      {{mojom_vkey::kCancel, ui::KeyboardCode::VKEY_CANCEL},
       {mojom_vkey::kControl, ui::KeyboardCode::VKEY_CONTROL},
       {mojom_vkey::kShift, ui::KeyboardCode::VKEY_SHIFT},
       {mojom_vkey::kModeChange, ui::KeyboardCode::VKEY_MODECHANGE},
       {mojom_vkey::kHelp, ui::KeyboardCode::VKEY_HELP},
       {mojom_vkey::kKana, ui::KeyboardCode::VKEY_KANA},
       {mojom_vkey::kNum4, ui::KeyboardCode::VKEY_4},
       {mojom_vkey::kWlan, ui::KeyboardCode::VKEY_WLAN},
       {mojom_vkey::kF14, ui::KeyboardCode::VKEY_F14},
       {mojom_vkey::kPrivacyScreenToggle,
        ui::KeyboardCode::VKEY_PRIVACY_SCREEN_TOGGLE},
       {mojom_vkey::kAccessibility, ui::KeyboardCode::VKEY_ACCESSIBILITY}});

  TestKeyboardCodeToMojo(enums);
  TestMojoToKeyboardCode(enums);
}

}  // namespace ash
