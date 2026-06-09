// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_INTEGRITY_SEARCH_INTEGRITY_H_
#define CHROME_BROWSER_SEARCH_INTEGRITY_SEARCH_INTEGRITY_H_

#include <optional>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
class TemplateURLService;

namespace search_integrity {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SearchDuplicateKeyword)
enum class SearchDuplicateKeyword {
  kNoDuplicates = 0,
  kNonDefaultDuplicated = 1,
  kDefaultDuplicated = 2,
  kBoth = 3,
  kMaxValue = kBoth,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/search/enums.xml:SearchDuplicateKeyword)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SearchReferralParam)
enum class SearchReferralParam {
  kPC = 0,
  kClid = 1,
  kClient = 2,
  kFr = 3,
  kGp = 4,
  kSourceid = 5,
  kT = 6,
  kTt = 7,
  kMaxValue = kTt,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/search/enums.xml:SearchReferralParam)

// A struct to hold the results of the site search integrity check.
struct SiteSearchIntegrityReport {
  bool has_obfuscated_search_url = false;
  bool has_cross_tld_search = false;
  bool has_cross_domain_search = false;
  bool has_extension_url_search = false;
};

// A struct to hold the results of the search integrity check.
struct SearchIntegrityReport {
  bool has_custom_option = false;
  bool is_default_custom = false;
  std::optional<SearchReferralParam> referral_param_found;
  bool is_default_custom_with_matching_policy_engine = false;
  bool is_default_enforced_without_policy = false;
  bool custom_populated_default = false;
  SearchDuplicateKeyword duplicate_keyword_status =
      SearchDuplicateKeyword::kNoDuplicates;
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
  SearchIntegrity(TemplateURLService* template_url_service, Profile* profile);
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

  // Callback executed after the TemplateURLService has finished loading.
  void OnTemplateURLServiceLoaded();

  void LogEnterpriseMetrics(const SearchIntegrityReport& report);

  SearchIntegrityReport CheckSearchEnginesReport();

  SiteSearchIntegrityReport CheckSiteSearchReport();

  // The template URL service, used to access se list.
  raw_ptr<TemplateURLService> template_url_service_;
  // The profile, used to check management status and locate the bloom filter
  // file.
  raw_ptr<Profile> profile_;

  // Subscription for the TemplateURLService loaded callback.
  base::CallbackListSubscription template_url_service_subscription_;

  // Factory for creating weak pointers to this instance, used for safe
  // asynchronous callbacks.
  base::WeakPtrFactory<SearchIntegrity> weak_ptr_factory_{this};
};

}  // namespace search_integrity

#endif  // CHROME_BROWSER_SEARCH_INTEGRITY_SEARCH_INTEGRITY_H_