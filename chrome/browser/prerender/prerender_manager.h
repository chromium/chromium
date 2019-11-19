// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/prerender/prerender_config.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_histograms.h"
#include "chrome/browser/prerender/prerender_origin.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/render_process_host_observer.h"
#include "url/gurl.h"
#include "url/origin.h"

class Profile;

namespace base {
class DictionaryValue;
class ListValue;
class TickClock;
}

struct NavigateParams;

namespace chrome_browser_net {
enum class NetworkPredictionStatus;
}

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
class Size;
}

namespace prerender {

namespace test_utils {
class PrerenderInProcessBrowserTest;
}

class PrerenderHandle;
class PrerenderHistory;

// Observer interface for PrerenderManager events.
class PrerenderManagerObserver {
 public:
  virtual ~PrerenderManagerObserver();

  // Called from the UI thread.
  virtual void OnFirstContentfulPaint() = 0;
};

// PrerenderManager is responsible for initiating and keeping prerendered
// views of web pages. All methods must be called on the UI thread unless
// indicated otherwise.
class PrerenderManager : public content::RenderProcessHostObserver,
                         public KeyedService,
                         public MediaCaptureDevicesDispatcher::Observer {
 public:
  enum PrerenderManagerMode {
    // Deprecated: Enables all types of prerendering for any origin.
    DEPRECATED_PRERENDER_MODE_ENABLED,

    // For each request to prerender performs a NoStatePrefetch for the same URL
    // instead.
    PRERENDER_MODE_NOSTATE_PREFETCH,

    // Ignores requests to prerender, but keeps track of pages that would have
    // been prerendered and records metrics for comparison with other modes.
    PRERENDER_MODE_SIMPLE_LOAD_EXPERIMENT
  };

  // One or more of these flags must be passed to ClearData() to specify just
  // what data to clear.  See function declaration for more information.
  enum ClearFlags {
    CLEAR_PRERENDER_CONTENTS = 0x1 << 0,
    CLEAR_PRERENDER_HISTORY = 0x1 << 1,
    CLEAR_MAX = 0x1 << 2
  };

  // If |url| matches a valid prerendered page in one of the contents,
  // try to swap it and merge browsing histories.
  //
  // Returns true if a prerendered page is swapped in. When this happens, the
  // PrerenderManager has already swapped out |contents_being_navigated| with
  // |replaced_contents| in the WebContents container [e.g. TabStripModel on
  // desktop]. |loaded| is set to true if the page finished loading.
  //
  // Returns false if nothing is swapped.
  //
  // |loaded| cannot be null.
  static bool MaybeUsePrerenderedPage(Profile* profile,
                                      content::WebContents* web_contents,
                                      const GURL& url,
                                      bool* loaded);

  // Owned by a Profile object for the lifetime of the profile.
  explicit PrerenderManager(Profile* profile);
  ~PrerenderManager() override;

  // From KeyedService:
  void Shutdown() override;

  // Entry points for adding prerenders.

  // Adds a prerender for |url| if valid. |process_id| and |route_id| identify
  // the RenderView that the prerender request came from. If |size| is empty, a
  // default from the PrerenderConfig is used. Returns a PrerenderHandle if the
  // URL was added, NULL if it was not. If the launching RenderView is itself
  // prerendering, the prerender is added as a pending prerender.
  std::unique_ptr<PrerenderHandle> AddPrerenderFromLinkRelPrerender(
      int process_id,
      int route_id,
      const GURL& url,
      uint32_t rel_types,
      const content::Referrer& referrer,
      const url::Origin& initiator_origin,
      const gfx::Size& size);

  // Adds a prerender for |url| if valid. As the prerender request is coming
  // from a source without a RenderFrameHost (i.e., the omnibox) we don't have a
  // child or route id, or a referrer. This method uses sensible values for
  // those. The |session_storage_namespace| matches the namespace of the active
  // tab at the time the prerender is generated from the omnibox. Returns a
  // PrerenderHandle or NULL. If the prerender fails, the prerender manager may
  // fallback and initiate a preconnect to |url|.
  std::unique_ptr<PrerenderHandle> AddPrerenderFromOmnibox(
      const GURL& url,
      content::SessionStorageNamespace* session_storage_namespace,
      const gfx::Size& size);

  // Adds a prerender for the prefetch url from NavigationPredictor on
  // page load, if NoStatePrefetch and prefetch_after_preconnect are true.
  // Uses the NavigationPredictor's browser context and the default
  // SessionStorageNamespace. Returns a PrerenderHandle or NULL.
  std::unique_ptr<PrerenderHandle> AddPrerenderFromNavigationPredictor(
      const GURL& url,
      content::SessionStorageNamespace* session_storage_namespace,
      const gfx::Size& size);

  std::unique_ptr<PrerenderHandle> AddPrerenderFromExternalRequest(
      const GURL& url,
      const content::Referrer& referrer,
      content::SessionStorageNamespace* session_storage_namespace,
      const gfx::Rect& bounds);

  // Adds a prerender from an external request that will prerender even on
  // cellular networks as long as the user setting for prerendering is ON.
  std::unique_ptr<PrerenderHandle> AddForcedPrerenderFromExternalRequest(
      const GURL& url,
      const content::Referrer& referrer,
      content::SessionStorageNamespace* session_storage_namespace,
      const gfx::Rect& bounds);

  // Cancels all active prerenders.
  void CancelAllPrerenders();

  // Wraps input and output parameters to MaybeUsePrerenderedPage.
  struct Params {
    Params(NavigateParams* params,
           content::WebContents* contents_being_navigated);
    Params(bool uses_post,
           const std::string& extra_headers,
           bool should_replace_current_entry,
           content::WebContents* contents_being_navigated);

    // Input parameters.
    const bool uses_post;
    const std::string extra_headers;
    const bool should_replace_current_entry;
    content::WebContents* const contents_being_navigated;

    // Output parameters.
    content::WebContents* replaced_contents = nullptr;
  };

  // If |url| matches a valid prerendered page and |params| are compatible, try
  // to swap it and merge browsing histories.
  //
  // Returns true if a prerendered page is swapped in. When this happens, the
  // PrerenderManager has already swapped out |contents_being_navigated| with
  // |replaced_contents| in the WebContents container [e.g. TabStripModel on
  // desktop].
  //
  // Returns false if nothing is swapped.
  bool MaybeUsePrerenderedPage(const GURL& url, Params* params);

  // Moves a PrerenderContents to the pending delete list from the list of
  // active prerenders when prerendering should be cancelled.
  virtual void MoveEntryToPendingDelete(PrerenderContents* entry,
                                        FinalStatus final_status);

  // Called to record the time to First Contentful Paint for all pages that were
  // not prerendered.
  //
  // As part of recording, determines whether the load had previously matched
  // the criteria for triggering a NoStatePrefetch. In the prerendering
  // experimental group such triggering makes the page prerendered, while in the
  // group doing only 'simple loads' it would have been a noop.
  //
  // Must not be called for prefetch loads themselves (which are never painted
  // anyway). The |is_no_store| must be true iff the main resource has a
  // "no-store" cache control HTTP header. The |was_hidden| tells whether the
  // the page was hidden at least once between starting the load and registering
  // the FCP.
  void RecordNoStateFirstContentfulPaint(const GURL& url,
                                         bool is_no_store,
                                         bool was_hidden,
                                         base::TimeDelta time);

  static PrerenderManagerMode GetMode() { return mode_; }
  static void SetMode(PrerenderManagerMode mode) { mode_ = mode; }

  // Query the list of current prerender pages to see if the given web contents
  // is prerendering a page. The optional parameter |origin| is an output
  // parameter which, if a prerender is found, is set to the Origin of the
  // prerender |web_contents|.
  bool IsWebContentsPrerendering(const content::WebContents* web_contents,
                                 Origin* origin) const;

  // Whether the PrerenderManager has an active prerender with the given url and
  // SessionStorageNamespace associated with the given WebContents.
  bool HasPrerenderedUrl(GURL url, content::WebContents* web_contents) const;

  // Whether the PrerenderManager has an active prerender with the given url and
  // SessionStorageNamespace associated with the given WebContents, and that
  // prerender has finished loading..
  bool HasPrerenderedAndFinishedLoadingUrl(
      GURL url,
      content::WebContents* web_contents) const;

  // Returns the PrerenderContents object for the given web_contents, otherwise
  // returns NULL. Note that the PrerenderContents may have been Destroy()ed,
  // but not yet deleted.
  PrerenderContents* GetPrerenderContents(
      const content::WebContents* web_contents) const;

  // Returns the PrerenderContents object for a given child_id, route_id pair,
  // otherwise returns NULL. Note that the PrerenderContents may have been
  // Destroy()ed, but not yet deleted.
  virtual PrerenderContents* GetPrerenderContentsForRoute(
      int child_id, int route_id) const;

  // Returns the PrerenderContents object that is found in active prerenders to
  // match the |render_process_id|. Otherwise returns a nullptr.
  PrerenderContents* GetPrerenderContentsForProcess(
      int render_process_id) const;

  // Returns a list of all WebContents being prerendered.
  std::vector<content::WebContents*> GetAllPrerenderingContents() const;

  // Checks whether |url| has been recently navigated to.
  bool HasRecentlyBeenNavigatedTo(Origin origin, const GURL& url);

  // Returns a Value object containing the active pages being prerendered, and
  // a history of pages which were prerendered.
  std::unique_ptr<base::DictionaryValue> CopyAsValue() const;

  // Clears the data indicated by which bits of clear_flags are set.
  //
  // If the CLEAR_PRERENDER_CONTENTS bit is set, all active prerenders are
  // cancelled and then deleted, and any WebContents queued for destruction are
  // destroyed as well.
  //
  // If the CLEAR_PRERENDER_HISTORY bit is set, the prerender history is
  // cleared, including any entries newly created by destroying them in
  // response to the CLEAR_PRERENDER_CONTENTS flag.
  //
  // Intended to be used when clearing the cache or history.
  void ClearData(int clear_flags);

  // Record a final status of a prerendered page in a histogram.
  void RecordFinalStatus(Origin origin, FinalStatus final_status) const;

  // MediaCaptureDevicesDispatcher::Observer
  void OnCreatingAudioStream(int render_process_id,
                             int render_frame_id) override;

  const Config& config() const { return config_; }
  Config& mutable_config() { return config_; }

  // Records that some visible tab navigated (or was redirected) to the
  // provided URL.
  void RecordNavigation(const GURL& url);

  Profile* profile() const { return profile_; }

  // Return current time and ticks with ability to mock the clock out for
  // testing.
  base::Time GetCurrentTime() const;
  base::TimeTicks GetCurrentTimeTicks() const;
  void SetTickClockForTesting(const base::TickClock* tick_clock);

  void DisablePageLoadMetricsObserverForTesting() {
    page_load_metric_observer_disabled_ = true;
  }

  bool PageLoadMetricsObserverDisabledForTesting() const {
    return page_load_metric_observer_disabled_;
  }

  void AddObserver(std::unique_ptr<PrerenderManagerObserver> observer);

  // Notification that a prerender has completed and its bytes should be
  // recorded.
  void RecordNetworkBytesConsumed(Origin origin, int64_t prerender_bytes);

  // Add to the running tally of bytes transferred over the network for this
  // profile if prerendering is currently enabled.
  void AddProfileNetworkBytesIfEnabled(int64_t bytes);

  // Registers a new ProcessHost performing a prerender. Called by
  // PrerenderContents.
  void AddPrerenderProcessHost(content::RenderProcessHost* process_host);

  // Returns whether or not |process_host| may be reused for new navigations
  // from a prerendering perspective. Currently, if Prerender Cookie Stores are
  // enabled, prerenders must be in their own processes that may not be shared.
  bool MayReuseProcessHost(content::RenderProcessHost* process_host);

  // content::RenderProcessHostObserver implementation.
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // Cleans up the expired prefetches and then returns true if |url| was
  // no-state prefetched recently. If so, |prefetch_age|, |final_status| and
  // |origin| are set based on the no-state prefetch information if they are
  // non-null.
  bool GetPrefetchInformation(const GURL& url,
                              base::TimeDelta* prefetch_age,
                              FinalStatus* final_status,
                              Origin* origin);

  void SetPrerenderContentsFactoryForTest(
      PrerenderContents::Factory* prerender_contents_factory);

  base::WeakPtr<PrerenderManager> AsWeakPtr();

  // Clears the list of recently prefetched URLs. Allows, for example, to reuse
  // the same URL in tests, without running into FINAL_STATUS_DUPLICATE.
  void ClearPrefetchInformationForTesting();

  // Returns true iff the |url| is found in the list of recent prefetches.
  bool HasRecentlyPrefetchedUrlForTesting(const GURL& url);

 protected:
  class PrerenderData : public base::SupportsWeakPtr<PrerenderData> {
   public:
    struct OrderByExpiryTime;

    PrerenderData(PrerenderManager* manager,
                  std::unique_ptr<PrerenderContents> contents,
                  base::TimeTicks expiry_time);

    ~PrerenderData();

    // A new PrerenderHandle has been created for this PrerenderData.
    void OnHandleCreated(PrerenderHandle* prerender_handle);

    // The launcher associated with a handle is navigating away from the context
    // that launched this prerender. If the prerender is active, it may stay
    // alive briefly though, in case we we going through a redirect chain that
    // will eventually land at it.
    void OnHandleNavigatedAway(PrerenderHandle* prerender_handle);

    // The launcher associated with a handle has taken explicit action to cancel
    // this prerender. We may well destroy the prerender in this case if no
    // other handles continue to track it.
    void OnHandleCanceled(PrerenderHandle* prerender_handle);

    PrerenderContents* contents() { return contents_.get(); }

    std::unique_ptr<PrerenderContents> ReleaseContents();

    int handle_count() const { return handle_count_; }

    base::TimeTicks abandon_time() const { return abandon_time_; }

    base::TimeTicks expiry_time() const { return expiry_time_; }
    void set_expiry_time(base::TimeTicks expiry_time) {
      expiry_time_ = expiry_time;
    }

   private:
    PrerenderManager* const manager_;
    std::unique_ptr<PrerenderContents> contents_;

    // The number of distinct PrerenderHandles created for |this|, including
    // ones that have called PrerenderData::OnHandleNavigatedAway(), but not
    // counting the ones that have called PrerenderData::OnHandleCanceled(). For
    // pending prerenders, this will always be 1, since the PrerenderManager
    // only merges handles of running prerenders.
    int handle_count_ = 0;

    // The time when OnHandleNavigatedAway was called.
    base::TimeTicks abandon_time_;

    // After this time, this prerender is no longer fresh, and should be
    // removed.
    base::TimeTicks expiry_time_;

    DISALLOW_COPY_AND_ASSIGN(PrerenderData);
  };

  // Called by a PrerenderData to signal that the launcher has navigated away
  // from the context that launched the prerender. A user may have clicked
  // a link in a page containing a <link rel=prerender> element, or the user
  // might have committed an omnibox navigation. This is used to possibly
  // shorten the TTL of the prerendered page.
  void SourceNavigatedAway(PrerenderData* prerender_data);

  // Same as base::SysInfo::IsLowEndDevice(), overridden in tests.
  virtual bool IsLowEndDevice() const;

 private:
  friend class test_utils::PrerenderInProcessBrowserTest;
  friend class PrerenderContents;
  friend class PrerenderHandle;
  friend class UnitTestPrerenderManager;

  class OnCloseWebContentsDeleter;
  struct NavigationRecord;
  using PrerenderDataVector = std::vector<std::unique_ptr<PrerenderData>>;

  // Time interval before a new prerender is allowed.
  static const int kMinTimeBetweenPrerendersMs = 500;

  // Time window for which we record old navigations, in milliseconds.
  static const int kNavigationRecordWindowMs = 5000;

  // Returns whether prerendering is currently enabled or the reason why it is
  // disabled.
  chrome_browser_net::NetworkPredictionStatus GetPredictionStatus() const;

  // Returns whether prerendering is currently enabled or the reason why it is
  // disabled after taking into account the origin of the request.
  chrome_browser_net::NetworkPredictionStatus GetPredictionStatusForOrigin(
      Origin origin) const;

  // Adds a prerender for |url| from |referrer|. The |origin| specifies how the
  // prerender was added. If |bounds| is empty, then
  // PrerenderContents::StartPrerendering will instead use a default from
  // PrerenderConfig. Returns a PrerenderHandle or NULL.
  std::unique_ptr<PrerenderHandle> AddPrerenderWithPreconnectFallback(
      Origin origin,
      const GURL& url,
      const content::Referrer& referrer,
      const base::Optional<url::Origin>& initiator_origin,
      const gfx::Rect& bounds,
      content::SessionStorageNamespace* session_storage_namespace);

  void StartSchedulingPeriodicCleanups();
  void StopSchedulingPeriodicCleanups();

  void EvictOldestPrerendersIfNecessary();

  // Deletes stale and cancelled prerendered PrerenderContents, as well as
  // WebContents that have been replaced by prerendered WebContents.
  // Also identifies and kills PrerenderContents that use too much
  // resources.
  void PeriodicCleanup();

  // Posts a task to call PeriodicCleanup.  Results in quicker destruction of
  // objects.  If |this| is deleted before the task is run, the task will
  // automatically be cancelled.
  void PostCleanupTask();

  base::TimeTicks GetExpiryTimeForNewPrerender(Origin origin) const;
  base::TimeTicks GetExpiryTimeForNavigatedAwayPrerender() const;

  void DeleteOldEntries();

  void DeleteToDeletePrerenders();

  // Virtual so unit tests can override this.
  virtual std::unique_ptr<PrerenderContents> CreatePrerenderContents(
      const GURL& url,
      const content::Referrer& referrer,
      const base::Optional<url::Origin>& initiator_origin,
      Origin origin);

  // Insures the |active_prerenders_| are sorted by increasing expiry time. Call
  // after every mutation of |active_prerenders_| that can possibly make it
  // unsorted (e.g. an insert, or changing an expiry time).
  void SortActivePrerenders();

  // Finds the active PrerenderData object for a running prerender matching
  // |url| and |session_storage_namespace|.
  PrerenderData* FindPrerenderData(
      const GURL& url,
      content::SessionStorageNamespace* session_storage_namespace);

  // Given the |prerender_contents|, find the iterator in |active_prerenders_|
  // correponding to the given prerender.
  PrerenderDataVector::iterator FindIteratorForPrerenderContents(
      PrerenderContents* prerender_contents);

  bool DoesRateLimitAllowPrerender(Origin origin) const;

  // Deletes old WebContents that have been replaced by prerendered ones.  This
  // is needed because they're replaced in a callback from the old WebContents,
  // so cannot immediately be deleted.
  void DeleteOldWebContents();

  // Called when PrerenderContents gets destroyed. Attaches the |final_status|
  // to the most recent prefetch matching the |url|.
  void SetPrefetchFinalStatusForUrl(const GURL& url, FinalStatus final_status);

  // Called when a prefetch has been used. Prefetches avoid cache revalidation
  // only once.
  void OnPrefetchUsed(const GURL& url);

  // Cleans up old NavigationRecord's.
  void CleanUpOldNavigations(std::vector<NavigationRecord>* navigations,
                             base::TimeDelta max_age);

  // Arrange for the given WebContents to be deleted asap. Delete |deleter| as
  // well.
  void ScheduleDeleteOldWebContents(std::unique_ptr<content::WebContents> tab,
                                    OnCloseWebContentsDeleter* deleter);

  // Adds to the history list.
  void AddToHistory(PrerenderContents* contents);

  // Returns a new Value representing the pages currently being prerendered.
  std::unique_ptr<base::ListValue> GetActivePrerendersAsValue() const;

  // Destroys all pending prerenders using FinalStatus.  Also deletes them as
  // well as any swapped out WebContents queued for destruction.
  // Used both on destruction, and when clearing the browsing history.
  void DestroyAllContents(FinalStatus final_status);

  // Records the final status a prerender in the case that a PrerenderContents
  // was never created, adds a PrerenderHistory entry, and may also initiate a
  // preconnect to |url|.
  void SkipPrerenderContentsAndMaybePreconnect(const GURL& url,
                                               Origin origin,
                                               FinalStatus final_status) const;

  // Swaps a prerender |prerender_data| for |url| into the tab, replacing
  // |web_contents|.  Returns the new WebContents that was swapped in, or NULL
  // if a swap-in was not possible.  If |should_replace_current_entry| is true,
  // the current history entry in |web_contents| is replaced.
  content::WebContents* SwapInternal(const GURL& url,
                                     content::WebContents* web_contents,
                                     PrerenderData* prerender_data,
                                     bool should_replace_current_entry);

  // May initiate a preconnect to |url_arg| based on |origin|.
  void MaybePreconnect(Origin origin, const GURL& url_arg) const;

  // The configuration.
  Config config_;

  // The profile that owns this PrerenderManager.
  Profile* profile_;

  // All running prerenders. Sorted by expiry time, in ascending order.
  PrerenderDataVector active_prerenders_;

  // Prerenders awaiting deletion.
  PrerenderDataVector to_delete_prerenders_;

  // List of recent navigations in this profile, sorted by ascending
  // |navigate_time_|.
  std::vector<NavigationRecord> navigations_;

  // List of recent prefetches, sorted by ascending navigate time.
  std::vector<NavigationRecord> prefetches_;

  std::unique_ptr<PrerenderContents::Factory> prerender_contents_factory_;

  static PrerenderManagerMode mode_;

  // RepeatingTimer to perform periodic cleanups of pending prerendered
  // pages.
  base::RepeatingTimer repeating_timer_;

  // Track time of last prerender to limit prerender spam.
  base::TimeTicks last_prerender_start_time_;

  std::vector<std::unique_ptr<content::WebContents>> old_web_contents_list_;

  std::vector<std::unique_ptr<OnCloseWebContentsDeleter>>
      on_close_web_contents_deleters_;

  const std::unique_ptr<PrerenderHistory> prerender_history_;

  const std::unique_ptr<PrerenderHistograms> histograms_;

  // The number of bytes transferred over the network for the profile this
  // PrerenderManager is attached to.
  int64_t profile_network_bytes_ = 0;

  // The value of profile_network_bytes_ that was last recorded.
  int64_t last_recorded_profile_network_bytes_ = 0;

  // Set of process hosts being prerendered.
  using PrerenderProcessSet = std::set<content::RenderProcessHost*>;
  PrerenderProcessSet prerender_process_hosts_;

  const base::TickClock* tick_clock_;

  bool page_load_metric_observer_disabled_ = false;

  std::vector<std::unique_ptr<PrerenderManagerObserver>> observers_;

  base::WeakPtrFactory<PrerenderManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PrerenderManager);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
