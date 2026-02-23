// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_integrity/search_integrity_allowlist.h"

#include <sstream>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/optimization_guide/core/filters/bloom_filter.h"
#include "url/gurl.h"

namespace search_integrity {

namespace {

// The number of hash functions and bits for the bloom filter. These values are
// chosen based on the expected number of search engine entries (~200)
// and a desired false-positive rate of ~1%.
constexpr int kNumHashFunctions = 10;
constexpr int kNumBits = 2875;

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
std::string SearchEngineAllowlist::LoadBloomFilterData(
    const base::FilePath& json_file_path,
    const base::FilePath& bloom_filter_path) {
  // Attempt to load an existing bloom filter from the file system.
  // This provides a fast startup path if the filter has been previously
  // generated and saved.
  std::string bloom_filter_data;
  if (base::ReadFileToString(bloom_filter_path, &bloom_filter_data)) {
    return bloom_filter_data;
  }

  // If loading the bloom filter fails, parse the JSON file to build it.
  // This typically happens on the first run or if the filter file is missing.
  std::string json_data;
  if (!base::ReadFileToString(json_file_path, &json_data)) {
    return std::string();
  }

  std::optional<base::Value> root =
      base::JSONReader::Read(json_data, base::JSON_PARSE_CHROMIUM_EXTENSIONS |
                                            base::JSON_ALLOW_TRAILING_COMMAS);
  // Check if parsing was successful and if the root is a dictionary.
  if (!root || !root->is_dict()) {
    return std::string();
  }

  const base::DictValue* elements_dict = root->GetDict().FindDict("elements");
  if (!elements_dict) {
    return std::string();
  }

  auto bloom_filter = std::make_unique<optimization_guide::BloomFilter>(
      kNumHashFunctions, kNumBits);

  for (auto element : *elements_dict) {
    const base::DictValue* engine_dict = element.second.GetIfDict();
    if (!engine_dict) {
      continue;
    }

    const std::string* search_url_str = engine_dict->FindString("search_url");
    if (search_url_str) {
      bloom_filter->Add(NormalizeUrlHelper(*search_url_str));
    }

    // Extract and normalize any alternate URLs.
    const base::ListValue* alternate_urls_list =
        engine_dict->FindList("alternate_urls");
    if (alternate_urls_list) {
      for (const auto& item : *alternate_urls_list) {
        if (item.is_string()) {
          bloom_filter->Add(NormalizeUrlHelper(item.GetString()));
        }
      }
    }
  }

  const std::vector<uint8_t>& bytes = bloom_filter->bytes();
  base::WriteFile(bloom_filter_path,
                  base::as_bytes(base::span<const uint8_t>(bytes)));

  return std::string(bytes.begin(), bytes.end());
}

void SearchEngineAllowlist::Initialize(const std::string& bloom_filter_data) {
  if (bloom_filter_data.empty()) {
    return;
  }
  allowed_urls_bloom_filter_ =
      std::make_unique<optimization_guide::BloomFilter>(
          kNumHashFunctions, kNumBits, bloom_filter_data);
}

std::string SearchEngineAllowlist::NormalizeUrl(const std::string& url) const {
  return NormalizeUrlHelper(url);
}

bool SearchEngineAllowlist::IsAllowed(const std::string& url) const {
  // If the bloom filter has not been initialized, no URLs are allowed.
  if (!allowed_urls_bloom_filter_) {
    return false;
  }
  // Normalize the input URL and check if it exists in the bloom filter.
  return allowed_urls_bloom_filter_->Contains(NormalizeUrl(url));
}

}  // namespace search_integrity
