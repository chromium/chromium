// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_integrity/search_integrity.h"

#include <set>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/search_integrity/search_integrity_allowlist.h"
#include "chrome/common/chrome_paths.h"
#include "components/search_engines/template_url_service.h"
#include "net/base/url_util.h"

namespace search_integrity {

namespace {

// Returns true if candidate_name and default_name share at least one common
// word.
bool IsNameMatch(std::u16string_view candidate_name,
                 std::u16string_view default_name) {
  for (const auto& name_one : base::SplitStringPiece(
           candidate_name, base::kWhitespaceUTF16, base::TRIM_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    for (const auto& name_two : base::SplitStringPiece(
             default_name, base::kWhitespaceUTF16, base::TRIM_WHITESPACE,
             base::SPLIT_WANT_NONEMPTY)) {
      if (base::i18n::ToLower(name_one) == base::i18n::ToLower(name_two)) {
        return true;
      }
    }
  }
  return false;
}

// Returns true if the given search engine is a custom engine that is not in the
// allowlist.
bool IsDisallowedCustomSearchEngine(const TemplateURL* template_url) {
  if (!template_url) {
    return false;
  }
  return template_url->prepopulate_id() == 0 &&
         !template_url->CreatedByPolicy() &&
         template_url->starter_pack_id() ==
             template_url_starter_pack_data::StarterPackId::kNone &&
         !SearchEngineAllowlist::GetInstance()->IsAllowed(template_url->url());
}

}  // namespace

SearchIntegrity::SearchIntegrity(TemplateURLService* template_url_service,
                                 const base::FilePath& profile_path)
    : template_url_service_(template_url_service),
      profile_path_(profile_path) {}

SearchIntegrity::~SearchIntegrity() = default;

void SearchIntegrity::CheckSearchEngines() {
  // Asynchronously initialize the search engine allowlist on a background
  // thread to avoid blocking the UI thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          [](const base::FilePath& profile_path) {
            // Construct the path to the prepopulated_engines.json file, which
            // is a bundled resource.
            base::FilePath json_path;
            if (!base::PathService::Get(chrome::DIR_RESOURCES, &json_path)) {
              return std::string();
            }
            json_path =
                json_path.Append(FILE_PATH_LITERAL("third_party"))
                    .Append(FILE_PATH_LITERAL("search_engines_data"))
                    .Append(FILE_PATH_LITERAL("resources"))
                    .Append(FILE_PATH_LITERAL("definitions"))
                    .Append(FILE_PATH_LITERAL("prepopulated_engines.json"));

            // Construct the path to the bloom filter file, which is stored in
            // the user's profile directory.
            base::FilePath bloom_filter_path =
                profile_path.Append(FILE_PATH_LITERAL("engine_allowlist.bf"));

            // Load or build the bloom filter data.
            return SearchEngineAllowlist::LoadBloomFilterData(
                json_path, bloom_filter_path);
          },
          profile_path_),

      // Once the background task is complete, run OnAllowlistInitialized on the
      // original (UI) thread.
      base::BindOnce(&SearchIntegrity::OnAllowlistInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SearchIntegrity::OnAllowlistInitialized(
    const std::string& bloom_filter_data) {
  if (!template_url_service_) {
    return;
  }

  SearchEngineAllowlist::GetInstance()->Initialize(bloom_filter_data);

  SearchIntegrityReport report = CheckSearchEnginesReport();

  base::UmaHistogramBoolean("Search.Integrity.HasCustomSearchEngine",
                            report.has_custom_option);
  base::UmaHistogramBoolean("Search.Integrity.IsDefaultSearchEngineCustom",
                            report.is_default_custom);
  base::UmaHistogramBoolean(
      "Search.Integrity.IsDefaultCustomWithMatchingPolicyEngine",
      report.is_default_custom_with_matching_policy_engine);
}

SearchIntegrityReport SearchIntegrity::CheckSearchEnginesReport() {
  SearchIntegrityReport report;

  // Retrieve the list of all installed search engines.
  auto template_urls = template_url_service_->GetTemplateURLs();

  // Iterate through all installed search engines to check if any of them are
  // not in the allowlist.
  for (const TemplateURL* template_url : template_urls) {
    // Only consider engines that appear in the "Default Search Engines" list.
    if (!template_url_service_->ShowInDefaultList(template_url)) {
      continue;
    }

    if (IsDisallowedCustomSearchEngine(template_url)) {
      report.has_custom_option = true;

      GURL url(template_url->url());
      for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
        if (it.GetKey() == "fr") {
          report.referral_id = it.GetUnescapedValue();
          break;
        }
      }
      break;
    }
  }

  const TemplateURL* default_search_provider =
      template_url_service_->GetDefaultSearchProvider();

  if (!default_search_provider) {
    return report;
  }

  if (IsDisallowedCustomSearchEngine(default_search_provider)) {
    report.is_default_custom = true;

    const std::u16string& default_name = default_search_provider->short_name();

    for (const TemplateURL* template_url : template_urls) {
      // Avoid comparing the engine to itself.
      if (template_url == default_search_provider) {
        continue;
      }

      const std::u16string& candidate_name = template_url->short_name();
      const bool names_match = IsNameMatch(candidate_name, default_name);

      if (names_match &&
          template_url->url() != default_search_provider->url() &&
          !IsDisallowedCustomSearchEngine(template_url)) {
        report.is_default_custom_with_matching_policy_engine = true;
        break;
      }
    }
  }

  return report;
}

}  // namespace search_integrity
