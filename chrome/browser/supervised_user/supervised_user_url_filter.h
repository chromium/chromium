// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_URL_FILTER_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_URL_FILTER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/supervised_user/supervised_user_error_page/supervised_user_error_page.h"
#include "chrome/browser/supervised_user/supervised_user_site_list.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "components/safe_search_api/url_checker.h"

class GURL;
class SupervisedUserDenylist;

namespace base {
class TaskRunner;
}

namespace content {
class WebContents;
}  // namespace content

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

// This class manages the filtering behavior for URLs, i.e. it tells callers
// if a URL should be allowed, blocked or warned about. It uses information
// from multiple sources:
//   * A default setting (allow, block or warn).
//   * The set of installed and enabled allowlists which contain URL patterns
//     and hostname hashes that should be allowed.
//   * User-specified manual overrides (allow or block) for either sites
//     (hostnames) or exact URLs, which take precedence over the previous
//     sources.
class SupervisedUserURLFilter {
 public:
  // TODO(crbug/1152622): Investigate whether FilteringBehavior::WARN is in
  // use. If it is not in use, remove it.
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.superviseduser
  enum FilteringBehavior {
    ALLOW,
    WARN,
    BLOCK,
    INVALID
  };

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // This enum describes the filter types of Chrome on Chrome OS, which is
  // set by Family Link App or at families.google.com/families. These values
  // are logged to UMA. Entries should not be renumbered and numeric values
  // should never be reused. Please keep in sync with "FamilyLinkWebFilterType"
  // in src/tools/metrics/histograms/enums.xml.
  enum class WebFilterType {
    // The web filter is set to "Allow all sites".
    kAllowAllSites = 0,

    // The web filter is set to "Try to block mature sites".
    kTryToBlockMatureSites = 1,

    // The web filter is set to "Only allow certain sites".
    kCertainSites = 2,

    // Used for UMA. Update kMaxValue to the last value. Add future entries
    // above this comment. Sync with enums.xml.
    kMaxValue = kCertainSites,
  };

  // This enum describes whether the approved list or blocked list is used on
  // Chrome on Chrome OS, which is set by Family Link App or at
  // families.google.com/families via "manage sites" setting. This is also
  // referred to as manual behavior/filter as parent need to add everything one
  // by one. These values are logged to UMA. Entries should not be renumbered
  // and numeric values should never be reused. Please keep in sync with
  // "FamilyLinkManagedSiteList" in src/tools/metrics/histograms/enums.xml.
  enum class ManagedSiteList {
    // The web filter has both empty blocked and approved list.
    kEmpty = 0,

    // The web filter has approved list only.
    kApprovedListOnly = 1,

    // The web filter has blocked list only.
    kBlockedListOnly = 2,

    // The web filter has both approved list and blocked list.
    kBoth = 3,

    // Used for UMA. Update kMaxValue to the last value. Add future entries
    // above this comment. Sync with enums.xml.
    kMaxValue = kBoth,
  };
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  using FilteringBehaviorCallback = base::OnceCallback<void(
      FilteringBehavior,
      supervised_user_error_page::FilteringBehaviorReason,
      bool /* uncertain */)>;

  class Observer {
   public:
    // Called whenever the allowlists are updated. This does *not* include
    // SetManualHosts/SetManualURLs.
    virtual void OnSiteListUpdated() = 0;
    // Called whenever a check started via
    // GetFilteringBehaviorForURLWithAsyncChecks completes.
    virtual void OnURLChecked(
        const GURL& url,
        FilteringBehavior behavior,
        supervised_user_error_page::FilteringBehaviorReason reason,
        bool uncertain) {}
  };

  struct Contents;

  SupervisedUserURLFilter();
  ~SupervisedUserURLFilter();

  // Returns true if the parental allowlist/blocklist should be skipped in
  // |contents|. SafeSearch filtering is still applied to |contents|.
  static bool ShouldSkipParentManualAllowlistFiltering(
      content::WebContents* contents);

  static FilteringBehavior BehaviorFromInt(int behavior_value);

  static bool ReasonIsAutomatic(
      supervised_user_error_page::FilteringBehaviorReason reason);

  // Returns true if the URL has a standard scheme. Only URLs with standard
  // schemes are filtered.
  // This method is public for testing.
  static bool HasFilteredScheme(const GURL& url);

  // Returns true if the |host| matches the pattern. A pattern is a hostname
  // with one or both of the following modifications:
  // - If the pattern starts with "*.", it matches the host or any subdomain
  //   (e.g. the pattern "*.google.com" would match google.com, www.google.com,
  //   or accounts.google.com).
  // - If the pattern ends with ".*", it matches the host on any known TLD
  //   (e.g. the pattern "google.*" would match google.com or google.co.uk).
  // If the |host| starts with "www." but the |pattern| starts with neither
  // "www." nor "*.", the function strips the "www." part of |host| and tries to
  // match again. See the SupervisedUserURLFilterTest.HostMatchesPattern unit
  // test for more examples. Asterisks in other parts of the pattern are not
  // allowed. |host| and |pattern| are assumed to be normalized to lower-case.
  // This method is public for testing.
  static bool HostMatchesPattern(const std::string& canonical_host,
                                 const std::string& pattern);

  // Returns the filtering behavior for a given URL, based on the default
  // behavior and whether it is on a site list.
  FilteringBehavior GetFilteringBehaviorForURL(const GURL& url) const;

  // Checks for a manual setting (i.e. manual exceptions and content packs)
  // for the given URL. If there is one, returns true and writes the result
  // into |behavior|. Otherwise returns false; in this case the value of
  // |behavior| is unspecified.
  bool GetManualFilteringBehaviorForURL(const GURL& url,
                                        FilteringBehavior* behavior) const;

  // Like |GetFilteringBehaviorForURL|, but also includes asynchronous checks
  // against a remote service. If the result is already determined by the
  // synchronous checks, then |callback| will be called synchronously.
  // Returns true if |callback| was called synchronously. If
  // |skip_manual_parent_filter| is set to true, it only uses the asynchronous
  // safe search checks.
  bool GetFilteringBehaviorForURLWithAsyncChecks(
      const GURL& url,
      FilteringBehaviorCallback callback,
      bool skip_manual_parent_filter = false) const;

  // Like |GetFilteringBehaviorForURLWithAsyncChecks| but used for subframes.
  bool GetFilteringBehaviorForSubFrameURLWithAsyncChecks(
      const GURL& url,
      const GURL& main_frame_url,
      FilteringBehaviorCallback callback) const;

  // Gets all the allowlists that the url is part of. Returns id->name of each
  // allowlist.
  std::map<std::string, std::u16string> GetMatchingAllowlistTitles(
      const GURL& url) const;

  // Sets the filtering behavior for pages not on a list (default is ALLOW).
  void SetDefaultFilteringBehavior(FilteringBehavior behavior);

  FilteringBehavior GetDefaultFilteringBehavior() const;

  // Asynchronously loads the specified site lists and updates the
  // filter to recognize each site on them.
  void LoadAllowlists(
      const std::vector<scoped_refptr<SupervisedUserSiteList>>& site_lists);

  // Sets the static denylist of blocked hosts.
  void SetDenylist(const SupervisedUserDenylist* denylist);
  // Returns whether the static denylist is set up.
  bool HasDenylist() const;

  // Set the list of matched patterns to the passed in list, for testing.
  void SetFromPatternsForTesting(const std::vector<std::string>& patterns);

  // Sets the site lists to the passed list, for testing.
  void SetFromSiteListsForTesting(
      const std::vector<scoped_refptr<SupervisedUserSiteList>>& site_lists);

  // Sets the set of manually allowed or blocked hosts.
  void SetManualHosts(std::map<std::string, bool> host_map);

  // Sets the set of manually allowed or blocked URLs.
  void SetManualURLs(std::map<GURL, bool> url_map);

  // Initializes the experimental asynchronous checker.
  void InitAsyncURLChecker(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Clears any asynchronous checker.
  void ClearAsyncURLChecker();

  // Returns whether the asynchronous checker is set up.
  bool HasAsyncURLChecker() const;

  // Removes all filter entries, clears the denylist and async checker if
  // present, and resets the default behavior to "allow".
  void Clear();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Sets a different task runner for testing.
  void SetBlockingTaskRunnerForTesting(
      const scoped_refptr<base::TaskRunner>& task_runner);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // // Returns WebFilterType for current web filter setting.
  WebFilterType GetWebFilterType() const;
  // Returns ManagedSiteList for current web filter setting.
  ManagedSiteList GetManagedSiteList() const;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 private:
  friend class SupervisedUserURLFilterTest;

  bool RunAsyncChecker(const GURL& url,
                       FilteringBehaviorCallback callback) const;

  void SetContents(std::unique_ptr<Contents> url_matcher);

  FilteringBehavior GetFilteringBehaviorForURL(
      const GURL& url,
      bool manual_only,
      supervised_user_error_page::FilteringBehaviorReason* reason) const;
  FilteringBehavior GetManualFilteringBehaviorForURL(const GURL& url) const;

  void CheckCallback(FilteringBehaviorCallback callback,
                     const GURL& url,
                     safe_search_api::Classification classification,
                     bool uncertain) const;

  base::ObserverList<Observer>::Unchecked observers_;

  FilteringBehavior default_behavior_;
  std::unique_ptr<Contents> contents_;

  // Maps from a URL to whether it is manually allowed (true) or blocked
  // (false).
  std::map<GURL, bool> url_map_;

  // Maps from a hostname to whether it is manually allowed (true) or blocked
  // (false).
  std::map<std::string, bool> host_map_;

  // Not owned.
  const SupervisedUserDenylist* denylist_;

  std::unique_ptr<safe_search_api::URLChecker> async_url_checker_;

  scoped_refptr<base::TaskRunner> blocking_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SupervisedUserURLFilter> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserURLFilter);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_URL_FILTER_H_
