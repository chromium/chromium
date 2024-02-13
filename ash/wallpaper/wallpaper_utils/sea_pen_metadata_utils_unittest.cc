// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/sea_pen_metadata_utils.h"

#include "ash/test/ash_test_base.h"
#include "base/json/values_util.h"
#include "base/time/time_override.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

base::subtle::ScopedTimeClockOverrides CreateScopedTimeNowOverride() {
  return base::subtle::ScopedTimeClockOverrides(
      []() -> base::Time {
        base::Time fake_now;
        bool success =
            base::Time::FromString("2023-04-05T01:23:45Z", &fake_now);
        DCHECK(success);
        return fake_now;
      },
      nullptr, nullptr);
}

TEST(SeaPenMetadataUtilsTest, SeaPenTextQueryToDict) {
  auto time_override = CreateScopedTimeNowOverride();
  std::string user_search_query = "user search query text";
  ash::personalization_app::mojom::SeaPenQueryPtr search_query =
      ash::personalization_app::mojom::SeaPenQuery::NewTextQuery(
          user_search_query);
  base::Value::Dict result = SeaPenQueryToDict(search_query);
  base::Value::Dict expected =
      base::Value::Dict()
          .Set("creation_time", base::TimeToValue(base::Time::Now()))
          .Set("freeform_query", user_search_query);
  EXPECT_EQ(expected, result);
}

TEST(SeaPenMetadataUtilsTest, SeaPenTemplateQueryToDict) {
  auto time_override = CreateScopedTimeNowOverride();
  base::flat_map<ash::personalization_app::mojom::SeaPenTemplateChip,
                 ash::personalization_app::mojom::SeaPenTemplateOption>
      options(
          {{ash::personalization_app::mojom::SeaPenTemplateChip::kFlowerColor,
            ash::personalization_app::mojom::SeaPenTemplateOption::
                kFlowerColorBlue},
           {ash::personalization_app::mojom::SeaPenTemplateChip::kFlowerType,
            ash::personalization_app::mojom::SeaPenTemplateOption::
                kFlowerTypeRose}});
  ash::personalization_app::mojom::SeaPenQueryPtr search_query =
      ash::personalization_app::mojom::SeaPenQuery::NewTemplateQuery(
          ash::personalization_app::mojom::SeaPenTemplateQuery::New(
              ash::personalization_app::mojom::SeaPenTemplateId::kFlower,
              options,
              ash::personalization_app::mojom::SeaPenUserVisibleQuery::New(
                  "test template query", "test template title")));
  base::Value::Dict result = SeaPenQueryToDict(search_query);
  base::Value::Dict expected =
      base::Value::Dict()
          .Set("creation_time", base::TimeToValue(base::Time::Now()))
          .Set("template_id",
               base::NumberToString(static_cast<int32_t>(
                   ash::personalization_app::mojom::SeaPenTemplateId::kFlower)))
          .Set("options",
               base::Value::Dict()
                   .Set(base::NumberToString(static_cast<int32_t>(
                            ash::personalization_app::mojom::
                                SeaPenTemplateChip::kFlowerColor)),
                        base::NumberToString(static_cast<int32_t>(
                            ash::personalization_app::mojom::
                                SeaPenTemplateOption::kFlowerColorBlue)))
                   .Set(base::NumberToString(static_cast<int32_t>(
                            ash::personalization_app::mojom::
                                SeaPenTemplateChip::kFlowerType)),
                        base::NumberToString(static_cast<int32_t>(
                            ash::personalization_app::mojom::
                                SeaPenTemplateOption::kFlowerTypeRose))))
          .Set("user_visible_query_text", "test template query")
          .Set("user_visible_query_template", "test template title");
  EXPECT_EQ(expected, result);
}

}  // namespace
}  // namespace ash
