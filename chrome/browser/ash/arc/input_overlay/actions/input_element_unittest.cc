// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"

#include "base/json/json_reader.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc::input_overlay {

class InputElementTest : public testing::Test {
 protected:
  InputElementTest() = default;
};

TEST(InputElementTest, TestInputElementEquality) {
  auto tap_a1 = InputElement::CreateActionTapKeyElement(ui::DomCode::US_A);
  auto tap_a2 = InputElement::CreateActionTapKeyElement(ui::DomCode::US_A);
  EXPECT_TRUE(*tap_a1 == *tap_a2);
  auto tap_b = InputElement::CreateActionTapKeyElement(ui::DomCode::US_B);
  EXPECT_FALSE(*tap_a1 == *tap_b);

  auto tap_primary_click1 =
      InputElement::CreateActionTapMouseElement(kPrimaryClick);
  auto tap_primary_click2 =
      InputElement::CreateActionTapMouseElement(kPrimaryClick);
  EXPECT_TRUE(*tap_primary_click1 == *tap_primary_click2);
  auto tap_secondary_click =
      InputElement::CreateActionTapMouseElement(kSecondaryClick);
  EXPECT_FALSE(*tap_primary_click1 == *tap_secondary_click);
}

}  // namespace arc::input_overlay
