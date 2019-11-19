// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/oobe_dialog_size_utils.h"

#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chromeos {

namespace {

constexpr int kGaiaDialogMaxSize = 768;
constexpr int kGaiaDialogMinMargin = 48;
constexpr int kGaiaDialogMinSize = 464;

constexpr int kShelfHeight = 56;
constexpr int kVirtualKeyboardHeight = 280;
constexpr int kDockedMagnifierHeight = 235;

}  // namespace

class OobeDialogSizeUtilsTest : public testing::Test {
 public:
  OobeDialogSizeUtilsTest() = default;

  ~OobeDialogSizeUtilsTest() override = default;

  void ValidateDialog(const gfx::Rect& area, const gfx::Rect& dialog) {
    // Dialog should fully fit into the area.
    EXPECT_TRUE(area.Contains(dialog));

    EXPECT_GE(dialog.x(), area.x());
    EXPECT_LE(dialog.right(), area.right());
    EXPECT_GE(dialog.y(), area.y());
    EXPECT_LE(dialog.bottom(), area.bottom());

    // Dialog is centered in area.
    EXPECT_EQ(area.CenterPoint(), dialog.CenterPoint());

    EXPECT_LE(dialog.width(), kGaiaDialogMaxSize);
    EXPECT_LE(dialog.height(), kGaiaDialogMaxSize);
    // If there is at least some space, we should have margins.
    if (dialog.width() > kGaiaDialogMinSize) {
      EXPECT_GE(dialog.x(), kGaiaDialogMinMargin);
      EXPECT_GE(area.right() - dialog.right(), kGaiaDialogMinMargin);
    }
    if (dialog.height() > kGaiaDialogMinSize) {
      EXPECT_TRUE(dialog.y() >= kGaiaDialogMinMargin);
      EXPECT_TRUE(area.bottom() - dialog.bottom() >= kGaiaDialogMinMargin);
    }
    // If dialog size is lesser than minimum size, there should be no margins
    if (dialog.width() < kGaiaDialogMinSize) {
      EXPECT_EQ(dialog.x(), area.x());
      EXPECT_EQ(dialog.right(), area.right());
    }
    if (dialog.height() < kGaiaDialogMinSize) {
      EXPECT_EQ(dialog.y(), area.y());
      EXPECT_EQ(dialog.bottom(), area.bottom());
    }
  }

  gfx::Rect SizeWithoutShelf(const gfx::Rect& area) const {
    return gfx::Rect(area.width(), area.height() - kShelfHeight);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OobeDialogSizeUtilsTest);
};

// We have plenty of space on the screen.
TEST_F(OobeDialogSizeUtilsTest, Chromebook) {
  gfx::Rect usual_device(1200, 800);
  gfx::Rect dialog;
  OobeDialogPaddingMode padding;

  CalculateOobeDialogBounds(usual_device, kShelfHeight, &dialog, &padding);
  ValidateDialog(SizeWithoutShelf(usual_device), dialog);
  EXPECT_EQ(padding, OobeDialogPaddingMode::PADDING_WIDE);
}

// We have plenty of space on the screen, but virtual keyboard takes some space.
TEST_F(OobeDialogSizeUtilsTest, ChromebookVirtualKeyboard) {
  gfx::Rect usual_device_with_keyboard(1200, 800 - kVirtualKeyboardHeight);
  gfx::Rect dialog;
  OobeDialogPaddingMode padding;

  CalculateOobeDialogBounds(usual_device_with_keyboard, 0, &dialog, &padding);
  ValidateDialog(usual_device_with_keyboard, dialog);
  EXPECT_EQ(padding, OobeDialogPaddingMode::PADDING_NARROW);
}

// Tablet device can have smaller screen size.
TEST_F(OobeDialogSizeUtilsTest, TabletHorizontal) {
  gfx::Rect tablet_device(1080, 675);
  gfx::Rect dialog;
  OobeDialogPaddingMode padding;

  CalculateOobeDialogBounds(tablet_device, kShelfHeight, &dialog, &padding);
  ValidateDialog(SizeWithoutShelf(tablet_device), dialog);
  EXPECT_EQ(padding, OobeDialogPaddingMode::PADDING_NARROW);
}

// Tablet device in horizontal mode with virtual keyboard have restricted
// vertical space.
TEST_F(OobeDialogSizeUtilsTest, TabletHorizontalVirtualKeyboard) {
  gfx::Rect tablet_device(1080, 675 - kVirtualKeyboardHeight);
  gfx::Rect dialog;
  OobeDialogPaddingMode padding;

  CalculateOobeDialogBounds(tablet_device, 0, &dialog, &padding);
  ValidateDialog(tablet_device, dialog);
  EXPECT_EQ(padding, OobeDialogPaddingMode::PADDING_NARROW);
}

// Tablet device in horizontal mode with docked magnifier have restricted
// vertical space.
TEST_F(OobeDialogSizeUtilsTest, TabletHorizontalDockedMagnifier) {
  gfx::Rect tablet_device(0, 0, 1080, 675 - kDockedMagnifierHeight);
  gfx::Rect dialog;
  OobeDialogPaddingMode padding;

  CalculateOobeDialogBounds(tablet_device, 0, &dialog, &padding);
  ValidateDialog(tablet_device, dialog);
  EXPECT_EQ(padding, OobeDialogPaddingMode::PADDING_NARROW);
}

// Tablet device in horizontal mode with virtual keyboard and docked
// magnifier results in very few vertical space.
TEST_F(OobeDialogSizeUtilsTest, TabletHorizontalVirtualKeyboardMagnifier) {
  gfx::Rect tablet_device(
      0, 0, 1080, 675 - kVirtualKeyboardHeight - kDockedMagnifierHeight);

  gfx::Rect dialog;
  OobeDialogPaddingMode padding;

  CalculateOobeDialogBounds(tablet_device, 0, &dialog, &padding);
  ValidateDialog(tablet_device, dialog);
  EXPECT_EQ(padding, OobeDialogPaddingMode::PADDING_NARROW);
}

// Tablet in vertical mode puts some strain on dialog width.
TEST_F(OobeDialogSizeUtilsTest, TabletVertical) {
  gfx::Rect tablet_device(675, 1080);
  gfx::Rect dialog;
  OobeDialogPaddingMode padding;

  CalculateOobeDialogBounds(tablet_device, kShelfHeight, &dialog, &padding);
  ValidateDialog(SizeWithoutShelf(tablet_device), dialog);
  EXPECT_EQ(padding, OobeDialogPaddingMode::PADDING_NARROW);
}

}  // namespace chromeos
