// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/sea_pen_utils.h"

#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_utils/sea_pen_metadata_utils.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/gfx/geometry/size.h"

namespace wallpaper_handlers {
namespace {

using SeaPenUtilsTest = ash::AshTestBase;
using SeaPenTemplateChip = ash::personalization_app::mojom::SeaPenTemplateChip;
using SeaPenTemplateOption =
    ash::personalization_app::mojom::SeaPenTemplateOption;
using SeaPenTemplateId = ash::personalization_app::mojom::SeaPenTemplateId;

TEST_F(SeaPenUtilsTest, GetLargestDisplaySizeSimple) {
  UpdateDisplay("1280x720");
  EXPECT_EQ(gfx::Size(1280, 720), GetLargestDisplaySizeLandscape());
}

TEST_F(SeaPenUtilsTest, GetLargestDisplaySizeRotated) {
  gfx::Size expected(640, 480);

  for (const std::string& display_spec :
       {"640x480/l", "640x480/r", "640x480/u", "480x640"}) {
    UpdateDisplay(display_spec);
    EXPECT_EQ(expected, GetLargestDisplaySizeLandscape()) << display_spec;
  }
}

TEST_F(SeaPenUtilsTest, GetLargestDisplaySizeMultiple) {
  UpdateDisplay("1600x900,1920x1080");
  EXPECT_EQ(gfx::Size(1920, 1080), GetLargestDisplaySizeLandscape());
}

TEST_F(SeaPenUtilsTest, GetLargestDisplaySizeScaleFactor) {
  // The second display is a portrait 4k display with a scale factor of 2.
  // Naively calling display.size() will return {1080,1920}. We still want
  // {3840,2160}.
  UpdateDisplay("2560x1440,3840x2160*2/l");
  EXPECT_EQ(gfx::Size(3840, 2160), GetLargestDisplaySizeLandscape());
}

TEST_F(SeaPenUtilsTest, IsValidTemplate) {
  base::flat_map<ash::personalization_app::mojom::SeaPenTemplateChip,
                 ash::personalization_app::mojom::SeaPenTemplateOption>
      options(
          {{ash::personalization_app::mojom::SeaPenTemplateChip::kFlowerColor,
            ash::personalization_app::mojom::SeaPenTemplateOption::
                kFlowerColorBlue},
           {ash::personalization_app::mojom::SeaPenTemplateChip::kFlowerType,
            ash::personalization_app::mojom::SeaPenTemplateOption::
                kFlowerTypeRose}});
  ash::personalization_app::mojom::SeaPenTemplateQueryPtr template_query =
      ash::personalization_app::mojom::SeaPenTemplateQuery::New(
          ash::personalization_app::mojom::SeaPenTemplateId::kFlower, options,
          ash::personalization_app::mojom::SeaPenUserVisibleQuery::New(
              "test template query", "test template title"));

  EXPECT_TRUE(ash::IsValidTemplateQuery(template_query));
}

TEST_F(SeaPenUtilsTest, IsValidTemplate_wrongChip) {
  base::flat_map<ash::personalization_app::mojom::SeaPenTemplateChip,
                 ash::personalization_app::mojom::SeaPenTemplateOption>
      options(
          {{ash::personalization_app::mojom::SeaPenTemplateChip::kFlowerColor,
            ash::personalization_app::mojom::SeaPenTemplateOption::
                kFlowerColorBlue},
           {ash::personalization_app::mojom::SeaPenTemplateChip::
                kCharactersColor,
            ash::personalization_app::mojom::SeaPenTemplateOption::
                kFlowerTypeRose}});
  ash::personalization_app::mojom::SeaPenTemplateQueryPtr template_query =
      ash::personalization_app::mojom::SeaPenTemplateQuery::New(
          ash::personalization_app::mojom::SeaPenTemplateId::kFlower, options,
          ash::personalization_app::mojom::SeaPenUserVisibleQuery::New(
              "test template query", "test template title"));

  EXPECT_FALSE(ash::IsValidTemplateQuery(template_query));
}

TEST_F(SeaPenUtilsTest, IsValidTemplate_wrongOption) {
  base::flat_map<ash::personalization_app::mojom::SeaPenTemplateChip,
                 ash::personalization_app::mojom::SeaPenTemplateOption>
      options(
          {{ash::personalization_app::mojom::SeaPenTemplateChip::kFlowerColor,
            ash::personalization_app::mojom::SeaPenTemplateOption::
                kFlowerColorBlue},
           {ash::personalization_app::mojom::SeaPenTemplateChip::kFlowerType,
            ash::personalization_app::mojom::SeaPenTemplateOption::
                kMineralColorCool}});
  ash::personalization_app::mojom::SeaPenTemplateQueryPtr template_query =
      ash::personalization_app::mojom::SeaPenTemplateQuery::New(
          ash::personalization_app::mojom::SeaPenTemplateId::kFlower, options,
          ash::personalization_app::mojom::SeaPenUserVisibleQuery::New(
              "test template query", "test template title"));

  EXPECT_FALSE(ash::IsValidTemplateQuery(template_query));
}

TEST_F(SeaPenUtilsTest, IsValidTemplate_tooManyOptions) {
  base::flat_map<ash::personalization_app::mojom::SeaPenTemplateChip,
                 ash::personalization_app::mojom::SeaPenTemplateOption>
      options(
          {{ash::personalization_app::mojom::SeaPenTemplateChip::kFlowerColor,
            ash::personalization_app::mojom::SeaPenTemplateOption::
                kFlowerColorBlue},
           {ash::personalization_app::mojom::SeaPenTemplateChip::kMineralColor,
            ash::personalization_app::mojom::SeaPenTemplateOption::
                kMineralColorCool},
           {ash::personalization_app::mojom::SeaPenTemplateChip::kFlowerType,
            ash::personalization_app::mojom::SeaPenTemplateOption::
                kFlowerTypeRose}});
  ash::personalization_app::mojom::SeaPenTemplateQueryPtr template_query =
      ash::personalization_app::mojom::SeaPenTemplateQuery::New(
          ash::personalization_app::mojom::SeaPenTemplateId::kFlower, options,
          ash::personalization_app::mojom::SeaPenUserVisibleQuery::New(
              "test template query", "test template title"));

  EXPECT_FALSE(ash::IsValidTemplateQuery(template_query));
}

TEST_F(SeaPenUtilsTest, IsValidTemplate_tooFewOptions) {
  base::flat_map<ash::personalization_app::mojom::SeaPenTemplateChip,
                 ash::personalization_app::mojom::SeaPenTemplateOption>
      options({
          {ash::personalization_app::mojom::SeaPenTemplateChip::kFlowerColor,
           ash::personalization_app::mojom::SeaPenTemplateOption::
               kFlowerColorBlue},
      });
  ash::personalization_app::mojom::SeaPenTemplateQueryPtr template_query =
      ash::personalization_app::mojom::SeaPenTemplateQuery::New(
          ash::personalization_app::mojom::SeaPenTemplateId::kFlower, options,
          ash::personalization_app::mojom::SeaPenUserVisibleQuery::New(
              "test template query", "test template title"));

  EXPECT_FALSE(ash::IsValidTemplateQuery(template_query));
}

TEST_F(SeaPenUtilsTest, IsValidTemplate_duplicateChips) {
  base::flat_map<ash::personalization_app::mojom::SeaPenTemplateChip,
                 ash::personalization_app::mojom::SeaPenTemplateOption>
      options(
          {{ash::personalization_app::mojom::SeaPenTemplateChip::kFlowerType,
            ash::personalization_app::mojom::SeaPenTemplateOption::
                kFlowerTypeRose},
           {ash::personalization_app::mojom::SeaPenTemplateChip::kFlowerType,
            ash::personalization_app::mojom::SeaPenTemplateOption::
                kFlowerTypeBirdOfParadise}});
  ash::personalization_app::mojom::SeaPenTemplateQueryPtr template_query =
      ash::personalization_app::mojom::SeaPenTemplateQuery::New(
          ash::personalization_app::mojom::SeaPenTemplateId::kFlower, options,
          ash::personalization_app::mojom::SeaPenUserVisibleQuery::New(
              "test template query", "test template title"));

  EXPECT_FALSE(ash::IsValidTemplateQuery(template_query));
}

TEST_F(SeaPenUtilsTest, GetFeedbackTextFromTemplateQuery) {
  base::flat_map<ash::personalization_app::mojom::SeaPenTemplateChip,
                 ash::personalization_app::mojom::SeaPenTemplateOption>
      options(
          {{ash::personalization_app::mojom::SeaPenTemplateChip::kFlowerType,
            ash::personalization_app::mojom::SeaPenTemplateOption::
                kFlowerTypeRose},
           {ash::personalization_app::mojom::SeaPenTemplateChip::kFlowerColor,
            ash::personalization_app::mojom::SeaPenTemplateOption::
                kFlowerColorBlue}});
  ash::personalization_app::mojom::SeaPenQueryPtr template_query =
      ash::personalization_app::mojom::SeaPenQuery::NewTemplateQuery(
          ash::personalization_app::mojom::SeaPenTemplateQuery::New(
              ash::personalization_app::mojom::SeaPenTemplateId::kFlower,
              options,
              ash::personalization_app::mojom::SeaPenUserVisibleQuery::New(
                  "test template query", "test template title")));

  ash::personalization_app::mojom::SeaPenFeedbackMetadataPtr metadata =
      ash::personalization_app::mojom::SeaPenFeedbackMetadata::New();
  metadata->log_id = "Flower";
  metadata->is_positive = true;
  metadata->generation_seed = 4294967290;

  std::string feedback_text =
      "#AIWallpaper Positive: test template query\ntemplate: Flower\noptions: "
      "(<flower_type>, rose)(<flower_color>, blue)\ngeneration_seed: "
      "4294967290\n";
  EXPECT_EQ(feedback_text, GetFeedbackText(template_query, metadata));
}

TEST_F(SeaPenUtilsTest, GetFeedbackTextFromTextQuery) {
  ash::personalization_app::mojom::SeaPenQueryPtr text_query =
      ash::personalization_app::mojom::SeaPenQuery::NewTextQuery(
          "test text query");

  ash::personalization_app::mojom::SeaPenFeedbackMetadataPtr metadata =
      ash::personalization_app::mojom::SeaPenFeedbackMetadata::New();
  metadata->is_positive = true;
  metadata->generation_seed = 4294967290;

  std::string feedback_text =
      "#AIWallpaper Positive: test text query\ngeneration_seed: "
      "4294967290\n";
  EXPECT_EQ(feedback_text, GetFeedbackText(text_query, metadata));
}

}  // namespace
}  // namespace wallpaper_handlers
