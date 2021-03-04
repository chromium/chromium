// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/oobe_dialog_size_utils.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chromeos {

namespace {

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

    EXPECT_LE(dialog.width(), kMaxDialogSize.width());
    EXPECT_LE(dialog.height(), kMaxDialogSize.height());
    // If there is at least some space, we should have margins.
    if (dialog.width() > kMinDialogSize.width()) {
      EXPECT_GE(dialog.x(), kMinMargins.left());
      EXPECT_GE(area.right() - dialog.right(), kMinMargins.right());
    }
    if (dialog.height() > kMinDialogSize.height()) {
      EXPECT_TRUE(dialog.y() >= kMinMargins.top());
      EXPECT_TRUE(area.bottom() - dialog.bottom() >= kMinMargins.bottom());
    }
    // If dialog size is lesser than minimum size, there should be no margins
    if (dialog.width() < kMinDialogSize.width()) {
      EXPECT_EQ(dialog.x(), area.x());
      EXPECT_EQ(dialog.right(), area.right());
    }
    if (dialog.height() < kMinDialogSize.height()) {
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

  CalculateOobeDialogBounds(
      usual_device, kShelfHeight, /* is_horizontal = */ false,
      /* is_new_oobe_layout_enabled = */ false, &dialog, &padding);
  ValidateDialog(SizeWithoutShelf(usual_device), dialog);
  EXPECT_EQ(padding, OobeDialogPaddingMode::PADDING_WIDE);
}

// We have plenty of space on the screen, but virtual keyboard takes some space.
TEST_F(OobeDialogSizeUtilsTest, ChromebookVirtualKeyboard) {
  gfx::Rect usual_device_with_keyboard(1200, 800 - kVirtualKeyboardHeight);
  gfx::Rect dialog;
  OobeDialogPaddingMode padding;

  CalculateOobeDialogBounds(
      usual_device_with_keyboard, 0, /* is_horizontal = */ false,
      /* is_new_oobe_layout_enabled = */ false, &dialog, &padding);
  ValidateDialog(usual_device_with_keyboard, dialog);
  EXPECT_EQ(padding, OobeDialogPaddingMode::PADDING_NARROW);
}

// Tablet device can have smaller screen size.
TEST_F(OobeDialogSizeUtilsTest, TabletHorizontal) {
  gfx::Rect tablet_device(1080, 675);
  gfx::Rect dialog;
  OobeDialogPaddingMode padding;

  CalculateOobeDialogBounds(
      tablet_device, kShelfHeight, /* is_horizontal = */ false,
      /* is_new_oobe_layout_enabled = */ false, &dialog, &padding);
  ValidateDialog(SizeWithoutShelf(tablet_device), dialog);
  EXPECT_EQ(padding, OobeDialogPaddingMode::PADDING_NARROW);
}

// Tablet device in horizontal mode with virtual keyboard have restricted
// vertical space.
TEST_F(OobeDialogSizeUtilsTest, TabletHorizontalVirtualKeyboard) {
  gfx::Rect tablet_device(1080, 675 - kVirtualKeyboardHeight);
  gfx::Rect dialog;
  OobeDialogPaddingMode padding;

  CalculateOobeDialogBounds(tablet_device, 0, /* is_horizontal = */ false,
                            /* is_new_oobe_layout_enabled = */ false, &dialog,
                            &padding);
  ValidateDialog(tablet_device, dialog);
  EXPECT_EQ(padding, OobeDialogPaddingMode::PADDING_NARROW);
}

// Tablet device in horizontal mode with docked magnifier have restricted
// vertical space.
TEST_F(OobeDialogSizeUtilsTest, TabletHorizontalDockedMagnifier) {
  gfx::Rect tablet_device(0, 0, 1080, 675 - kDockedMagnifierHeight);
  gfx::Rect dialog;
  OobeDialogPaddingMode padding;

  CalculateOobeDialogBounds(tablet_device, 0, /* is_horizontal = */ false,
                            /* is_new_oobe_layout_enabled = */ false, &dialog,
                            &padding);
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

  CalculateOobeDialogBounds(tablet_device, 0, /* is_horizontal = */ false,
                            /* is_new_oobe_layout_enabled = */ false, &dialog,
                            &padding);
  ValidateDialog(tablet_device, dialog);
  EXPECT_EQ(padding, OobeDialogPaddingMode::PADDING_NARROW);
}

// Tablet in vertical mode puts some strain on dialog width.
TEST_F(OobeDialogSizeUtilsTest, TabletVertical) {
  gfx::Rect tablet_device(675, 1080);
  gfx::Rect dialog;
  OobeDialogPaddingMode padding;

  CalculateOobeDialogBounds(
      tablet_device, kShelfHeight, /* is_horizontal = */ false,
      /* is_new_oobe_layout_enabled = */ false, &dialog, &padding);
  ValidateDialog(SizeWithoutShelf(tablet_device), dialog);
  EXPECT_EQ(padding, OobeDialogPaddingMode::PADDING_NARROW);
}

class OobeDialogSizeUtilsTestNewLayout : public testing::Test {
 public:
  OobeDialogSizeUtilsTestNewLayout() = default;

  ~OobeDialogSizeUtilsTestNewLayout() override = default;

  void ValidateDialog(const gfx::Rect& display,
                      const gfx::Rect& area,
                      const gfx::Rect& dialog) {
    // Dialog should fully fit into the area.
    EXPECT_TRUE(area.Contains(dialog));

    // Dialog is centered in area.
    EXPECT_EQ(area.CenterPoint(), dialog.CenterPoint());

    const gfx::Size min_dialog_size = GetMinDialogSize(display);
    const gfx::Size max_dialog_size = GetMaxDialogSize(display);
    EXPECT_LE(dialog.width(), max_dialog_size.width());
    EXPECT_LE(dialog.height(), max_dialog_size.height());

    // If there is at least some space, we should have margins.
    const gfx::Size margins = ExpectedMargins(display.size());
    if (dialog.width() > min_dialog_size.width()) {
      EXPECT_EQ(dialog.x() + (area.right() - dialog.right()), margins.width());
    }
    if (dialog.height() > min_dialog_size.height()) {
      EXPECT_EQ(dialog.y() + (area.bottom() - dialog.bottom()),
                margins.height());
    }
    // If dialog size is lesser than minimum size, there should be no margins
    if (dialog.width() < min_dialog_size.width()) {
      EXPECT_EQ(dialog.x(), area.x());
      EXPECT_EQ(dialog.right(), area.right());
    }
    if (dialog.height() < min_dialog_size.height()) {
      EXPECT_EQ(dialog.y(), area.y());
      EXPECT_EQ(dialog.bottom(), area.bottom());
    }
  }

  gfx::Size GetMinDialogSize(const gfx::Rect& display) {
    if (IsHorizontal(display)) {
      return kMinLandscapeDialogSize;
    }
    return kMinPortraitDialogSize;
  }

  gfx::Size GetMaxDialogSize(const gfx::Rect& display) {
    if (IsHorizontal(display)) {
      return kMaxLandscapeDialogSize;
    }
    return kMaxPortraitDialogSize;
  }

  gfx::Size ExpectedMargins(const gfx::Size& display_size) {
    gfx::Size margin = ScaleToCeiledSize(display_size, 0.08);
    gfx::Size margins = margin + margin;
    margins.SetToMax(kMinMargins.size());
    return margins;
  }

  gfx::Rect SizeWithoutShelf(const gfx::Rect& area) const {
    return gfx::Rect(area.width(), area.height() - kShelfHeight);
  }

  gfx::Rect SizeWithoutKeyboard(const gfx::Rect& area) const {
    return gfx::Rect(area.width(), area.height() - kVirtualKeyboardHeight);
  }

  gfx::Rect SizeWithoutDockedMagnifier(const gfx::Rect& area) const {
    return gfx::Rect(area.width(), area.height() - kDockedMagnifierHeight);
  }

  bool IsHorizontal(const gfx::Rect& area) const {
    return area.width() > area.height();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  DISALLOW_COPY_AND_ASSIGN(OobeDialogSizeUtilsTestNewLayout);
};

// We have plenty of space on the screen.
TEST_F(OobeDialogSizeUtilsTestNewLayout, Chromebook) {
  gfx::Rect usual_device(1200, 800);
  gfx::Rect dialog;
  OobeDialogPaddingMode padding;

  CalculateOobeDialogBounds(
      usual_device, kShelfHeight, IsHorizontal(usual_device),
      /* is_new_oobe_layout_enabled = */ true, &dialog, &padding);
  ValidateDialog(usual_device, SizeWithoutShelf(usual_device), dialog);
}

// We have plenty of space on the screen, but virtual keyboard takes some space.
TEST_F(OobeDialogSizeUtilsTestNewLayout, ChromebookVirtualKeyboard) {
  gfx::Rect usual_device(1200, 800);
  gfx::Rect dialog;
  OobeDialogPaddingMode padding;

  CalculateOobeDialogBounds(
      SizeWithoutKeyboard(usual_device), 0, IsHorizontal(usual_device),
      /* is_new_oobe_layout_enabled = */ true, &dialog, &padding);
  ValidateDialog(usual_device, SizeWithoutKeyboard(usual_device), dialog);
}

// Tablet device can have smaller screen size.
TEST_F(OobeDialogSizeUtilsTestNewLayout, TabletHorizontal) {
  gfx::Rect tablet_device(1080, 675);
  gfx::Rect dialog;
  OobeDialogPaddingMode padding;

  CalculateOobeDialogBounds(
      tablet_device, kShelfHeight, IsHorizontal(tablet_device),
      /* is_new_oobe_layout_enabled = */ true, &dialog, &padding);
  ValidateDialog(tablet_device, SizeWithoutShelf(tablet_device), dialog);
}

// Tablet device in horizontal mode with virtual keyboard have restricted
// vertical space.
TEST_F(OobeDialogSizeUtilsTestNewLayout, TabletHorizontalVirtualKeyboard) {
  gfx::Rect tablet_device(1080, 675);
  gfx::Rect dialog;
  OobeDialogPaddingMode padding;

  CalculateOobeDialogBounds(
      SizeWithoutKeyboard(tablet_device), 0, IsHorizontal(tablet_device),
      /* is_new_oobe_layout_enabled = */ true, &dialog, &padding);
  ValidateDialog(tablet_device, SizeWithoutKeyboard(tablet_device), dialog);
}

// Tablet device in horizontal mode with docked magnifier have restricted
// vertical space.
TEST_F(OobeDialogSizeUtilsTestNewLayout, TabletHorizontalDockedMagnifier) {
  gfx::Rect tablet_device(1080, 675);
  gfx::Rect dialog;
  OobeDialogPaddingMode padding;

  CalculateOobeDialogBounds(
      SizeWithoutDockedMagnifier(tablet_device), 0, IsHorizontal(tablet_device),
      /* is_new_oobe_layout_enabled = */ true, &dialog, &padding);
  ValidateDialog(tablet_device, SizeWithoutDockedMagnifier(tablet_device),
                 dialog);
}

// Tablet device in horizontal mode with virtual keyboard and docked
// magnifier results in very few vertical space.
TEST_F(OobeDialogSizeUtilsTestNewLayout,
       TabletHorizontalVirtualKeyboardMagnifier) {
  gfx::Rect tablet_device(1080, 675);

  gfx::Rect dialog;
  OobeDialogPaddingMode padding;

  CalculateOobeDialogBounds(
      SizeWithoutDockedMagnifier(SizeWithoutKeyboard(tablet_device)), 0,
      IsHorizontal(tablet_device), /* is_new_oobe_layout_enabled = */ true,
      &dialog, &padding);
  ValidateDialog(tablet_device,
                 SizeWithoutDockedMagnifier(SizeWithoutKeyboard(tablet_device)),
                 dialog);
}

// Tablet in vertical mode puts some strain on dialog width.
TEST_F(OobeDialogSizeUtilsTestNewLayout, ChromeTabVertical) {
  gfx::Rect tablet_device(461, 738);
  gfx::Rect dialog;
  OobeDialogPaddingMode padding;

  CalculateOobeDialogBounds(
      tablet_device, kShelfHeight, IsHorizontal(tablet_device),
      /* is_new_oobe_layout_enabled = */ true, &dialog, &padding);
  ValidateDialog(tablet_device, SizeWithoutShelf(tablet_device), dialog);
}

// Tablet in horziontal mode puts some strain on dialog width.
TEST_F(OobeDialogSizeUtilsTestNewLayout, ChromeTabHorizontal) {
  gfx::Rect tablet_device(738, 461);
  gfx::Rect dialog;
  OobeDialogPaddingMode padding;

  CalculateOobeDialogBounds(
      tablet_device, kShelfHeight, IsHorizontal(tablet_device),
      /* is_new_oobe_layout_enabled = */ true, &dialog, &padding);
  ValidateDialog(tablet_device, SizeWithoutShelf(tablet_device), dialog);
}

}  // namespace chromeos
