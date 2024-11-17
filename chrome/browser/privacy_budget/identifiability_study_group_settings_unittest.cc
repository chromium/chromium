// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/identifiability_study_group_settings.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(IdentifiabilityStudyGroupSettingsTest, Disabled) {
  auto settings =
      IdentifiabilityStudyGroupSettings::InitFrom(false, 0, 0, "", "", "");
  EXPECT_FALSE(settings.enabled());
}

TEST(IdentifiabilityStudyGroupSettingsTest, DisabledWithParams) {
  auto settings =
      IdentifiabilityStudyGroupSettings::InitFrom(false, 10, 40, "", "", "");
  EXPECT_FALSE(settings.enabled());
}

TEST(IdentifiabilityStudyGroupSettingsTest, DisabledBySurfaceCountZero) {
  auto settings =
      IdentifiabilityStudyGroupSettings::InitFrom(true, 0, 40, "", "", "");
  EXPECT_FALSE(settings.enabled());
}

TEST(IdentifiabilityStudyGroupSettingsTest, ValidRandomSurfaceSampling) {
  auto settings =
      IdentifiabilityStudyGroupSettings::InitFrom(true, 10, 40, "", "", "1,4");
  EXPECT_TRUE(settings.enabled());
  EXPECT_FALSE(settings.IsUsingAssignedBlockSampling());
  EXPECT_TRUE(settings.IsUsingRandomSampling());
  EXPECT_TRUE(settings.IsUsingSamplingOfSurfaces());
  EXPECT_EQ(10, settings.expected_surface_count());
  EXPECT_EQ(40, settings.surface_budget());
  const std::vector<blink::IdentifiableSurface::Type> expected_allowed_types{
      blink::IdentifiableSurface::Type::kWebFeature,
      blink::IdentifiableSurface::Type::kGenericFontLookup};
  EXPECT_EQ(expected_allowed_types, settings.allowed_random_types());
}

TEST(IdentifiabilityStudyGroupSettingsTest, ValidAssignedBlockSampling) {
  auto settings = IdentifiabilityStudyGroupSettings::InitFrom(
      true, 0, 0, "1;2,3;4,5;6", "1,1,1", "");
  EXPECT_TRUE(settings.enabled());
  EXPECT_TRUE(settings.IsUsingAssignedBlockSampling());
  EXPECT_FALSE(settings.IsUsingRandomSampling());
  EXPECT_TRUE(settings.IsUsingSamplingOfSurfaces());
}

TEST(IdentifiabilityStudyGroupSettingsTest, InvalidNegativeWeight) {
  auto settings = IdentifiabilityStudyGroupSettings::InitFrom(
      true, 0, 0, "1;2,3;4,5;6", "-1,1,1", "");
  EXPECT_FALSE(settings.enabled());
}

TEST(IdentifiabilityStudyGroupSettingsTest, InvalidSurfaceTooLikely) {
  auto settings = IdentifiabilityStudyGroupSettings::InitFrom(
      true, 0, 0, "1;2,1;4,5;6", "1,1,1", "");
  EXPECT_FALSE(settings.enabled());
}
