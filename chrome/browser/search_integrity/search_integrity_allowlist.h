// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_INTEGRITY_SEARCH_INTEGRITY_ALLOWLIST_H_
#define CHROME_BROWSER_SEARCH_INTEGRITY_SEARCH_INTEGRITY_ALLOWLIST_H_

#include <string>

#include "base/memory/singleton.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace search_integrity {

// This class manages the allowlist URLs using an in-memory set for efficient
// lookups. It is responsible for parsing search engine definitions JSON,
// normalizing the URLs, and providing a way to check if a given URL is part of
// the allowlist.
class SearchEngineAllowlist {
 public:
  // Returns the singleton instance of the allowlist.
  static SearchEngineAllowlist* GetInstance();

  // Parses the JSON data containing search engine definitions and builds an
  // allowlist of normalized URLs. This is separated from Initialize() so that
  // JSON parsing can be performed asynchronously on a background thread (e.g.
  // ThreadPool) to avoid blocking the UI thread during startup.
  static absl::flat_hash_set<std::string> BuildAllowlist(
      const std::string& historical_json_data);

  // Initializes the allowlist singleton with the pre-built set of URLs. This
  // method must be called on the UI thread.
  void Initialize(absl::flat_hash_set<std::string> allowed_urls);

  // Resets the allowlist state for testing.
  void ResetForTesting();

  // Checks if a given URL is present in the allowlist. This method normalizes
  // the URL before checking it against the allowlist.
  bool IsAllowed(const std::string& url) const;

 private:
  friend struct base::DefaultSingletonTraits<SearchEngineAllowlist>;

  SearchEngineAllowlist();
  ~SearchEngineAllowlist();

  // Normalizes a URL by replacing specific placeholders.
  std::string NormalizeUrl(const std::string& url) const;

  // The set storing normalized official search engine URLs.
  absl::flat_hash_set<std::string> allowed_urls_;
};

}  // namespace search_integrity

#endif  // CHROME_BROWSER_SEARCH_INTEGRITY_SEARCH_INTEGRITY_ALLOWLIST_H_
