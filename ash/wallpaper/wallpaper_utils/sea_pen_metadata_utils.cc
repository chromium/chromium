// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/sea_pen_metadata_utils.h"

#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"

namespace ash {

base::Value::Dict SeaPenQueryToDict(
    const personalization_app::mojom::SeaPenQueryPtr& query) {
  base::Value::Dict query_dict = base::Value::Dict();
  query_dict.Set(kSeaPenCreationTimeKey, base::TimeToValue(base::Time::Now()));

  switch (query->which()) {
    case personalization_app::mojom::SeaPenQuery::Tag::kTextQuery:
      query_dict.Set(kSeaPenFreeformQueryKey, query->get_text_query());
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

std::string QueryDictToXmpString(const base::Value::Dict& query_dict) {
  static constexpr char kXmpData[] = R"(
            <x:xmpmeta xmlns:x="adobe:ns:meta/" x:xmptk="XMP Core 6.0.0">
               <rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
                  <rdf:Description rdf:about="" xmlns:dc="http://purl.org/dc/elements/1.1/">
                     <dc:description>%s</dc:description>
                  </rdf:Description>
               </rdf:RDF>
            </x:xmpmeta>)";
  return base::StringPrintf(kXmpData,
                            base::WriteJson(query_dict).value_or("").c_str());
}

}  // namespace ash
