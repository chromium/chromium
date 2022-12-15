// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_url_filter.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/supervised_user/kids_management_url_checker_client.h"
#include "chrome/browser/supervised_user/supervised_user_denylist.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/url_matcher/url_util.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension_urls.h"
#endif

using net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES;
using net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES;
using net::registry_controlled_domains::GetCanonicalHostRegistryLength;

namespace {

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

bool IsSameDomain(const GURL& url1, const GURL& url2) {
  return net::registry_controlled_domains::SameDomainOrHost(
      url1, url2, EXCLUDE_PRIVATE_REGISTRIES);
}

bool SetFilteringBehaviorResult(
    SupervisedUserURLFilter::FilteringBehavior behavior,
    SupervisedUserURLFilter::FilteringBehavior* result) {
  if (*result == behavior)
    return false;

  // First time to find a match in allow/block list
  if (*result == SupervisedUserURLFilter::FilteringBehavior::INVALID) {
    *result = behavior;
    return false;
  }

  // Another match is found and doesn't have the same behavior. Block is
  // the preferred behvior, override the result only if the match is block.
  if (behavior == SupervisedUserURLFilter::FilteringBehavior::BLOCK)
    *result = behavior;

  return true;
}

bool IsNonStandardUrlScheme(const GURL& effective_url) {
  // URLs with a non-standard scheme (e.g. chrome://) are always allowed.
  return !effective_url.SchemeIsHTTPOrHTTPS() &&
         !effective_url.SchemeIsWSOrWSS() &&
         !effective_url.SchemeIs(url::kFtpScheme);
}

bool IsAlwaysAllowedHost(const GURL& effective_url) {
  // Allow navigations to allowed origins.
  constexpr auto kAllowedHosts = base::MakeFixedFlatSet<base::StringPiece>(
      {"accounts.google.com", "families.google.com", "familylink.google.com",
       "myaccount.google.com", "policies.google.com", "support.google.com"});

  return base::Contains(kAllowedHosts, effective_url.host_piece());
}

bool IsAlwaysAllowedUrlPrefix(const GURL& effective_url) {
  // A list of allowed URL prefixes.
  //
  // Consider using url_matcher::CreateURLPrefixCondition (initialized once at
  // startup) for performance if the set of allowed URL prefixes grows large.
  static const char* const kAllowedUrlPrefixes[] = {
      // The Chrome sync dashboard is linked to from within Chrome settings.
      // Allow both the initial URL that is loaded, and the URL to which it
      // redirects.
      chrome::kSyncGoogleDashboardURL, "https://chrome.google.com/sync"};

  for (const char* allowedUrlPrefix : kAllowedUrlPrefixes) {
    if (base::StartsWith(effective_url.spec(), allowedUrlPrefix))
      return true;
  }
  return false;
}

bool IsPlayStoreTermsOfServiceUrl(const GURL& effective_url) {
  // Play Store terms of service path:
  static const char* kPlayStoreHost = "play.google.com";
  static const char* kPlayTermsPath = "/about/play-terms";
  // Check Play Store terms of service.
  // path_piece is checked separately from the host to match international pages
  // like https://play.google.com/intl/pt-BR_pt/about/play-terms/.
  return effective_url.SchemeIs(url::kHttpsScheme) &&
         effective_url.host_piece() == kPlayStoreHost &&
         (effective_url.path_piece().find(kPlayTermsPath) !=
          base::StringPiece::npos);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
bool IsCrxWebstoreOrDownloadUrl(const GURL& effective_url) {
  static const char* const kCrxDownloadUrls[] = {
      "https://clients2.googleusercontent.com/crx/blobs/",
      "https://chrome.google.com/webstore/download/"};

  // Chrome Webstore.
  if (extension_urls::IsWebstoreDomain(
          url_matcher::util::Normalize(effective_url))) {
    return true;
  }

  // Allow webstore crx downloads. This applies to both extension installation
  // and updates.
  if (extension_urls::GetWebstoreUpdateUrl() ==
      url_matcher::util::Normalize(effective_url)) {
    return true;
  }

  // The actual CRX files are downloaded from other URLs. Allow them too.
  // These URLs have https scheme.
  if (!effective_url.SchemeIs(url::kHttpsScheme))
    return false;

  for (const char* crx_download_url_str : kCrxDownloadUrls) {
    GURL crx_download_url(crx_download_url_str);
    if (crx_download_url.host_piece() == effective_url.host_piece() &&
        base::StartsWith(effective_url.path_piece(),
                         crx_download_url.path_piece(),
                         base::CompareCase::SENSITIVE)) {
      return true;
    }
  }
  return false;
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace

namespace {

// UMA histogram FamilyUser.ManagedSiteList.Conflict
// Reports conflict when the user tries to access a url that has a match in
// both of the allow list and the block list.
const char kManagedSiteListConflictHistogramName[] =
    "FamilyUser.ManagedSiteList.Conflict";

// UMA histogram FamilyUser.WebFilterType
// Reports WebFilterType which indicates web filter behaviour are used for
// current Family Link user.
constexpr char kWebFilterTypeHistogramName[] = "FamilyUser.WebFilterType";

// UMA histogram FamilyUser.ManualSiteListType
// Reports ManualSiteListType which indicates approved list and blocked list
// usage for current Family Link user.
constexpr char kManagedSiteListHistogramName[] = "FamilyUser.ManagedSiteList";

// UMA histogram FamilyUser.ManagedSiteListCount.Approved
// Reports the number of approved urls and domains for current Family Link user.
constexpr char kApprovedSitesCountHistogramName[] =
    "FamilyUser.ManagedSiteListCount.Approved";

// UMA histogram FamilyUser.ManagedSiteListCount.Blocked
// Reports the number of blocked urls and domains for current Family Link user.
constexpr char kBlockedSitesCountHistogramName[] =
    "FamilyUser.ManagedSiteListCount.Blocked";
}  // namespace

SupervisedUserURLFilter::SupervisedUserURLFilter()
    : default_behavior_(ALLOW),
      denylist_(nullptr),
      blocking_task_runner_(base::ThreadPool::CreateTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {}

SupervisedUserURLFilter::~SupervisedUserURLFilter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
const char* SupervisedUserURLFilter::GetWebFilterTypeHistogramNameForTest() {
  return kWebFilterTypeHistogramName;
}

// static
const char* SupervisedUserURLFilter::GetManagedSiteListHistogramNameForTest() {
  return kManagedSiteListHistogramName;
}

// static
const char*
SupervisedUserURLFilter::GetApprovedSitesCountHistogramNameForTest() {
  return kApprovedSitesCountHistogramName;
}

// static
const char*
SupervisedUserURLFilter::GetBlockedSitesCountHistogramNameForTest() {
  return kBlockedSitesCountHistogramName;
}

// static
const char*
SupervisedUserURLFilter::GetManagedSiteListConflictHistogramNameForTest() {
  return kManagedSiteListConflictHistogramName;
}

// static
bool SupervisedUserURLFilter::ShouldSkipParentManualAllowlistFiltering(
    content::WebContents* contents) {
  // Note that |contents| can be an inner WebContents. Get the outer most
  // WebContents and check if it belongs to the EDUCoexistence login flow.
  content::WebContents* outer_most_content =
      contents->GetOutermostWebContents();

  return outer_most_content->GetLastCommittedURL() ==
         GURL(SupervisedUserService::GetEduCoexistenceLoginUrl());
}

// static
SupervisedUserURLFilter::FilteringBehavior
SupervisedUserURLFilter::BehaviorFromInt(int behavior_value) {
  DCHECK(behavior_value == ALLOW || behavior_value == BLOCK)
      << "SupervisedUserURLFilter value not supported: " << behavior_value;
  return static_cast<FilteringBehavior>(behavior_value);
}

// static
bool SupervisedUserURLFilter::HostMatchesPattern(
    const std::string& canonical_host,
    const std::string& pattern) {
  std::string trimmed_pattern = pattern;
  std::string trimmed_host = canonical_host;

  // If pattern starts with https:// or http:// trim it.
  if (base::StartsWith(pattern, "https://", base::CompareCase::SENSITIVE)) {
    trimmed_pattern = trimmed_pattern.substr(8);
  } else if (base::StartsWith(pattern, "http://",
                              base::CompareCase::SENSITIVE)) {
    trimmed_pattern = trimmed_pattern.substr(7);
  }

  bool host_starts_with_www =
      base::StartsWith(canonical_host, "www.", base::CompareCase::SENSITIVE);
  bool pattern_starts_with_www =
      base::StartsWith(trimmed_pattern, "www.", base::CompareCase::SENSITIVE);

  // Trim the initial "www." if it appears on either the host or the pattern,
  // but not if it appears on both.
  if (host_starts_with_www != pattern_starts_with_www) {
    if (host_starts_with_www) {
      trimmed_host = trimmed_host.substr(4);
    } else if (pattern_starts_with_www) {
      trimmed_pattern = trimmed_pattern.substr(4);
    }
  }

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

// Static.
std::string SupervisedUserURLFilter::WebFilterTypeToDisplayString(
    WebFilterType web_filter_type) {
  switch (web_filter_type) {
    case WebFilterType::kAllowAllSites:
      return "allow_all_sites";
    case WebFilterType::kCertainSites:
      return "allow_certain_sites";
    case WebFilterType::kTryToBlockMatureSites:
      return "block_mature_sites";
  }
}

SupervisedUserURLFilter::FilteringBehavior
SupervisedUserURLFilter::GetFilteringBehaviorForURL(const GURL& url) const {
  supervised_user_error_page::FilteringBehaviorReason reason;
  return GetFilteringBehaviorForURL(url, false, &reason);
}

bool SupervisedUserURLFilter::IsExemptedFromGuardianApproval(
    const GURL& effective_url) const {
  bool exempted_from_guardian_approval =
      IsNonStandardUrlScheme(effective_url) ||
      IsAlwaysAllowedHost(effective_url) ||
      IsAlwaysAllowedUrlPrefix(effective_url) ||
      IsPlayStoreTermsOfServiceUrl(effective_url);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  exempted_from_guardian_approval |= IsCrxWebstoreOrDownloadUrl(effective_url);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  return exempted_from_guardian_approval;
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

  GURL effective_url = url_matcher::util::GetEmbeddedURL(url);
  if (!effective_url.is_valid())
    effective_url = url;

  *reason = supervised_user_error_page::MANUAL;

  if (IsExemptedFromGuardianApproval(effective_url))
    return ALLOW;

  // Check manual denylists and allowlists.
  FilteringBehavior manual_result =
      GetManualFilteringBehaviorForURL(effective_url);
  if (manual_result != INVALID)
    return manual_result;

  // Check the static denylist, unless the default is to block anyway.
  if (!manual_only && default_behavior_ != BLOCK && denylist_ &&
      denylist_->HasURL(effective_url)) {
    *reason = supervised_user_error_page::DENYLIST;
    return BLOCK;
  }

  // Fall back to the default behavior.
  *reason = supervised_user_error_page::DEFAULT;
  return default_behavior_;
}

// There may be conflicting patterns, say, "allow *.google.com" and "block
// www.google.*". To break the tie, we prefer denylists over allowlists, by
// returning early if there is a BLOCK and evaluating all manual overrides
// before returning an ALLOW. If there are no applicable manual overrides,
// return INVALID.
SupervisedUserURLFilter::FilteringBehavior
SupervisedUserURLFilter::GetManualFilteringBehaviorForURL(
    const GURL& url) const {
  FilteringBehavior result = INVALID;
  bool conflict = false;

  // Check manual overrides for the exact URL.
  auto url_it = url_map_.find(url_matcher::util::Normalize(url));
  if (url_it != url_map_.end()) {
    conflict =
        SetFilteringBehaviorResult(url_it->second ? ALLOW : BLOCK, &result);
  }

  // Check manual overrides for the hostname.
  const std::string host = url.host();
  auto host_it = host_map_.find(host);
  if (host_it != host_map_.end()) {
    conflict |=
        SetFilteringBehaviorResult(host_it->second ? ALLOW : BLOCK, &result);
  }

  // Look for patterns matching the hostname, with a value that is different
  // from the default (a value of true in the map meaning allowed).
  for (const auto& host_entry : host_map_) {
    if (HostMatchesPattern(host, host_entry.first)) {
      conflict |= SetFilteringBehaviorResult(host_entry.second ? ALLOW : BLOCK,
                                             &result);
    }
  }

  if (result != INVALID)
    UMA_HISTOGRAM_BOOLEAN(kManagedSiteListConflictHistogramName, conflict);

  return result;
}

bool SupervisedUserURLFilter::GetFilteringBehaviorForURLWithAsyncChecks(
    const GURL& url,
    FilteringBehaviorCallback callback,
    bool skip_manual_parent_filter) const {
  supervised_user_error_page::FilteringBehaviorReason reason =
      supervised_user_error_page::DEFAULT;
  FilteringBehavior behavior = GetFilteringBehaviorForURL(url, false, &reason);

  if (behavior == ALLOW && reason != supervised_user_error_page::DEFAULT) {
    std::move(callback).Run(behavior, reason, false);
    for (Observer& observer : observers_)
      observer.OnURLChecked(url, behavior, reason, false);
    return true;
  }

  if (!skip_manual_parent_filter) {
    // Any non-default reason trumps the async checker.
    // Also, if we're blocking anyway, then there's no need to check it.
    if (reason != supervised_user_error_page::DEFAULT || behavior == BLOCK ||
        !async_url_checker_) {
      std::move(callback).Run(behavior, reason, false);
      for (Observer& observer : observers_)
        observer.OnURLChecked(url, behavior, reason, false);
      return true;
    }
  }

  // Runs mature url filter if the |async_url_checker_| exists.
  return RunAsyncChecker(url, std::move(callback));
}

bool SupervisedUserURLFilter::GetFilteringBehaviorForSubFrameURLWithAsyncChecks(
    const GURL& url,
    const GURL& main_frame_url,
    FilteringBehaviorCallback callback) const {
  supervised_user_error_page::FilteringBehaviorReason reason =
      supervised_user_error_page::DEFAULT;
  FilteringBehavior behavior = GetFilteringBehaviorForURL(url, false, &reason);

  // If the reason is not default, then it is manually allowed or blocked.
  if (reason != supervised_user_error_page::DEFAULT) {
    std::move(callback).Run(behavior, reason, false);
    for (Observer& observer : observers_)
      observer.OnURLChecked(url, behavior, reason, false);
    return true;
  }

  // If the reason is default and behavior is block and the subframe url is not
  // the same domain as the main frame, block the subframe.
  if (behavior == FilteringBehavior::BLOCK &&
      !IsSameDomain(url, main_frame_url)) {
    // It is not in the same domain and is blocked.
    std::move(callback).Run(behavior, reason, false);
    for (Observer& observer : observers_)
      observer.OnURLChecked(url, behavior, reason, false);
    return true;
  }

  // Runs mature url filter if the |async_url_checker_| exists.
  return RunAsyncChecker(url, std::move(callback));
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

void SupervisedUserURLFilter::SetDenylist(
    const SupervisedUserDenylist* denylist) {
  denylist_ = denylist;
}

bool SupervisedUserURLFilter::HasDenylist() const {
  return !!denylist_;
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

  std::unique_ptr<safe_search_api::URLCheckerClient> url_checker_client =
      std::make_unique<KidsManagementURLCheckerClient>(country);
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
  url_map_.clear();
  host_map_.clear();
  denylist_ = nullptr;
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

SupervisedUserURLFilter::WebFilterType
SupervisedUserURLFilter::GetWebFilterType() const {
  // If the default filtering behavior is not block, it means the web filter
  // was set to either "allow all sites" or "try to block mature sites".
  if (default_behavior_ == BLOCK)
    return WebFilterType::kCertainSites;

  bool safe_sites_enabled = HasAsyncURLChecker() || HasDenylist();
  return safe_sites_enabled ? WebFilterType::kTryToBlockMatureSites
                            : WebFilterType::kAllowAllSites;
}

void SupervisedUserURLFilter::ReportWebFilterTypeMetrics() const {
  if (!is_filter_initialized_)
    return;

  base::UmaHistogramEnumeration(kWebFilterTypeHistogramName,
                                GetWebFilterType());
}

void SupervisedUserURLFilter::ReportManagedSiteListMetrics() const {
  if (!is_filter_initialized_)
    return;

  if (url_map_.empty() && host_map_.empty()) {
    base::UmaHistogramEnumeration(kManagedSiteListHistogramName,
                                  ManagedSiteList::kEmpty);
    base::UmaHistogramCounts1000(kApprovedSitesCountHistogramName, 0);
    base::UmaHistogramCounts1000(kBlockedSitesCountHistogramName, 0);
    return;
  }

  ManagedSiteList managed_site_list = ManagedSiteList::kMaxValue;
  int approved_count = 0;
  int blocked_count = 0;
  for (const auto& it : url_map_) {
    if (it.second) {
      approved_count++;
    } else {
      blocked_count++;
    }
  }

  for (const auto& it : host_map_) {
    if (it.second) {
      approved_count++;
    } else {
      blocked_count++;
    }
  }

  if (approved_count > 0 && blocked_count > 0) {
    managed_site_list = ManagedSiteList::kBoth;
  } else if (approved_count > 0) {
    managed_site_list = ManagedSiteList::kApprovedListOnly;
  } else {
    managed_site_list = ManagedSiteList::kBlockedListOnly;
  }

  base::UmaHistogramCounts1000(kApprovedSitesCountHistogramName,
                               approved_count);
  base::UmaHistogramCounts1000(kBlockedSitesCountHistogramName, blocked_count);

  base::UmaHistogramEnumeration(kManagedSiteListHistogramName,
                                managed_site_list);
}

void SupervisedUserURLFilter::SetFilterInitialized(bool is_filter_initialized) {
  is_filter_initialized_ = is_filter_initialized;
}

bool SupervisedUserURLFilter::RunAsyncChecker(
    const GURL& url,
    FilteringBehaviorCallback callback) const {
  // The parental setting may allow all sites to be visited. In such case, the
  // |async_url_checker_| will not be created.
  if (!async_url_checker_) {
    std::move(callback).Run(FilteringBehavior::ALLOW,
                            supervised_user_error_page::DEFAULT, false);
    return true;
  }

  return async_url_checker_->CheckURL(
      url_matcher::util::Normalize(url),
      base::BindOnce(&SupervisedUserURLFilter::CheckCallback,
                     base::Unretained(this), std::move(callback)));
}

void SupervisedUserURLFilter::CheckCallback(
    FilteringBehaviorCallback callback,
    const GURL& url,
    safe_search_api::Classification classification,
    bool uncertain) const {
  FilteringBehavior behavior =
      GetBehaviorFromSafeSearchClassification(classification);
  std::move(callback).Run(behavior, supervised_user_error_page::ASYNC_CHECKER,
                          uncertain);
  for (Observer& observer : observers_) {
    observer.OnURLChecked(url, behavior,
                          supervised_user_error_page::ASYNC_CHECKER, uncertain);
  }
}
