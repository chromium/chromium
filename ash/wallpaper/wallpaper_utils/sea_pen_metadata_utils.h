// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_UTILS_SEA_PEN_METADATA_UTILS_H_
#define ASH_WALLPAPER_WALLPAPER_UTILS_SEA_PEN_METADATA_UTILS_H_

#include <string>
#include <string_view>

#include "ash/ash_export.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/values.h"

namespace ash {

// Keys for fields stored in SeaPen metadata json.
inline constexpr std::string_view kSeaPenCreationTimeKey = "creation_time";
inline constexpr std::string_view kSeaPenFreeformQueryKey = "freeform_query";
inline constexpr std::string_view kSeaPenTemplateIdKey = "template_id";
inline constexpr std::string_view kSeaPenTemplateOptionsKey = "options";
inline constexpr std::string_view kSeaPenUserVisibleQueryTextKey =
    "user_visible_query_text";
inline constexpr std::string_view kSeaPenUserVisibleQueryTemplateKey =
    "user_visible_query_template";

/**
 * Serializes a sea pen query information `query` into
 * base::Value::Dict format based on the query type. Such as
 * {creation_time:<number>, freeform_query:<string>} or {creation_time:<number>,
 * user_visible_query_text:<string>, user_visible_query_template:<string>,
 * template_id:<number>, options:{<chip_number>:<option_number>, ...}}. For
 * example:
 * {"creation_time":"13349580387513653","freeform_query":"test freeform query"}
 * {"creation_time":"13349580387513653", "user_visible_query_text": "test
 * template query", "user_visible_query_template": "test template",
 * "template_id":"2","options":{"4":"34","5":"40"}}
 *
 * @param query  pointer to the sea pen query
 * @return query information in base::Value::Dict format
 */
ASH_EXPORT base::Value::Dict SeaPenQueryToDict(
    const personalization_app::mojom::SeaPenQueryPtr& query);

// Constructs the xmp metadata string from the base::Value::Dict query
// information.
ASH_EXPORT std::string QueryDictToXmpString(
    const base::Value::Dict& query_dict);

// Converts the extracted Sea Pen metadata base::Value::Dict `query_dict` into
// RecentSeaPenImageInfo.
ASH_EXPORT personalization_app::mojom::RecentSeaPenImageInfoPtr
SeaPenQueryDictToRecentImageInfo(const base::Value::Dict& query_dict);
}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_UTILS_SEA_PEN_METADATA_UTILS_H_
