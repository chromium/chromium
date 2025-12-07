// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_UTILS_SEA_PEN_METADATA_UTILS_H_
#define ASH_WALLPAPER_WALLPAPER_UTILS_SEA_PEN_METADATA_UTILS_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "ash/webui/common/mojom/sea_pen.mojom-forward.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
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

// Extracts the data between the first <dc:description> tag from `data`. Copies
// the extracted portion into a new return string. Returns empty string on
// failure. Note that failure may happen in normal operation, as VC Background
// metadata should not be wrapped in XML after M124.
ASH_EXPORT std::string ExtractDcDescriptionContents(std::string_view data);

// Prepare SeaPen metadata for writing into a jpg file header by wrapping it in
// XML.
ASH_EXPORT std::string QueryDictToXmpString(
    const base::Value::Dict& query_dict);

// Converts the extracted SeaPen metadata string into
// RecentSeaPenImageInfo. Calls `callback` with nullptr if `metadata` is invalid
// or cannot be safely decoded. `json` must not be wrapped in XMP metadata XML
// tags.
ASH_EXPORT void DecodeJsonMetadata(
    const std::string& json,
    base::OnceCallback<
        void(personalization_app::mojom::RecentSeaPenImageInfoPtr)> callback);

// Decodes the SeaPen metadata `json` string and extracts the template id from
// it. Calls `callback` with nullopt if metadata is invalid or cannot be safely
// decoded. `json` must not be wrapped in XMP metadata XML tags.
ASH_EXPORT void DecodeJsonMetadataGetTemplateId(
    const std::string& json,
    base::OnceCallback<void(std::optional<int>)> callback);

// Extract the id from a sea pen file name. SeaPen images must be saved to disk
// as `/path/to/file/{id}.jpg` where id is a positive integer. `file_path` can
// either include or omit the extension, and can include or omit leading
// directories.
ASH_EXPORT std::optional<uint32_t> GetIdFromFileName(
    const base::FilePath& file_path);

// Extract the valid ids from a vector of file paths. Filters out invalid
// FilePaths, so may return a smaller vector than the input.
ASH_EXPORT std::vector<uint32_t> GetIdsFromFilePaths(
    const std::vector<base::FilePath>& file_paths);

ASH_EXPORT bool IsValidTemplateQuery(
    const personalization_app::mojom::SeaPenTemplateQueryPtr& query);

// Extract the visible query string at the time of the image was saved from
// RecentSeaPenImageInfo `ptr`, empty string if the query is null or invalid.
ASH_EXPORT std::string GetQueryString(
    const personalization_app::mojom::RecentSeaPenImageInfoPtr& ptr);
}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_UTILS_SEA_PEN_METADATA_UTILS_H_
