// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_INTEGRITY_SEARCH_INTEGRITY_H_
#define CHROME_BROWSER_SEARCH_INTEGRITY_SEARCH_INTEGRITY_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class TemplateURLService;

namespace search_integrity {

// A struct to hold the results of the search integrity check.
struct SearchIntegrityReport {
  bool has_custom_option = false;
  bool is_default_custom = false;
  std::string referral_id;
  bool is_default_custom_with_matching_policy_engine = false;
};

// Manages the Search Integrity feature, which detects non-standard search
// engines. This service is responsible for checking the user's installed and
// default search engines against a predefined allowlist and recording metrics
// based on the findings.
class SearchIntegrity : public KeyedService {
 public:
  // Constructs a SearchIntegrity service instance.
  // `template_url_service`: The service for accessing the
  // engines.
  // `profile_path`: The path to the profile directory, used for storing
  // the bloom filter.
  SearchIntegrity(TemplateURLService* template_url_service,
                  const base::FilePath& profile_path);
  ~SearchIntegrity() override;

  SearchIntegrity(const SearchIntegrity&) = delete;
  SearchIntegrity& operator=(const SearchIntegrity&) = delete;

  // Checks the user's search engines against the allowlist. This will
  // initialize the allowlist if it hasn't been already. The initialization is
  // performed asynchronously on a background thread.
  void CheckSearchEngines();

 private:
  friend class SearchIntegrityTest;

  // Callback executed after the allowlist has been initialized. This method
  // proceeds with checking and recording metrics.
  void OnAllowlistInitialized(const std::string& bloom_filter_data);

  SearchIntegrityReport CheckSearchEnginesReport();

  // The template URL service, used to access se list.
  raw_ptr<TemplateURLService> template_url_service_;
  // The path to the profile, used to locate the bloom filter file.
  const base::FilePath profile_path_;

  // Factory for creating weak pointers to this instance, used for safe
  // asynchronous callbacks.
  base::WeakPtrFactory<SearchIntegrity> weak_ptr_factory_{this};
};

}  // namespace search_integrity

#endif  // CHROME_BROWSER_SEARCH_INTEGRITY_SEARCH_INTEGRITY_H_
