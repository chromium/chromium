// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_properties.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef testing::Test ThemePropertiesTest;

TEST_F(ThemePropertiesTest, AlignmentConversion) {
  // Verify that we get out what we put in.
  std::string top_left = "left top";
  int alignment = ThemeProperties::StringToAlignment(top_left);
  EXPECT_EQ(ThemeProperties::ALIGN_TOP | ThemeProperties::ALIGN_LEFT,
            alignment);
  EXPECT_EQ(top_left, ThemeProperties::AlignmentToString(alignment));

  // We get back a normalized version of what we put in.
  alignment = ThemeProperties::StringToAlignment("top");
  EXPECT_EQ(ThemeProperties::ALIGN_TOP, alignment);
  EXPECT_EQ("center top", ThemeProperties::AlignmentToString(alignment));

  alignment = ThemeProperties::StringToAlignment("left");
  EXPECT_EQ(ThemeProperties::ALIGN_LEFT, alignment);
  EXPECT_EQ("left center", ThemeProperties::AlignmentToString(alignment));

  alignment = ThemeProperties::StringToAlignment("right");
  EXPECT_EQ(ThemeProperties::ALIGN_RIGHT, alignment);
  EXPECT_EQ("right center", ThemeProperties::AlignmentToString(alignment));

  alignment = ThemeProperties::StringToAlignment("righttopbottom");
  EXPECT_EQ(ThemeProperties::ALIGN_CENTER, alignment);
  EXPECT_EQ("center center", ThemeProperties::AlignmentToString(alignment));
}

TEST_F(ThemePropertiesTest, AlignmentConversionInput) {
  // Verify that we output in an expected format.
  int alignment = ThemeProperties::StringToAlignment("bottom right");
  EXPECT_EQ("right bottom", ThemeProperties::AlignmentToString(alignment));

  // Verify that bad strings don't cause explosions.
  alignment = ThemeProperties::StringToAlignment("new zealand");
  EXPECT_EQ("center center", ThemeProperties::AlignmentToString(alignment));

  // Verify that bad strings don't cause explosions.
  alignment = ThemeProperties::StringToAlignment("new zealand top");
  EXPECT_EQ("center top", ThemeProperties::AlignmentToString(alignment));

  // Verify that bad strings don't cause explosions.
  alignment = ThemeProperties::StringToAlignment("new zealandtop");
  EXPECT_EQ("center center", ThemeProperties::AlignmentToString(alignment));
}

}  // namespace
