// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_integrity/search_integrity.h"

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"  // nogncheck
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_integrity/search_integrity_allowlist.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/browser_resources.h"
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
  constexpr size_t kMinWordLength = 3;

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
  return percent_count >= 3;
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
  std::string json_data =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_SEARCH_ENGINE_PREPOPULATED_ENGINES_JSON);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          [](const base::FilePath& profile_path, const std::string& json_data) {
            // Construct the path to the bloom filter file, which is stored in
            // the user's profile directory.
            base::FilePath bloom_filter_path =
                profile_path.Append(FILE_PATH_LITERAL("engine_allowlist.bf"));

            // Load or build the bloom filter data.
            return SearchEngineAllowlist::LoadBloomFilterData(
                json_data, bloom_filter_path);
          },
          profile_->GetPath(), std::move(json_data)),

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

    if (report.referral_param_found.has_value()) {
      base::UmaHistogramEnumeration("Search.Integrity.Referral.ParameterFound",
                                    report.referral_param_found.value());
    }
  }
}

void SearchIntegrity::LogEnterpriseMetrics(
    const SearchIntegrityReport& report) {
  base::UmaHistogramBoolean(
      "Search.Integrity.Enterprise.IsDefaultCustomWithMatchingPolicyEngine",
      report.is_default_custom_with_matching_policy_engine);

  if (report.referral_param_found.has_value()) {
    base::UmaHistogramEnumeration(
        "Search.Integrity.Enterprise.Referral.ParameterFound",
        report.referral_param_found.value());
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
    size_t keyword_registry_len =
        net::registry_controlled_domains::PermissiveGetHostRegistryLength(
            keyword_host,
            net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
            net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
    size_t search_registry_len =
        net::registry_controlled_domains::PermissiveGetHostRegistryLength(
            search_host,
            net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
            net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);

    // Strip the eTLD
    std::string keyword_base = keyword_domain;
    if (keyword_registry_len != std::string::npos && keyword_registry_len > 0 &&
        keyword_domain.length() > keyword_registry_len) {
      keyword_base = keyword_domain.substr(
          0, keyword_domain.length() - keyword_registry_len - 1);
    }
    std::string search_base = search_domain;
    if (search_registry_len != std::string::npos && search_registry_len > 0 &&
        search_domain.length() > search_registry_len) {
      search_base = search_domain.substr(
          0, search_domain.length() - search_registry_len - 1);
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

}  // namespace search_integrity
