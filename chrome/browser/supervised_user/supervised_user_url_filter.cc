// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_url_filter.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <unordered_map>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/supervised_user/experimental/supervised_user_blacklist.h"
#include "chrome/browser/supervised_user/kids_management_url_checker_client.h"
#include "chrome/common/chrome_features.h"
#include "components/policy/core/browser/url_blacklist_manager.h"
#include "components/policy/core/browser/url_util.h"
#include "components/safe_search_api/safe_search/safe_search_url_checker_client.h"
#include "components/url_matcher/url_matcher.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension_urls.h"
#endif

using content::BrowserThread;
using net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES;
using net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES;
using net::registry_controlled_domains::GetCanonicalHostRegistryLength;
using policy::url_util::CreateConditionSet;
using policy::url_util::FilterToComponents;
using url_matcher::URLMatcher;
using url_matcher::URLMatcherConditionSet;

using HostnameHash = SupervisedUserSiteList::HostnameHash;

namespace {

struct HashHostnameHash {
  size_t operator()(const HostnameHash& value) const {
    return value.hash();
  }
};

SupervisedUserURLFilter::FilteringBehavior
GetBehaviorFromSafeSearchClassification(
    safe_search_api::Classification classification) {
  switch (classification) {
    case safe_search_api::Classification::SAFE:
      return SupervisedUserURLFilter::ALLOW;
    case safe_search_api::Classification::UNSAFE:
      return SupervisedUserURLFilter::BLOCK;
  }
  NOTREACHED();
  return SupervisedUserURLFilter::BLOCK;
}

}  // namespace

struct SupervisedUserURLFilter::Contents {
  URLMatcher url_matcher;
  std::unordered_multimap<HostnameHash,
                          scoped_refptr<SupervisedUserSiteList>,
                          HashHostnameHash>
      hostname_hashes;
  // This only tracks pattern lists.
  std::map<URLMatcherConditionSet::ID, scoped_refptr<SupervisedUserSiteList>>
      site_lists_by_matcher_id;
};

namespace {

// URL schemes not in this list (e.g., file:// and chrome://) will always be
// allowed.
const char* const kFilteredSchemes[] = {"http", "https", "ftp", "ws", "wss"};

#if BUILDFLAG(ENABLE_EXTENSIONS)
const char* const kCrxDownloadUrls[] = {
    "https://clients2.googleusercontent.com/crx/blobs/",
    "https://chrome.google.com/webstore/download/"};
#endif

// Whitelisted origins:
const char kFamiliesSecureUrl[] = "https://families.google.com/";
const char kFamiliesUrl[] = "http://families.google.com/";

// Play Store terms of service path:
const char kPlayStoreHost[] = "play.google.com";
const char kPlayTermsPath[] = "/about/play-terms";

// This class encapsulates all the state that is required during construction of
// a new SupervisedUserURLFilter::Contents.
class FilterBuilder {
 public:
  FilterBuilder();
  ~FilterBuilder();

  // Adds a single URL pattern and returns the id of its matcher.
  URLMatcherConditionSet::ID AddPattern(const std::string& pattern);

  // Adds all the sites in |site_list|, with URL patterns and hostname hashes.
  void AddSiteList(const scoped_refptr<SupervisedUserSiteList>& site_list);

  // Finalizes construction of the SupervisedUserURLFilter::Contents and returns
  // them. This method should be called before this object is destroyed.
  std::unique_ptr<SupervisedUserURLFilter::Contents> Build();

 private:
  std::unique_ptr<SupervisedUserURLFilter::Contents> contents_;
  URLMatcherConditionSet::Vector all_conditions_;
  URLMatcherConditionSet::ID matcher_id_;
  std::map<URLMatcherConditionSet::ID, scoped_refptr<SupervisedUserSiteList>>
      site_lists_by_matcher_id_;
};

FilterBuilder::FilterBuilder()
    : contents_(new SupervisedUserURLFilter::Contents()),
      matcher_id_(0) {}

FilterBuilder::~FilterBuilder() {
  DCHECK(!contents_.get());
}

URLMatcherConditionSet::ID FilterBuilder::AddPattern(
    const std::string& pattern) {
  std::string scheme;
  std::string host;
  uint16_t port = 0;
  std::string path;
  std::string query;
  bool match_subdomains = true;
  if (!FilterToComponents(pattern, &scheme, &host, &match_subdomains, &port,
                          &path, &query)) {
    LOG(ERROR) << "Invalid pattern " << pattern;
    return -1;
  }

  scoped_refptr<URLMatcherConditionSet> condition_set =
      CreateConditionSet(&contents_->url_matcher, ++matcher_id_, scheme, host,
                         match_subdomains, port, path, query, true);
  all_conditions_.push_back(std::move(condition_set));
  return matcher_id_;
}

void FilterBuilder::AddSiteList(
    const scoped_refptr<SupervisedUserSiteList>& site_list) {
  for (const std::string& pattern : site_list->patterns()) {
    URLMatcherConditionSet::ID id = AddPattern(pattern);
    if (id >= 0) {
      site_lists_by_matcher_id_[id] = site_list;
    }
  }

  for (const HostnameHash& hash : site_list->hostname_hashes())
    contents_->hostname_hashes.insert(std::make_pair(hash, site_list));
}

std::unique_ptr<SupervisedUserURLFilter::Contents> FilterBuilder::Build() {
  contents_->url_matcher.AddConditionSets(all_conditions_);
  contents_->site_lists_by_matcher_id.insert(site_lists_by_matcher_id_.begin(),
                                             site_lists_by_matcher_id_.end());
  return std::move(contents_);
}

std::unique_ptr<SupervisedUserURLFilter::Contents>
CreateWhitelistFromPatternsForTesting(
    const std::vector<std::string>& patterns) {
  FilterBuilder builder;
  for (const std::string& pattern : patterns)
    builder.AddPattern(pattern);

  return builder.Build();
}

std::unique_ptr<SupervisedUserURLFilter::Contents>
CreateWhitelistsFromSiteListsForTesting(
    const std::vector<scoped_refptr<SupervisedUserSiteList>>& site_lists) {
  FilterBuilder builder;
  for (const scoped_refptr<SupervisedUserSiteList>& site_list : site_lists)
    builder.AddSiteList(site_list);
  return builder.Build();
}

std::unique_ptr<SupervisedUserURLFilter::Contents> LoadWhitelistsAsyncThread(
    const std::vector<scoped_refptr<SupervisedUserSiteList>>& site_lists) {
  FilterBuilder builder;
  for (const scoped_refptr<SupervisedUserSiteList>& site_list : site_lists)
    builder.AddSiteList(site_list);

  return builder.Build();
}

}  // namespace

SupervisedUserURLFilter::SupervisedUserURLFilter()
    : default_behavior_(ALLOW),
      contents_(new Contents()),
      blacklist_(nullptr),
      blocking_task_runner_(base::CreateTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {}

SupervisedUserURLFilter::~SupervisedUserURLFilter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
SupervisedUserURLFilter::FilteringBehavior
SupervisedUserURLFilter::BehaviorFromInt(int behavior_value) {
  DCHECK_GE(behavior_value, ALLOW);
  DCHECK_LE(behavior_value, BLOCK);
  return static_cast<FilteringBehavior>(behavior_value);
}

// static
bool SupervisedUserURLFilter::HasFilteredScheme(const GURL& url) {
  for (const char* scheme : kFilteredSchemes) {
    if (url.scheme() == scheme)
      return true;
  }
  return false;
}

// static
bool SupervisedUserURLFilter::HostMatchesPattern(
    const std::string& canonical_host,
    const std::string& pattern) {
  std::string trimmed_pattern = pattern;
  std::string trimmed_host = canonical_host;
  if (base::EndsWith(pattern, ".*", base::CompareCase::SENSITIVE)) {
    size_t registry_length = GetCanonicalHostRegistryLength(
        trimmed_host, EXCLUDE_UNKNOWN_REGISTRIES, EXCLUDE_PRIVATE_REGISTRIES);
    // A host without a known registry part does not match.
    if (registry_length == 0)
      return false;

    trimmed_pattern.erase(trimmed_pattern.length() - 2);
    trimmed_host.erase(trimmed_host.length() - (registry_length + 1));
  }

  if (base::StartsWith(trimmed_pattern, "*.", base::CompareCase::SENSITIVE)) {
    trimmed_pattern.erase(0, 2);

    // The remaining pattern should be non-empty, and it should not contain
    // further stars. Also the trimmed host needs to end with the trimmed
    // pattern.
    if (trimmed_pattern.empty() ||
        trimmed_pattern.find('*') != std::string::npos ||
        !base::EndsWith(trimmed_host, trimmed_pattern,
                        base::CompareCase::SENSITIVE)) {
      return false;
    }

    // The trimmed host needs to have a dot separating the subdomain from the
    // matched pattern piece, unless there is no subdomain.
    int pos = trimmed_host.length() - trimmed_pattern.length();
    DCHECK_GE(pos, 0);
    return (pos == 0) || (trimmed_host[pos - 1] == '.');
  }

  return trimmed_host == trimmed_pattern;
}

SupervisedUserURLFilter::FilteringBehavior
SupervisedUserURLFilter::GetFilteringBehaviorForURL(const GURL& url) const {
  supervised_user_error_page::FilteringBehaviorReason reason;
  return GetFilteringBehaviorForURL(url, false, &reason);
}

bool SupervisedUserURLFilter::GetManualFilteringBehaviorForURL(
    const GURL& url, FilteringBehavior* behavior) const {
  supervised_user_error_page::FilteringBehaviorReason reason;
  *behavior = GetFilteringBehaviorForURL(url, true, &reason);
  return reason == supervised_user_error_page::MANUAL;
}

SupervisedUserURLFilter::FilteringBehavior
SupervisedUserURLFilter::GetFilteringBehaviorForURL(
    const GURL& url,
    bool manual_only,
    supervised_user_error_page::FilteringBehaviorReason* reason) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GURL effective_url = policy::url_util::GetEmbeddedURL(url);
  if (!effective_url.is_valid())
    effective_url = url;

  *reason = supervised_user_error_page::MANUAL;

  // URLs with a non-standard scheme (e.g. chrome://) are always allowed.
  if (!HasFilteredScheme(effective_url))
    return ALLOW;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Allow webstore crx downloads. This applies to both extension installation
  // and updates.
  if (extension_urls::GetWebstoreUpdateUrl() ==
      policy::url_util::Normalize(effective_url)) {
    return ALLOW;
  }

  // The actual CRX files are downloaded from other URLs. Allow them too.
  for (const char* crx_download_url_str : kCrxDownloadUrls) {
    GURL crx_download_url(crx_download_url_str);
    if (effective_url.SchemeIs(url::kHttpsScheme) &&
        crx_download_url.host_piece() == effective_url.host_piece() &&
        base::StartsWith(effective_url.path_piece(),
                         crx_download_url.path_piece(),
                         base::CompareCase::SENSITIVE)) {
      return ALLOW;
    }
  }
#endif

  // Allow navigations to whitelisted origins (currently families.google.com).
  static const base::NoDestructor<base::flat_set<GURL>> kWhitelistedOrigins(
      base::flat_set<GURL>({GURL(kFamiliesUrl).GetOrigin(),
                            GURL(kFamiliesSecureUrl).GetOrigin()}));
  if (base::Contains(*kWhitelistedOrigins, effective_url.GetOrigin()))
    return ALLOW;

  // Check Play Store terms of service.
  // path_piece is checked separetly from the host to match international pages
  // like https://play.google.com/intl/pt-BR_pt/about/play-terms/.
  if (effective_url.SchemeIs(url::kHttpsScheme) &&
      effective_url.host_piece() == kPlayStoreHost &&
      effective_url.path_piece().find(kPlayTermsPath) !=
          base::StringPiece::npos) {
    return ALLOW;
  }

  // Check manual blacklists and whitelists.
  FilteringBehavior manual_result =
      GetManualFilteringBehaviorForURL(effective_url);
  if (manual_result != INVALID)
    return manual_result;

  // Check the list of URL patterns.
  std::set<URLMatcherConditionSet::ID> matching_ids =
      contents_->url_matcher.MatchURL(effective_url);

  if (!matching_ids.empty()) {
    *reason = supervised_user_error_page::WHITELIST;
    return ALLOW;
  }

  // Check the list of hostname hashes.
  if (contents_->hostname_hashes.count(HostnameHash(effective_url.host()))) {
    *reason = supervised_user_error_page::WHITELIST;
    return ALLOW;
  }

  // Check the static blacklist, unless the default is to block anyway.
  if (!manual_only && default_behavior_ != BLOCK && blacklist_ &&
      blacklist_->HasURL(effective_url)) {
    *reason = supervised_user_error_page::BLACKLIST;
    return BLOCK;
  }

  // Fall back to the default behavior.
  *reason = supervised_user_error_page::DEFAULT;
  return default_behavior_;
}

// There may be conflicting patterns, say, "allow *.google.com" and "block
// www.google.*". To break the tie, we prefer blacklists over whitelists, by
// returning early if there is a BLOCK and evaluating all manual overrides
// before returning an ALLOW. If there are no applicable manual overrides,
// return INVALID.
SupervisedUserURLFilter::FilteringBehavior
SupervisedUserURLFilter::GetManualFilteringBehaviorForURL(
    const GURL& url) const {
  FilteringBehavior result = INVALID;

  // Check manual overrides for the exact URL.
  auto url_it = url_map_.find(policy::url_util::Normalize(url));
  if (url_it != url_map_.end()) {
    if (!url_it->second)
      return BLOCK;
    result = ALLOW;
  }

  // Check manual overrides for the hostname.
  const std::string host = url.host();
  auto host_it = host_map_.find(host);
  if (host_it != host_map_.end()) {
    if (!host_it->second)
      return BLOCK;
    result = ALLOW;
  }

  // Look for patterns matching the hostname, with a value that is different
  // from the default (a value of true in the map meaning allowed).
  for (const auto& host_entry : host_map_) {
    if (HostMatchesPattern(host, host_entry.first)) {
      if (!host_entry.second)
        return BLOCK;
      result = ALLOW;
    }
  }

  return result;
}

bool SupervisedUserURLFilter::GetFilteringBehaviorForURLWithAsyncChecks(
    const GURL& url,
    FilteringBehaviorCallback callback) const {
  supervised_user_error_page::FilteringBehaviorReason reason =
      supervised_user_error_page::DEFAULT;
  FilteringBehavior behavior = GetFilteringBehaviorForURL(url, false, &reason);
  // Any non-default reason trumps the async checker.
  // Also, if we're blocking anyway, then there's no need to check it.
  if (reason != supervised_user_error_page::DEFAULT || behavior == BLOCK ||
      !async_url_checker_) {
    std::move(callback).Run(behavior, reason, false);
    for (Observer& observer : observers_)
      observer.OnURLChecked(url, behavior, reason, false);
    return true;
  }

  return async_url_checker_->CheckURL(
      policy::url_util::Normalize(url),
      base::BindOnce(&SupervisedUserURLFilter::CheckCallback,
                     base::Unretained(this), std::move(callback)));
}

std::map<std::string, base::string16>
SupervisedUserURLFilter::GetMatchingWhitelistTitles(const GURL& url) const {
  std::map<std::string, base::string16> whitelists;

  std::set<URLMatcherConditionSet::ID> matching_ids =
      contents_->url_matcher.MatchURL(url);

  for (const auto& matching_id : matching_ids) {
    const scoped_refptr<SupervisedUserSiteList>& site_list =
        contents_->site_lists_by_matcher_id[matching_id];
    whitelists[site_list->id()] = site_list->title();
  }

  // Add the site lists that match the URL hostname hash to the map of
  // whitelists (IDs -> titles).
  const auto& range =
      contents_->hostname_hashes.equal_range(HostnameHash(url.host()));
  for (auto it = range.first; it != range.second; ++it)
    whitelists[it->second->id()] = it->second->title();

  return whitelists;
}

void SupervisedUserURLFilter::SetDefaultFilteringBehavior(
    FilteringBehavior behavior) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  default_behavior_ = behavior;
}

SupervisedUserURLFilter::FilteringBehavior
SupervisedUserURLFilter::GetDefaultFilteringBehavior() const {
  return default_behavior_;
}

void SupervisedUserURLFilter::LoadWhitelists(
    const std::vector<scoped_refptr<SupervisedUserSiteList>>& site_lists) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::Bind(&LoadWhitelistsAsyncThread, site_lists),
      base::Bind(&SupervisedUserURLFilter::SetContents,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SupervisedUserURLFilter::SetBlacklist(
    const SupervisedUserBlacklist* blacklist) {
  blacklist_ = blacklist;
}

bool SupervisedUserURLFilter::HasBlacklist() const {
  return !!blacklist_;
}

void SupervisedUserURLFilter::SetFromPatternsForTesting(
    const std::vector<std::string>& patterns) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::Bind(&CreateWhitelistFromPatternsForTesting, patterns),
      base::Bind(&SupervisedUserURLFilter::SetContents,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SupervisedUserURLFilter::SetFromSiteListsForTesting(
    const std::vector<scoped_refptr<SupervisedUserSiteList>>& site_lists) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::Bind(&CreateWhitelistsFromSiteListsForTesting, site_lists),
      base::Bind(&SupervisedUserURLFilter::SetContents,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SupervisedUserURLFilter::SetManualHosts(
    std::map<std::string, bool> host_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_map_ = std::move(host_map);
}

void SupervisedUserURLFilter::SetManualURLs(std::map<GURL, bool> url_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url_map_ = std::move(url_map);
}

void SupervisedUserURLFilter::InitAsyncURLChecker(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  std::string country;
  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  if (variations_service) {
    country = variations_service->GetStoredPermanentCountry();
    if (country.empty())
      country = variations_service->GetLatestCountry();
  }

  std::unique_ptr<safe_search_api::URLCheckerClient> url_checker_client;

  if ((base::FeatureList::IsEnabled(
          features::kKidsManagementUrlClassification))) {
    url_checker_client =
        std::make_unique<KidsManagementURLCheckerClient>(country);
  } else {
    // TODO(crbug.com/940454): remove safe_search_checker
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("supervised_user_url_filter", R"(
        semantics {
          sender: "Supervised Users"
          description:
            "Checks whether a given URL (or set of URLs) is considered safe by "
            "Google SafeSearch."
          trigger:
            "If the parent enabled this feature for the child account, this is "
            "sent for every navigation."
          data: "URL(s) to be checked."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature is only used in child accounts and cannot be "
            "disabled by settings. Parent accounts can disable it in the "
            "family dashboard."
          policy_exception_justification: "Not implemented."
        })");
    url_checker_client =
        std::make_unique<safe_search_api::SafeSearchURLCheckerClient>(
            std::move(url_loader_factory), traffic_annotation, country);
  }

  async_url_checker_ = std::make_unique<safe_search_api::URLChecker>(
      std::move(url_checker_client));
}

void SupervisedUserURLFilter::ClearAsyncURLChecker() {
  async_url_checker_.reset();
}

bool SupervisedUserURLFilter::HasAsyncURLChecker() const {
  return !!async_url_checker_;
}

void SupervisedUserURLFilter::Clear() {
  default_behavior_ = ALLOW;
  SetContents(std::make_unique<Contents>());
  url_map_.clear();
  host_map_.clear();
  blacklist_ = nullptr;
  async_url_checker_.reset();
}

void SupervisedUserURLFilter::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SupervisedUserURLFilter::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SupervisedUserURLFilter::SetBlockingTaskRunnerForTesting(
    const scoped_refptr<base::TaskRunner>& task_runner) {
  blocking_task_runner_ = task_runner;
}

void SupervisedUserURLFilter::SetContents(std::unique_ptr<Contents> contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  contents_ = std::move(contents);
  for (Observer& observer : observers_)
    observer.OnSiteListUpdated();
}

void SupervisedUserURLFilter::CheckCallback(
    FilteringBehaviorCallback callback,
    const GURL& url,
    safe_search_api::Classification classification,
    bool uncertain) const {
  DCHECK(default_behavior_ != BLOCK);

  FilteringBehavior behavior =
      GetBehaviorFromSafeSearchClassification(classification);

  std::move(callback).Run(behavior, supervised_user_error_page::ASYNC_CHECKER,
                          uncertain);
  for (Observer& observer : observers_) {
    observer.OnURLChecked(url, behavior,
                          supervised_user_error_page::ASYNC_CHECKER, uncertain);
  }
}
