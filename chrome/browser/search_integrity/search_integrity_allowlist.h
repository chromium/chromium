// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_INTEGRITY_SEARCH_INTEGRITY_ALLOWLIST_H_
#define CHROME_BROWSER_SEARCH_INTEGRITY_SEARCH_INTEGRITY_ALLOWLIST_H_

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "url/gurl.h"

namespace optimization_guide {
class BloomFilter;
}

namespace search_integrity {

// This class manages the allowlist URLs using a bloom filter for efficient
// lookups. It is responsible for parsing the prepopulated_engines.json,
//  normalizing the URLs, and providing a way to
// check if a given URL is part of the allowlist.
class SearchEngineAllowlist {
 public:
  // Returns the singleton instance of the allowlist.
  static SearchEngineAllowlist* GetInstance();

  // Loads the bloom filter data from disk or generates it from the JSON file.
  static std::string LoadBloomFilterData(
      const base::FilePath& json_file_path,
      const base::FilePath& bloom_filter_path);

  // Initializes the allowlist with the provided bloom filter data.
  // This method must be called on the thread where IsAllowed will be used
  void Initialize(const std::string& bloom_filter_data);

  // Checks if a given URL is present in the allowlist. This method normalizes
  // the URL before checking it against the bloom filter.
  bool IsAllowed(const std::string& url) const;

 private:
  friend struct base::DefaultSingletonTraits<SearchEngineAllowlist>;

  SearchEngineAllowlist();
  ~SearchEngineAllowlist();

  // Normalizes a URL by replacing specific placeholders and stripping
  // sensitive query parameters.
  std::string NormalizeUrl(const std::string& url) const;

  // The Bloom filter for storing normalized official search engine URLs.
  std::unique_ptr<optimization_guide::BloomFilter> allowed_urls_bloom_filter_;
};

}  // namespace search_integrity

#endif  // CHROME_BROWSER_SEARCH_INTEGRITY_SEARCH_INTEGRITY_ALLOWLIST_H_
