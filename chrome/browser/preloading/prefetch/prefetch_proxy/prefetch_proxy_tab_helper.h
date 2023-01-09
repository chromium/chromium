// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_TAB_HELPER_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_TAB_HELPER_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_container.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_cookie_listener.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_network_context.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_prefetch_status.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_probe_result.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_type.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetched_mainframe_response_container.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/speculation_host_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/isolation_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class PrefetchProxyPageLoadMetricsObserver;
class PrefetchProxyPrefetchMetricsCollector;
class PrefetchProxySubresourceManager;
class Profile;

namespace net {
class IsolationInfo;
}  // namespace net

namespace network {
class SimpleURLLoader;
}  // namespace network

// This class listens to predictions of the next navigation and prefetches the
// mainpage content of Google Search Result Page links when they are available.
// When a prefetched page is navigated to, this class also copies over any
// cookies from the prefetch into the profile's cookie jar.
class PrefetchProxyTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PrefetchProxyTabHelper>,
      public NavigationPredictorKeyedService::Observer {
 public:
  ~PrefetchProxyTabHelper() override;

  class Observer {
   public:
    // Called when a decoy prefetch is completed with either success or failire.
    virtual void OnDecoyPrefetchCompleted(const GURL& url) {}

    // Called when a prefetch for |url| is completed successfully.
    virtual void OnPrefetchCompletedSuccessfully(const GURL& url) {}

    // Called when a prefetch for |url| is completed with an error code.
    // Negative values for |error_code| are a net::Error and positive values are
    // a HTTP error code.
    virtual void OnPrefetchCompletedWithError(const GURL& url, int error_code) {
    }

    // Called when a NoStatePrefetch finishes loading.
    virtual void OnNoStatePrefetchFinished() {}

    // Called when a url's eligiblity checks are done and fully processed.
    virtual void OnNewEligiblePrefetchStarted() {}

    // Called when the cookies associated with a prefetch are changed after the
    // initial eligiblity check.
    virtual void OnCookiesChangedForPrefetchAfterInitialCheck(const GURL& url) {
    }
  };

  // Container for several metrics which pertain to prefetching actions
  // on a Google SRP. RefCounted to allow TabHelper's friend classes to monitor
  // metrics without needing a callback for every event.
  class PrefetchMetrics : public base::RefCounted<PrefetchMetrics> {
   public:
    PrefetchMetrics();

    // The number of SRP links that were predicted. Only set on Google SRP pages
    // for eligible users. This should be used as the source of truth for
    // determining if the previous page was a Google SRP that could have had
    // prefetching actions.
    size_t predicted_urls_count_ = 0;

    // The number of SRP links that were eligible to be prefetched.
    size_t prefetch_eligible_count_ = 0;

    // The number of eligible prefetches that were attempted.
    size_t prefetch_attempted_count_ = 0;

    // The number of attempted prefetches that were successful (net error was OK
    // and HTTP response code was 2XX).
    size_t prefetch_successful_count_ = 0;

    // The total number of redirects encountered during all prefetches.
    size_t prefetch_total_redirect_count_ = 0;

    // The duration between navigation start and the start of prefetching.
    absl::optional<base::TimeDelta> navigation_to_prefetch_start_;

   private:
    friend class base::RefCounted<PrefetchMetrics>;
    ~PrefetchMetrics();
  };

  // Records metrics on the page load after a Google SRP for eligible users.
  class AfterSRPMetrics {
   public:
    AfterSRPMetrics();
    AfterSRPMetrics(const AfterSRPMetrics& other);
    ~AfterSRPMetrics();

    // The url of the page following the Google SRP.
    GURL url_;

    // The number of SRP links that were eligible to be prefetched on the SRP.
    // This is copied from the same named member of |srp_metrics_|.
    size_t prefetch_eligible_count_ = 0;

    // The position of the link on the SRP that was navigated to. Not set if the
    // navigated page wasn't in the SRP.
    absl::optional<size_t> clicked_link_srp_position_;

    // The status of a prefetch done on the SRP that may have been used here.
    absl::optional<PrefetchProxyPrefetchStatus> prefetch_status_;

    // The amount of time it took the probe to complete. Set only when a
    // prefetch is used and a probe was required.
    absl::optional<base::TimeDelta> probe_latency_;
  };

  // Checks if a |service_worker_context_for_test_| is available, and if not,
  // returns the real service worker context from the default storage partition.
  // This is used for passing the |service_worker_context| to
  // CheckEligibilityOfURL().
  static content::ServiceWorkerContext* GetServiceWorkerContext(
      Profile* profile);

  // Used to determine if |url| is eligible for prefetch proxy. Also gives a
  // reason in |status| if one is applicable.
  using OnEligibilityResultCallback = base::OnceCallback<void(
      const GURL& url,
      bool eligible,
      absl::optional<PrefetchProxyPrefetchStatus> status)>;
  static void CheckEligibilityOfURL(
      Profile* profile,
      const GURL& url,
      const PrefetchType& prefetch_type,
      OnEligibilityResultCallback result_callback);

  const PrefetchMetrics& srp_metrics() const { return *(page_->srp_metrics_); }

  // Returns nullopt unless the previous page load was a Google SRP where |this|
  // got parsed SRP links from NavigationPredictor.
  absl::optional<PrefetchProxyTabHelper::AfterSRPMetrics> after_srp_metrics()
      const;

  // Fetches |prefetches| (up to a limit) with the given |PrefetchType|.
  void PrefetchSpeculationCandidates(
      const std::vector<std::pair<GURL, PrefetchType>>& prefetches,
      const GURL& source_document_url,
      base::WeakPtr<content::SpeculationHostDevToolsObserver>
          devtools_observer);

  // content::WebContentsObserver implementation.
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // Notifies |this| that the NSP for |url| is done.
  void OnPrerenderDone(const GURL& url);

  // Takes ownership of a prefetched response by URL, if one if available.
  std::unique_ptr<PrefetchedMainframeResponseContainer> TakePrefetchResponse(
      const GURL& url);

  // Updates |prefetch_status_by_url_|.
  void OnPrefetchStatusUpdate(const GURL& url,
                              PrefetchProxyPrefetchStatus usage);

  // Called by the URLLoaderInterceptor to update |page_.probe_latency_|.
  void NotifyPrefetchProbeLatency(base::TimeDelta probe_latency);

  // Called by the URLLoaderInterceptor to report the outcome of an origin
  // probe.
  void ReportProbeResult(const GURL& url, PrefetchProxyProbeResult result);

  // When a previously prefetched page is navigated to, any cookies set on that
  // page load should be copied over to the normal profile. While this copy is
  // in progress, this method returns true to indicate to the navigation loader
  // interceptor that it should wait to commit the mainframe.
  // |SetOnAfterSRPCookieCopyCompleteCallback| can be used to set a callback
  // when the cookie copy process is complete.
  bool IsWaitingForAfterSRPCookiesCopy() const;
  void SetOnAfterSRPCookieCopyCompleteCallback(base::OnceClosure callback);

  // Called when the |PrefetchProxyURLLoaderInterceptor| checks the status of
  // the cookie copy.
  void OnInterceptorCheckCookieCopy();

  // Returns whether or not the cookies for the given URL have changed since the
  // initial eligibiilty check.
  bool HaveCookiesChanged(const GURL& url) const;

  void AddObserverForTesting(Observer* observer);
  void RemoveObserverForTesting(Observer* observer);

  network::mojom::NetworkContext* GetIsolatedContextForTesting(
      const GURL& url) const;

  // Sets the service_worker_context_for_test_ with a FakeServiceWorkerContext
  // for the the purpose of testing.
  // Used in the SetUp() method in prefetch_proxy_tab_helper_unittest.cc.
  static void SetServiceWorkerContextForTest(
      content::ServiceWorkerContext* context);

  // Overrides the logic for determining which hostnames should not be proxied.
  static void SetHostNonUniqueFilterForTest(bool (*filter)(base::StringPiece));
  static void ResetHostNonUniqueFilterForTest();

 protected:
  // Exposed for testing.
  explicit PrefetchProxyTabHelper(content::WebContents* web_contents);
  virtual network::mojom::URLLoaderFactory* GetURLLoaderFactory(
      const GURL& url);

 private:
  friend class PrefetchProxyPageLoadMetricsObserver;
  friend class content::WebContentsUserData<PrefetchProxyTabHelper>;

  // Identifies the state of the cookie copying process we're in, if any.
  enum class CookieCopyStatus {
    // No cookies need to be copied.
    kNoNavigation,

    // The cookie copy process is in progress.
    kWaitingForCopy,

    // The cookie copy process is complete.
    kCopyComplete,
  };

  // Owns all per-pageload state in the tab helper so that new navigations only
  // need to reset an instance of this class to clean up previous state.
  class CurrentPageLoad {
   public:
    explicit CurrentPageLoad(content::NavigationHandle* handle);
    ~CurrentPageLoad();

    // Helper functions to create / get the network context for a given URL. If
    // |PrefetchProxyUseIndividualNetworkContextsForEachPrefetch| is true, then
    // this will use the network context for a single prefetch in
    // |prefetch_containers_|. Otherwise this will use this instances
    // |network_context_|.
    void CreateNetworkContextForUrl(const GURL& url);
    PrefetchProxyNetworkContext* GetNetworkContextForUrl(const GURL& url) const;

    raw_ptr<Profile> profile_;

    // The set of URLs that can potentially be prefetched, and the state
    // associated the individual prefetches.
    std::map<GURL, std::unique_ptr<PrefetchContainer>> prefetch_containers_;

    // The start time of the current navigation.
    const base::TimeTicks navigation_start_;

    // Number of requests started that are decoy requests.
    size_t decoy_requests_attempted_ = 0;

    // The metrics pertaining to prefetching actions on a Google SRP page.
    scoped_refptr<PrefetchMetrics> srp_metrics_;

    // The metrics pertaining to how the prefetch is used after the Google SRP.
    // Only set for pages after a Google SRP.
    std::unique_ptr<AfterSRPMetrics> after_srp_metrics_;

    // Collects metrics on all prefetching. This is a scoped refptr so that it
    // can also be shared with subresource managers until all pointers to it are
    // destroyed, at which time it logs UKM.
    scoped_refptr<PrefetchProxyPrefetchMetricsCollector>
        prefetch_metrics_collector_;

    // The url loaders that do all the prefetches. Only active loaders are in
    // this set.
    std::set<std::unique_ptr<network::SimpleURLLoader>,
             base::UniquePtrComparator>
        url_loaders_;

    // An ordered list of the URLs to prefetch.
    std::vector<PrefetchContainer*> urls_to_prefetch_;

    // The amount of time that the probe took to complete. Kept in this class
    // until commit in order to be plumbed into |AfterSRPMetrics|.
    absl::optional<base::TimeDelta> probe_latency_;

    // The number of no state prefetch requests that have been made.
    size_t number_of_no_state_prefetch_attempts_ = 0;

    // The number of spare renderers that were started during this page load.
    size_t number_of_spare_renderers_started_ = 0;

    // All urls that are eligible to be no state prefetched. Once a no state
    // prefetch finishes, in success or in error, it is removed from this list.
    // If there is an active no state prefetch, its url will always be the first
    // element.
    std::vector<PrefetchContainer*> urls_to_no_state_prefetch_;

    // If the current page load was prerendered, then this subresource manager
    // is taken from |PrefetchProxyService| and used to facilitate loading
    // of prefetched resources from cache. Note: An
    // |PrefetchProxySubresourceManager| is dependent on the
    // |PrefetchProxyNetworkContext|s from the previous page load remaining
    // alive.
    std::unique_ptr<PrefetchProxySubresourceManager> subresource_manager_;

    // The current status of copying cookies for the next page load when the
    // user navigates to a prefetched link.
    CookieCopyStatus cookie_copy_status_ = CookieCopyStatus::kNoNavigation;

    // The time at which copying cookies for the next page load started.
    absl::optional<base::TimeTicks> cookie_copy_start_time_;

    // A callback that runs once |cookie_copy_status_| is set to copy complete.
    base::OnceClosure on_after_srp_cookie_copy_complete_;

    // If |PrefetchProxyUseIndividualNetworkContextsForEachPrefetch| is false
    // then this network context is used for all prefetches for this page load.
    // Otherwise each prefetch in |prefetch_containers_| will use its own
    // network context. The main purpose of using separate network contexts is
    // allow for a custom proxy configuration.
    std::unique_ptr<PrefetchProxyNetworkContext> network_context_;

    // This keeps the network context used to prefetch the current page load
    // from the previous page load alive, if the current page load was
    // prerendered, because |subresource_manager_| is dependent on it.
    std::unique_ptr<PrefetchProxyNetworkContext> previous_network_context_;
  };

  // Returns true if the current profile is not incognito and meets any
  // requirements for Lite Mode being enabled.
  static bool IsProfileEligible(Profile* profile);
  bool IsProfileEligible() const;

  // Returns whether the |url| is eligible, possibly with a status, without
  // considering any user data like service workers or cookies. Used to
  // determine eligibility and whether to send decoy requests.
  static std::pair<bool, absl::optional<PrefetchProxyPrefetchStatus>>
  CheckEligibilityOfURLSansUserData(Profile* profile,
                                    const GURL& url,
                                    const PrefetchType& prefetch_type);

  // Computes the AfterSRPMetrics that would be returned for the next
  // navigation, when it commits. This method exists to allow the PLM
  // Observer to get the AfterSRPMetrics if the navigation fails to commit,
  // so that metrics can be logged anyways. Returns nullptr if the after srp
  // metrics wouldn't be set on the next commit.
  std::unique_ptr<PrefetchProxyTabHelper::AfterSRPMetrics>
  ComputeAfterSRPMetricsBeforeCommit(content::NavigationHandle* handle) const;

  // A helper method to make it easier to tell when prefetching is already
  // active.
  bool PrefetchingActive() const;

  // Starts prefetching the next eligible links.
  void Prefetch();

  // Helper method for |Prefetch| which starts a single prefetch.
  void StartSinglePrefetch();

  // Called when |loader| encounters a redirect.
  void OnPrefetchRedirect(network::SimpleURLLoader* loader,
                          const GURL& original_url,
                          const net::RedirectInfo& redirect_info,
                          const network::mojom::URLResponseHead& response_head,
                          std::vector<std::string>* removed_headers);

  // Called when |loader| completes. |url| is the url that was requested and
  // |key| is the temporary NIK used during the request.
  void OnPrefetchComplete(network::SimpleURLLoader* loader,
                          const GURL& url,
                          const net::IsolationInfo& isolation_info,
                          std::unique_ptr<std::string> response_body);

  // Checks the response from |OnPrefetchComplete| for success or failure. On
  // success the response is moved to a |PrefetchedMainframeResponseContainer|
  // and cached in |prefetched_responses_|.
  void HandlePrefetchResponse(PrefetchContainer* prefetch_container,
                              const net::IsolationInfo& isolation_info,
                              network::mojom::URLResponseHeadPtr head,
                              std::unique_ptr<std::string> body);

  // Checks if the given |url| is eligible to be no state prefetched and if so,
  // adds it to the list of urls to be no state prefetched.
  void MaybeDoNoStatePrefetch(PrefetchContainer* prefetch_container);

  // Starts a new no state prefetch for the next eligible url.
  void DoNoStatePrefetch();

  // Starts a spare renderer. Should only be called when all NSPs and
  // prefetching is complete.
  void StartSpareRenderer();

  // Makes a clone of |this|'s prefetch response so that it can be used for
  // NoStatePrefetch now and later reused if the user navigates to that page.
  std::unique_ptr<PrefetchedMainframeResponseContainer>
  CopyPrefetchResponseForNSP(PrefetchContainer* prefetch_container);

  // Updates any status like kPrefetchUsed or kPrefetchNotUsed with additional
  // information about applicable NoStatePrefetches given |self|. Note: This is
  // done here because the navigation loader interceptor doesn't have visibility
  // itself and can't report it. Static and public to enable testing.
  PrefetchProxyPrefetchStatus MaybeUpdatePrefetchStatusWithNSPContext(
      PrefetchContainer* prefetch_container) const;

  // NavigationPredictorKeyedService::Observer:
  void OnPredictionUpdated(
      const absl::optional<NavigationPredictorKeyedService::Prediction>
          prediction) override;

  // Fetches the |prefetch_targets| with the given |PrefetchType|.
  void PrefetchUrls(
      const std::vector<std::pair<GURL, PrefetchType>>& prefetch_targets,
      base::WeakPtr<content::SpeculationHostDevToolsObserver>
          devtools_observer);

  // Used as a callback for when the eligibility of |url| is determined.
  void OnGotEligibilityResult(
      const GURL& url,
      bool eligible,
      absl::optional<PrefetchProxyPrefetchStatus> status);

  // Starts a query for all cookies associated with |prefetch_container|| in the
  // isolated cookie jar so that they can be copied to the normal profile. After
  // this method is called, |IsWaitingForAfterSRPCookiesCopy| returns true until
  // |OnCopiedIsolatedCookiesAfterSRPClick| runs.
  void CopyIsolatedCookiesOnAfterSRPClick(
      PrefetchContainer* prefetch_container);

  // Starts copying all cookies in |cookie_list| to the normal profile.
  void OnGotIsolatedCookiesToCopyAfterSRPClick(
      const GURL& url,
      const net::CookieAccessResultList& cookie_list,
      const net::CookieAccessResultList& excluded_cookies);

  // When this is called, |IsWaitingForAfterSRPCookiesCopy| will return false
  // again and the callback passed to |SetOnAfterSRPCookieCopyCompleteCallback|,
  // if any, is run.
  void OnCopiedIsolatedCookiesAfterSRPClick();

  // Prepare to serve prefetched resources for the given |url| when a navigation
  // to that url is started. This initiates the copying of cookies from the
  // isolated network context to the default context, and notifies the
  // |PrefetchProxySubresourceManager| associated with |url|.
  void PrepareToServe(const GURL& url);

  // Called when the cookies of a prefetch have changed at some point after the
  // initial check.
  void OnCookiesChangedAfterInitialCheck(const GURL& url);

  raw_ptr<Profile> profile_;

  // Owns all members which need to be reset on a new page load.
  std::unique_ptr<CurrentPageLoad> page_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PrefetchProxyTabHelper> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  PrefetchProxyTabHelper(const PrefetchProxyTabHelper&) = delete;
  PrefetchProxyTabHelper& operator=(const PrefetchProxyTabHelper&) = delete;
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_TAB_HELPER_H_
