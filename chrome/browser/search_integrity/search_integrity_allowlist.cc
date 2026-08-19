// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_integrity/search_integrity_allowlist.h"

#include <optional>
#include <string>
#include <utility>

#include "base/json/json_reader.h"
#include "base/memory/singleton.h"
#include "base/strings/string_util.h"
#include "base/values.h"

namespace search_integrity {

namespace {

std::string NormalizeUrlHelper(const std::string& url) {
  std::string normalized_url = url;
  // Replace the search terms placeholder for consistent URL comparisons.
  base::ReplaceSubstringsAfterOffset(&normalized_url, 0, "{searchTerms}", "%s");

  return normalized_url;
}

}  // namespace

// static
SearchEngineAllowlist* SearchEngineAllowlist::GetInstance() {
  return base::Singleton<SearchEngineAllowlist>::get();
}

SearchEngineAllowlist::SearchEngineAllowlist() = default;
SearchEngineAllowlist::~SearchEngineAllowlist() = default;

// static
absl::flat_hash_set<std::string> SearchEngineAllowlist::BuildAllowlist(
    const std::string& historical_json_data) {
  absl::flat_hash_set<std::string> allowed_urls;
  if (historical_json_data.empty()) {
    return allowed_urls;
  }

  std::optional<base::Value> root =
      base::JSONReader::Read(historical_json_data,
                             base::JSON_PARSE_CHROMIUM_EXTENSIONS |
                                 base::JSON_ALLOW_TRAILING_COMMAS);
  if (!root || !root->is_dict()) {
    return allowed_urls;
  }

  const base::DictValue* elements_dict = root->GetDict().FindDict("elements");
  if (!elements_dict) {
    return allowed_urls;
  }

  for (const auto element : *elements_dict) {
    const base::DictValue* engine_dict = element.second.GetIfDict();
    if (!engine_dict) {
      continue;
    }

    const std::string* search_url_str = engine_dict->FindString("search_url");
    if (search_url_str && !search_url_str->empty()) {
      allowed_urls.insert(NormalizeUrlHelper(*search_url_str));
    }

    const base::ListValue* alternate_urls_list =
        engine_dict->FindList("alternate_urls");
    if (alternate_urls_list) {
      for (const auto& item : *alternate_urls_list) {
        if (item.is_string() && !item.GetString().empty()) {
          allowed_urls.insert(NormalizeUrlHelper(item.GetString()));
        }
      }
    }
  }

  return allowed_urls;
}

void SearchEngineAllowlist::Initialize(
    absl::flat_hash_set<std::string> allowed_urls) {
  allowed_urls_ = std::move(allowed_urls);
}

void SearchEngineAllowlist::ResetForTesting() {
  allowed_urls_.clear();
}

std::string SearchEngineAllowlist::NormalizeUrl(const std::string& url) const {
  return NormalizeUrlHelper(url);
}

bool SearchEngineAllowlist::IsAllowed(const std::string& url) const {
  if (allowed_urls_.empty()) {
    return false;
  }
  return allowed_urls_.contains(NormalizeUrl(url));
}

}  // namespace search_integrity
