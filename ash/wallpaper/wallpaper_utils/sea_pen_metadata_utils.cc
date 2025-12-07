// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/sea_pen_metadata_utils.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/wallpaper/wallpaper_utils/sea_pen_utils_generated.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "ash/webui/common/mojom/sea_pen_generated.mojom.h"
#include "base/files/file_path.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/re2/src/re2/re2.h"

namespace ash {

namespace {

// Converts a base::Value `time_value` into human-readable string representation
// of the date, such as "Dec 30, 2023". The string is translated into the user's
// current locale. Returns null on failure.
std::optional<std::u16string> GetCreationTimeInfo(
    const base::Value& time_value) {
  auto time = base::ValueToTime(time_value);
  if (!time) {
    DVLOG(2) << __func__ << " invalid time value received";
    return std::nullopt;
  }
  return base::TimeFormatShortDate(*time);
}

std::optional<base::Value::Dict> AsOptionalDict(
    data_decoder::DataDecoder::ValueOrError parsed) {
  if (!parsed.has_value()) {
    LOG(WARNING) << "Failed to parse JSON: " << parsed.error();
    return std::nullopt;
  }
  if (!parsed->is_dict()) {
    LOG(WARNING) << "Parsed JSON is not a dictionary";
    return std::nullopt;
  }
  base::Value::Dict& dict = parsed->GetDict();
  if (!dict.contains(kSeaPenFreeformQueryKey) &&
      !dict.contains(kSeaPenTemplateIdKey)) {
    LOG(WARNING) << "Parsed JSON does not contain required keys";
    return std::nullopt;
  }
  return std::move(dict);
}

personalization_app::mojom::RecentSeaPenImageInfoPtr
SeaPenQueryDictToRecentImageInfo(
    const std::optional<base::Value::Dict> query_dict) {
  if (!query_dict.has_value()) {
    DVLOG(2) << __func__ << " query_dict nullopt";
    return nullptr;
  }
  auto* creation_time = query_dict->Find(kSeaPenCreationTimeKey);
  if (!creation_time) {
    DVLOG(2) << __func__
             << " missing creation time information in extracted data";
    return nullptr;
  }

  auto* freeform_query = query_dict->FindString(kSeaPenFreeformQueryKey);
  if (freeform_query) {
    std::string unescaped_text;
    base::UnescapeBinaryURLComponentSafe(
        *freeform_query, /* fail_on_path_separators= */ false, &unescaped_text);
    return personalization_app::mojom::RecentSeaPenImageInfo::New(
        personalization_app::mojom::SeaPenQuery::NewTextQuery(unescaped_text),
        GetCreationTimeInfo(*creation_time));
  }

  auto* template_id_ptr = query_dict->FindString(kSeaPenTemplateIdKey);
  auto* option_dict = query_dict->FindDict(kSeaPenTemplateOptionsKey);
  if (!template_id_ptr || !option_dict) {
    DVLOG(2) << __func__ << " missing template information in extracted data";
    return nullptr;
  }

  int template_id;
  if (!base::StringToInt(*template_id_ptr, &template_id)) {
    DVLOG(2) << __func__ << " invalid template id received";
    return nullptr;
  }

  base::flat_map<personalization_app::mojom::SeaPenTemplateChip,
                 personalization_app::mojom::SeaPenTemplateOption>
      options;
  for (const auto [chip, option] : *option_dict) {
    int chip_id, option_id;
    if (!base::StringToInt(chip, &chip_id) ||
        !base::StringToInt(option.GetString(), &option_id)) {
      DVLOG(2) << __func__ << " invalid chip option received";
      return nullptr;
    }
    options[static_cast<personalization_app::mojom::SeaPenTemplateChip>(
        chip_id)] =
        static_cast<personalization_app::mojom::SeaPenTemplateOption>(
            option_id);
  }

  auto* user_visible_query_text =
      query_dict->FindString(kSeaPenUserVisibleQueryTextKey);
  auto* user_visible_query_template =
      query_dict->FindString(kSeaPenUserVisibleQueryTemplateKey);

  if (!user_visible_query_text || !user_visible_query_template) {
    DVLOG(2) << __func__
             << " missing user visible query information in extracted data";
    return nullptr;
  }

  personalization_app::mojom::SeaPenTemplateQueryPtr template_query =
      personalization_app::mojom::SeaPenTemplateQuery::New(
          static_cast<personalization_app::mojom::SeaPenTemplateId>(
              template_id),
          options,
          personalization_app::mojom::SeaPenUserVisibleQuery::New(
              *user_visible_query_text, *user_visible_query_template));

  if (!IsValidTemplateQuery(template_query)) {
    DVLOG(2) << __func__ << "invalid template query";
    return nullptr;
  }

  return personalization_app::mojom::RecentSeaPenImageInfo::New(
      personalization_app::mojom::SeaPenQuery::NewTemplateQuery(
          std::move(template_query)),
      GetCreationTimeInfo(*creation_time));
}

std::optional<int> ExtractTemplateIdFromSeaPenQueryDict(
    const std::optional<base::Value::Dict> query_dict) {
  if (!query_dict.has_value()) {
    DVLOG(2) << __func__ << " query_dict nullopt";
    return std::nullopt;
  }

  int template_id;
  auto* template_id_ptr = query_dict->FindString(kSeaPenTemplateIdKey);
  if (!template_id_ptr || !base::StringToInt(*template_id_ptr, &template_id)) {
    return std::nullopt;
  }
  return template_id;
}

}  // namespace

base::Value::Dict SeaPenQueryToDict(
    const personalization_app::mojom::SeaPenQueryPtr& query) {
  base::Value::Dict query_dict = base::Value::Dict();
  query_dict.Set(kSeaPenCreationTimeKey, base::TimeToValue(base::Time::Now()));

  switch (query->which()) {
    case personalization_app::mojom::SeaPenQuery::Tag::kTextQuery:
      query_dict.Set(kSeaPenFreeformQueryKey,
                     base::EscapeAllExceptUnreserved(query->get_text_query()));
      break;
    case personalization_app::mojom::SeaPenQuery::Tag::kTemplateQuery:
      query_dict.Set(kSeaPenTemplateIdKey,
                     base::NumberToString(static_cast<int32_t>(
                         query->get_template_query()->id)));
      base::Value::Dict options_dict = base::Value::Dict();
      for (const auto& [chip, option] : query->get_template_query()->options) {
        options_dict.Set(base::NumberToString(static_cast<int32_t>(chip)),
                         base::NumberToString(static_cast<int32_t>(option)));
      }
      query_dict.Set(kSeaPenTemplateOptionsKey, std::move(options_dict));
      query_dict.Set(kSeaPenUserVisibleQueryTextKey,
                     query->get_template_query()->user_visible_query->text);
      query_dict.Set(
          kSeaPenUserVisibleQueryTemplateKey,
          query->get_template_query()->user_visible_query->template_title);
      break;
  }

  return query_dict;
}

std::string ExtractDcDescriptionContents(std::string_view data) {
  re2::RE2 tag_pattern("<dc:description>(.*)</dc:description>");
  std::string result;
  if (!re2::RE2::PartialMatch(data, tag_pattern, &result)) {
    VLOG(0) << "Failed to find dc:description tag";
    return std::string();
  }
  return result;
}

std::string QueryDictToXmpString(const base::Value::Dict& query_dict) {
  static constexpr char kXmpData[] = R"(
            <x:xmpmeta xmlns:x="adobe:ns:meta/" x:xmptk="XMP Core 6.0.0">
               <rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
                  <rdf:Description rdf:about="" xmlns:dc="http://purl.org/dc/elements/1.1/">
                     <dc:description>%s</dc:description>
                  </rdf:Description>
               </rdf:RDF>
            </x:xmpmeta>)";
  return base::StringPrintf(
      kXmpData, base::WriteJson(query_dict).value_or(std::string()).c_str());
}

void DecodeJsonMetadata(
    const std::string& json,
    base::OnceCallback<
        void(personalization_app::mojom::RecentSeaPenImageInfoPtr)> callback) {
  data_decoder::DataDecoder::ParseJsonIsolated(
      json, base::BindOnce(&AsOptionalDict)
                .Then(base::BindOnce(&SeaPenQueryDictToRecentImageInfo))
                .Then(std::move(callback)));
}

void DecodeJsonMetadataGetTemplateId(
    const std::string& json,
    base::OnceCallback<void(std::optional<int>)> callback) {
  data_decoder::DataDecoder::ParseJsonIsolated(
      json, base::BindOnce(&AsOptionalDict)
                .Then(base::BindOnce(&ExtractTemplateIdFromSeaPenQueryDict))
                .Then(std::move(callback)));
}

std::optional<uint32_t> GetIdFromFileName(const base::FilePath& file_path) {
  const std::string name = file_path.BaseName().RemoveExtension().value();
  uint32_t value;
  if (base::StringToUint(name, &value)) {
    return value;
  }
  LOG(WARNING) << "Invalid SeaPen file_path: " << file_path;
  return std::nullopt;
}

std::vector<uint32_t> GetIdsFromFilePaths(
    const std::vector<base::FilePath>& file_paths) {
  std::vector<uint32_t> result;
  for (const auto& file_path : file_paths) {
    std::optional<uint32_t> id = GetIdFromFileName(file_path);
    if (id.has_value()) {
      result.push_back(id.value());
    }
  }
  return result;
}

bool IsValidTemplateQuery(
    const personalization_app::mojom::SeaPenTemplateQueryPtr& query) {
  const auto query_id = query->id;
  const auto query_options = query->options;
  if (!TemplateToChipSet().contains(query_id)) {
    LOG(WARNING) << "Template id not found.";
    return false;
  }

  const auto chip_set = TemplateToChipSet().find(query_id)->second;
  if (chip_set.size() != query_options.size()) {
    LOG(WARNING) << "The chip size does not match the expected chip size.";
    return false;
  }

  for (const auto& [query_chip, query_option] : query_options) {
    if (!chip_set.contains(query_chip)) {
      // The query chip is not in the template's chip set.
      LOG(WARNING) << "Chip id is not found.";
      return false;
    }
    const auto available_options = ChipToOptionSet().find(query_chip)->second;
    if (!available_options.contains(query_option)) {
      // The query's option is not an allowed option.
      LOG(WARNING) << "Option id not found.";
      return false;
    }
  }
  return true;
}

std::string GetQueryString(
    const personalization_app::mojom::RecentSeaPenImageInfoPtr& ptr) {
  if (ptr.is_null() || ptr->query.is_null()) {
    return std::string();
  }
  switch (ptr->query->which()) {
    case personalization_app::mojom::SeaPenQuery::Tag::kTextQuery:
      return ptr->query->get_text_query();
    case personalization_app::mojom::SeaPenQuery::Tag::kTemplateQuery:
      return ptr->query->get_template_query()->user_visible_query->text;
  }
  return std::string();
}
}  // namespace ash
