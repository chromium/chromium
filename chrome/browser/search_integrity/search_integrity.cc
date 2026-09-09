// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_integrity/search_integrity.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"  // nogncheck
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_integrity/search_integrity_allowlist.h"
#include "chrome/browser/search_integrity/search_integrity_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url_service.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace search_integrity {

namespace {

// A list of common words to ignore when comparing search engine names from the
// prepopulated engines list file.
static constexpr auto kStopList = base::MakeFixedFlatSet<std::u16string_view>(
    {u"search", u"engine", u"web", u"internet", u"net", u"plus", u"next",
     u"the", u"and"});

// Returns true if candidate_name and default_name share at least one common
// word.
bool IsNameMatch(std::u16string_view candidate_name,
                 std::u16string_view default_name) {
  auto get_words = [](std::u16string_view text) {
    std::vector<std::u16string> words;
    for (auto piece : base::SplitStringPiece(text, base::kWhitespaceUTF16,
                                             base::TRIM_WHITESPACE,
                                             base::SPLIT_WANT_NONEMPTY)) {
      std::u16string cleaned_piece;
      base::RemoveChars(piece, u".,!?:;\"'()[]{}<>-", &cleaned_piece);
      if (cleaned_piece.length() >= kMinWordLength) {
        words.push_back(base::i18n::ToLower(cleaned_piece));
      }
    }
    return words;
  };

  const auto candidate_words = get_words(candidate_name);
  const auto default_words = get_words(default_name);

  for (const auto& word : candidate_words) {
    if (kStopList.contains(word)) {
      continue;
    }

    for (const auto& def_word : default_words) {
      if (word == def_word) {
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

  return !template_url->CreatedByPolicy() &&
         template_url->starter_pack_id() ==
             template_url_starter_pack_data::StarterPackId::kNone &&
         !SearchEngineAllowlist::GetInstance()->IsAllowed(template_url->url());
}

// Returns true if the URL host has excessive hex escapes (i.e. >= 3 '%').
bool IsObfuscatedUrl(const std::string& url_str) {
  size_t scheme_pos = url_str.find("://");
  size_t host_start = (scheme_pos != std::string::npos) ? scheme_pos + 3 : 0;
  size_t host_end = url_str.find('/', host_start);
  if (host_end == std::string::npos) {
    host_end = url_str.length();
  }
  std::string raw_host = url_str.substr(host_start, host_end - host_start);
  int percent_count = 0;
  for (char c : raw_host) {
    if (c == '%') {
      percent_count++;
    }
  }
  return percent_count >= kObfuscatedUrlPercentThreshold;
}

// Returns true if the search engine is controlled or added by an extension.
bool IsExtensionEngine(const TemplateURL* turl) {
  if (!turl) {
    return false;
  }
  return turl->type() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION ||
         turl->type() == TemplateURL::OMNIBOX_API_EXTENSION ||
         turl->GetExtensionInfo() != nullptr;
}

// Returns the extension ID if the search engine was added by an extension.
std::string GetExtensionId(const TemplateURL* turl) {
  if (turl && turl->GetExtensionInfo()) {
    return turl->GetExtensionInfo()->extension_id;
  }
  return "";
}

}  // namespace

SearchIntegrity::SearchIntegrity(TemplateURLService* template_url_service,
                                 Profile* profile)
    : template_url_service_(template_url_service), profile_(profile) {}

SearchIntegrity::~SearchIntegrity() = default;

void SearchIntegrity::CheckSearchEngines() {
  // Asynchronously initialize the search engine allowlist on a background
  // thread to avoid blocking the UI thread.

  // Get the JSON data from resources.
#if defined(IDR_SEARCH_ENGINE_HISTORICAL_SEARCH_URLS_JSON)
  std::string json_data =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_SEARCH_ENGINE_HISTORICAL_SEARCH_URLS_JSON);
#else
  std::string json_data;
#endif

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          [](std::string json_data) {
            return SearchEngineAllowlist::BuildAllowlist(json_data);
          },
          std::move(json_data)),

      // Once the background task is complete, run OnAllowlistInitialized on the
      // original (UI) thread.
      base::BindOnce(&SearchIntegrity::OnAllowlistInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SearchIntegrity::OnAllowlistInitialized(
    absl::flat_hash_set<std::string> allowed_urls) {
  if (!template_url_service_) {
    return;
  }

  SearchEngineAllowlist::GetInstance()->Initialize(std::move(allowed_urls));

  if (template_url_service_->loaded()) {
    OnTemplateURLServiceLoaded();
  } else {
    template_url_service_subscription_ =
        template_url_service_->RegisterOnLoadedCallback(
            base::BindOnce(&SearchIntegrity::OnTemplateURLServiceLoaded,
                           weak_ptr_factory_.GetWeakPtr()));
    template_url_service_->Load();
  }
}

void SearchIntegrity::OnTemplateURLServiceLoaded() {
  SearchIntegrityReport report = CheckSearchEnginesReport();

  if (enterprise_util::IsBrowserManaged(profile_)) {
    LogEnterpriseMetrics(report);
  } else {
    base::UmaHistogramBoolean("Search.Integrity.HasCustomSearchEngine",
                              report.has_custom_option);
    base::UmaHistogramBoolean("Search.Integrity.IsDefaultSearchEngineCustom",
                              report.is_default_custom);
    base::UmaHistogramBoolean(
        "Search.Integrity.IsDefaultCustomWithMatchingPolicyEngine",
        report.is_default_custom_with_matching_policy_engine);
    base::UmaHistogramBoolean("Search.Integrity.IsDefaultEnforcedWithoutPolicy",
                              report.is_default_enforced_without_policy);
    base::UmaHistogramEnumeration("Search.Integrity.DuplicateKeyword",
                                  report.duplicate_keyword_status);
    base::UmaHistogramBoolean("Search.Integrity.CustomPopulatedDefault",
                              report.custom_populated_default);

    if (report.referral_param_found.has_value()) {
      base::UmaHistogramEnumeration("Search.Integrity.Referral.ParameterFound",
                                    report.referral_param_found.value());
    }
  }

  SiteSearchIntegrityReport site_report = CheckSiteSearchReport();

  base::UmaHistogramBoolean("Search.Integrity.ObfuscatedSearchUrl",
                            site_report.has_obfuscated_search_url);
  base::UmaHistogramBoolean("Search.Integrity.CrossTldSearch",
                            site_report.has_cross_tld_search);
  base::UmaHistogramBoolean("Search.Integrity.CrossDomainSearch",
                            site_report.has_cross_domain_search);
  base::UmaHistogramBoolean("Search.Integrity.ExtensionUrlSearch",
                            site_report.has_extension_url_search);

  DuplicateKeywordDetailedReport duplicate_report =
      CheckDuplicateKeywordReport();
  LogDuplicateKeywordMetrics(duplicate_report);
}

void SearchIntegrity::LogEnterpriseMetrics(
    const SearchIntegrityReport& report) {
  base::UmaHistogramBoolean(
      "Search.Integrity.Enterprise.IsDefaultCustomWithMatchingPolicyEngine",
      report.is_default_custom_with_matching_policy_engine);
  base::UmaHistogramBoolean(
      "Search.Integrity.Enterprise.IsDefaultEnforcedWithoutPolicy",
      report.is_default_enforced_without_policy);
  base::UmaHistogramEnumeration("Search.Integrity.Enterprise.DuplicateKeyword",
                                report.duplicate_keyword_status);

  if (report.referral_param_found.has_value()) {
    base::UmaHistogramEnumeration(
        "Search.Integrity.Enterprise.Referral.ParameterFound",
        report.referral_param_found.value());
  }
}

void SearchIntegrity::LogDuplicateKeywordMetrics(
    const DuplicateKeywordDetailedReport& report) {
  if (report.distinct_duplicated_keywords_count == 0) {
    return;
  }

  const bool is_enterprise = enterprise_util::IsBrowserManaged(profile_);
  const std::string prefix =
      is_enterprise ? "Search.Integrity.Enterprise.DuplicateKeyword."
                    : "Search.Integrity.DuplicateKeyword.";

  base::UmaHistogramCounts100(base::StrCat({prefix, "DuplicatedKeywordsCount"}),
                              report.distinct_duplicated_keywords_count);

  const std::string entries_histogram_name =
      base::StrCat({prefix, "EntriesPerDuplicatedKeyword"});
  for (int count : report.entries_per_duplicated_keyword) {
    base::UmaHistogramCounts100(entries_histogram_name, count);
  }

  base::UmaHistogramBoolean(base::StrCat({prefix, "HasExtensionOnlyDuplicate"}),
                            report.has_extension_only_duplicate);

  base::UmaHistogramBoolean(
      base::StrCat({prefix, "HasMixedExtensionDuplicate"}),
      report.has_mixed_extension_duplicate);

  base::UmaHistogramBoolean(base::StrCat({prefix, "HasStarterPackDuplicate"}),
                            report.has_starter_pack_duplicate);

  base::UmaHistogramBoolean(base::StrCat({prefix, "HasTrivialDuplicates"}),
                            report.has_trivial_duplicates);
}

SearchIntegrityReport SearchIntegrity::CheckSearchEnginesReport() {
  SearchIntegrityReport report;

  // Retrieve the list of all installed search engines.
  auto template_urls = template_url_service_->GetTemplateURLs();

  // A map of referral parameter keys to their corresponding enum values.
  static const base::NoDestructor<
      std::map<std::string_view, SearchReferralParam>>
      kReferralParameterMap({
          {"PC", SearchReferralParam::kPC},
          {"clid", SearchReferralParam::kClid},
          {"client", SearchReferralParam::kClient},
          {"fr", SearchReferralParam::kFr},
          {"gp", SearchReferralParam::kGp},
          {"sourceid", SearchReferralParam::kSourceid},
          {"t", SearchReferralParam::kT},
          {"tt", SearchReferralParam::kTt},
      });

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
        auto iter = kReferralParameterMap->find(it.GetKey());
        if (iter != kReferralParameterMap->end()) {
          report.referral_param_found = iter->second;
          break;
        }
      }
    }
  }

  const TemplateURL* default_search_provider =
      template_url_service_->GetDefaultSearchProvider();

  std::set<std::u16string> seen_keywords;
  bool default_duplicated = false;
  bool non_default_duplicated = false;

  const std::u16string default_keyword =
      default_search_provider
          ? base::i18n::ToLower(default_search_provider->keyword())
          : std::u16string();

  for (const TemplateURL* turl : template_urls) {
    std::u16string keyword = base::i18n::ToLower(turl->keyword());
    if (!seen_keywords.insert(keyword).second) {
      if (default_search_provider && keyword == default_keyword) {
        default_duplicated = true;
      } else {
        non_default_duplicated = true;
      }
    }
  }

  if (default_duplicated && non_default_duplicated) {
    report.duplicate_keyword_status = SearchDuplicateKeyword::kBoth;
  } else if (default_duplicated) {
    report.duplicate_keyword_status =
        SearchDuplicateKeyword::kDefaultDuplicated;
  } else if (non_default_duplicated) {
    report.duplicate_keyword_status =
        SearchDuplicateKeyword::kNonDefaultDuplicated;
  } else {
    report.duplicate_keyword_status = SearchDuplicateKeyword::kNoDuplicates;
  }

  if (!default_search_provider) {
    return report;
  }

  if (default_search_provider->enforced_by_policy() &&
      !profile_->GetPrefs()->IsManagedPreference(
          prefs::kDefaultSearchProviderEnabled)) {
    report.is_default_enforced_without_policy = true;
  }

  if (IsDisallowedCustomSearchEngine(default_search_provider)) {
    report.is_default_custom = true;
    if (default_search_provider->prepopulate_id() != 0) {
      report.custom_populated_default = true;
    }
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

SiteSearchIntegrityReport SearchIntegrity::CheckSiteSearchReport() {
  SiteSearchIntegrityReport report;

  for (const TemplateURL* template_url :
       template_url_service_->GetTemplateURLs()) {
    // Check whether the keyword is a URL, return if it's not
    std::string keyword = base::UTF16ToUTF8(template_url->keyword());
    if (keyword.find('.') == std::string::npos) {
      continue;
    }
    std::string keyword_url_str = keyword;
    if (keyword.find("://") == std::string::npos) {
      keyword_url_str = "http://" + keyword;
    }
    GURL keyword_url(keyword_url_str);
    if (!keyword_url.is_valid() || !keyword_url.has_host()) {
      continue;
    }

    GURL search_url(template_url->url());
    if (!search_url.is_valid() || !search_url.has_host()) {
      continue;
    }

    if (base::StartsWith(template_url->url(), "chrome-extension://",
                         base::CompareCase::SENSITIVE)) {
      report.has_extension_url_search = true;
      continue;
    }

    std::string keyword_host(keyword_url.host());
    std::string search_host(search_url.host());

    if (IsObfuscatedUrl(keyword) || IsObfuscatedUrl(template_url->url())) {
      report.has_obfuscated_search_url = true;
    }

    std::string keyword_domain =
        net::registry_controlled_domains::GetDomainAndRegistry(
            keyword_host,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

    if (keyword_domain.empty()) {
      continue;
    }

    std::string search_domain =
        net::registry_controlled_domains::GetDomainAndRegistry(
            search_host,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

    if (search_domain.empty()) {
      search_domain = search_host;
    }

    if (keyword_domain == search_domain) {
      continue;
    }

    // At this point we know the domains are not the same, so what's left is to
    // check whether it's a completely different domain, or a similar domain in
    // a different eTLD.
    std::optional<size_t> keyword_registry_len =
        net::registry_controlled_domains::PermissiveGetHostRegistry(
            keyword_host,
            net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
            net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES)
            .transform(&std::string_view::size);
    std::optional<size_t> search_registry_len =
        net::registry_controlled_domains::PermissiveGetHostRegistry(
            search_host,
            net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
            net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES)
            .transform(&std::string_view::size);

    // Strip the eTLD
    std::string keyword_base = keyword_domain;
    if (keyword_registry_len > 0 &&
        keyword_domain.length() > keyword_registry_len) {
      keyword_base = keyword_domain.substr(
          0, keyword_domain.length() - *keyword_registry_len - 1);
    }
    std::string search_base = search_domain;
    if (search_registry_len > 0 &&
        search_domain.length() > search_registry_len) {
      search_base = search_domain.substr(
          0, search_domain.length() - *search_registry_len - 1);
    }

    if (keyword_base == search_base && !keyword_base.empty()) {
      // Keyword and search have the same (non-empty) domain when eTLD is
      // stripped.
      report.has_cross_tld_search = true;
    } else {
      // Keyword and search have different domains even when eTLD is stripped.
      report.has_cross_domain_search = true;
    }
  }

  return report;
}

DuplicateKeywordDetailedReport SearchIntegrity::CheckDuplicateKeywordReport() {
  DuplicateKeywordDetailedReport report;
  if (!template_url_service_) {
    return report;
  }

  const TemplateURLService::TemplateURLVector template_urls =
      template_url_service_->GetTemplateURLs();

  // Bucket all TemplateURLs by normalized keyword
  std::map<std::u16string, std::vector<const TemplateURL*>> keyword_clusters;
  for (const TemplateURL* turl : template_urls) {
    if (!turl) {
      continue;
    }
    std::u16string normalized_kw = base::i18n::ToLower(turl->keyword());
    if (normalized_kw.empty()) {
      continue;
    }
    keyword_clusters[normalized_kw].push_back(turl);
  }

  // Inspect clusters that have duplicates
  for (const auto& [keyword, cluster] : keyword_clusters) {
    if (cluster.size() <= 1) {
      continue;
    }

    report.distinct_duplicated_keywords_count++;
    report.entries_per_duplicated_keyword.push_back(
        static_cast<int>(cluster.size()));

    // Trivial Duplicates (identical URLs) and Starter Pack check
    if (!keyword.empty() && keyword[0] == u'@') {
      report.has_starter_pack_duplicate = true;
    }
    std::set<std::string> unique_urls;
    for (const TemplateURL* turl : cluster) {
      unique_urls.insert(turl->url());
      if (turl->starter_pack_id() !=
          template_url_starter_pack_data::StarterPackId::kNone) {
        report.has_starter_pack_duplicate = true;
      }
    }
    if (unique_urls.size() < cluster.size()) {
      report.has_trivial_duplicates = true;
    }

    // Extension collisions
    std::set<std::string> extension_ids;
    bool has_non_extension = false;
    int unknown_ext_idx = 0;

    for (const TemplateURL* turl : cluster) {
      if (IsExtensionEngine(turl)) {
        std::string ext_id = GetExtensionId(turl);
        if (!ext_id.empty()) {
          extension_ids.insert(ext_id);
        } else {
          extension_ids.insert(base::StrCat(
              {"unknown_ext_", base::NumberToString(++unknown_ext_idx)}));
        }
      } else {
        has_non_extension = true;
      }
    }

    if (!extension_ids.empty() && has_non_extension) {
      report.has_mixed_extension_duplicate = true;
    } else if (extension_ids.size() >= 2 && !has_non_extension) {
      report.has_extension_only_duplicate = true;
    }
  }

  return report;
}

}  // namespace search_integrity
