// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"

#include "base/json/json_reader.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_code.h"

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

TEST(InputElementTest, TestInputElementOverlap) {
  auto move_a = InputElement::CreateActionMoveKeyElement(
      {ui::DomCode::US_A, ui::DomCode::NONE, ui::DomCode::NONE,
       ui::DomCode::NONE});
  auto move_b = InputElement::CreateActionMoveKeyElement(
      {ui::DomCode::US_B, ui::DomCode::NONE, ui::DomCode::NONE,
       ui::DomCode::NONE});
  auto move_c = InputElement::CreateActionMoveKeyElement(
      {ui::DomCode::US_A, ui::DomCode::NONE, ui::DomCode::NONE,
       ui::DomCode::NONE});
  auto tap_a = InputElement::CreateActionTapKeyElement(ui::DomCode::US_A);
  auto tap_b = InputElement::CreateActionTapKeyElement(ui::DomCode::NONE);
  EXPECT_FALSE(move_a->IsOverlapped(*move_b));
  EXPECT_TRUE(move_a->IsOverlapped(*move_c));
  EXPECT_TRUE(move_a->IsOverlapped(*tap_a));
  EXPECT_TRUE(move_c->IsOverlapped(*tap_a));
  EXPECT_FALSE(move_b->IsOverlapped(*move_c));

  EXPECT_FALSE(tap_b->IsOverlapped(*move_a));
  EXPECT_FALSE(tap_b->IsOverlapped(*move_b));
  EXPECT_FALSE(tap_b->IsOverlapped(*move_c));
  EXPECT_FALSE(tap_b->IsOverlapped(*tap_a));
}

TEST(InputElementTest, TestInputElementUnbound) {
  auto input_element = std::make_unique<InputElement>();
  EXPECT_TRUE(input_element->IsUnbound());

  auto move_a = InputElement::CreateActionMoveKeyElement(
      {ui::DomCode::US_A, ui::DomCode::NONE, ui::DomCode::NONE,
       ui::DomCode::NONE});
  EXPECT_FALSE(move_a->IsUnbound());

  auto move_b = InputElement::CreateActionMoveKeyElement(
      {ui::DomCode::NONE, ui::DomCode::NONE, ui::DomCode::NONE,
       ui::DomCode::NONE});
  EXPECT_TRUE(move_b->IsUnbound());

  auto tap_a = InputElement::CreateActionTapKeyElement(ui::DomCode::US_A);
  EXPECT_FALSE(tap_a->IsUnbound());

  auto tap_b = InputElement::CreateActionTapKeyElement(ui::DomCode::NONE);
  EXPECT_TRUE(tap_b->IsUnbound());
}
}  // namespace arc::input_overlay
