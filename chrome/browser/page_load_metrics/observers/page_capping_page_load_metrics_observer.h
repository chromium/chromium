// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PAGE_CAPPING_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PAGE_CAPPING_PAGE_LOAD_METRICS_OBSERVER_H_

#include <memory>
#include <vector>

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "third_party/blink/public/mojom/loader/pause_subresource_loading_handle.mojom.h"

namespace content {
class WebContents;
}  // namespace content

class PageLoadCappingBlacklist;

// A class that tracks the data usage of a page load and triggers an infobar
// when the page load is above a certain threshold. The thresholds are field
// trial controlled and vary based on whether media has played on the page.
// TODO(ryansturm): This class can change the functionality of the page itself
// through pausing subresource loading (by owning a collection of
// PauseSubResourceLoadingHandlePtr's). This type of behavior is typically not
// seen in page load metrics observers, but the PageLoadTracker functionality
// (request data usage) is necessary for determining triggering conditions.
// Consider moving to a WebContentsObserver/TabHelper and source byte updates
// from this class to that observer. https://crbug.com/840399
class PageCappingPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  PageCappingPageLoadMetricsObserver();
  ~PageCappingPageLoadMetricsObserver() override;

  // Returns whether the page's subresource loading is currently paused.
  bool IsPausedForTesting() const;

  // Tests can change the behavior of clock for testing time between resource
  // loads.
  void SetTickClockForTesting(base::TickClock* clock);

  // The current state of the page.
  // This class operates as a state machine going from each of the below states
  // in order. This is recorded to UKM, so the enum should not be changed.
  enum class PageCappingState {
    // The initial state of the page. No InfoBar has been shown.
    kInfoBarNotShown = 0,
    // When the cap is met, an InfoBar will be shown.
    kInfoBarShown = 1,
    // If the user clicks pause on the InfoBar, the page will be paused.
    kPagePaused = 2,
    // If the user then clicks resume on the InfoBar the page is resumed. This
    // is the final state.
    kPageResumed = 3,
  };

 protected:
  // Virtual for testing.
  // Gets the random offset for the capping threshold.
  virtual int64_t GetFuzzingOffset() const;

  // Virtual for testing.
  // Gets the page load capping blacklist from the page load capping service.
  // Returns null for incognito profiles or profiles that have Data Saver
  // disabled.
  virtual PageLoadCappingBlacklist* GetPageLoadCappingBlacklist() const;

 private:
  // page_load_metrics::PageLoadMetricsObserver:
  void OnResourceDataUseObserved(
      const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
          resources) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override;
  void MediaStartedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& video_type,
      bool is_in_main_frame) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnComplete(const page_load_metrics::mojom::PageLoadTiming& timing,
                  const page_load_metrics::PageLoadExtraInfo& info) override;

  // If this is the first time this is called, queries the page load capping
  // blacklist for whether the InfoBar should be allowed and records UMA.
  // Otherwise, this returns the cached value.
  // Records nothing for incognito Profiles and returns false.
  bool IsBlacklisted();

  // Reports whether the page was an opt out or not to the blacklist.
  void ReportOptOut();

  // Records a new estimate of data savings based on data used and field trial
  // params. Also records the PageCappingState to UKM.
  void RecordDataSavingsAndUKM(
      const page_load_metrics::PageLoadExtraInfo& info);

  // Writes the amount of savings to the data saver feature. Virtual for
  // testing.
  virtual void WriteToSavings(int64_t bytes_saved);

  // Show the page capping infobar if it has not been shown before and the data
  // use is above the threshold.
  void MaybeCreate();

  // Pauses or unpauses the subresource loading of the page based on |paused|.
  // TODO(ryansturm): New Subframes will not be paused automatically and may
  // load resources. https://crbug.com/835895
  void PauseSubresourceLoading(bool paused);

  // Sets |time_to_expire| to the earliest time duration that the page load is
  // considered not to be using data anymore. |time_to_expire| must be passed in
  // as TimeDelta initialized to 0 to handle the case of the underlying weak
  // pointer being destroyed.
  // If |time_to_expire| is returned as 0, the consumer should treat the page as
  // not using data anymore, and does not need to wait any longer to consider
  // the page stopped with respect to data use..
  void TimeToExpire(base::TimeDelta* time_to_expire) const;

  // The current bytes threshold of the capping page triggering.
  base::Optional<int64_t> page_cap_;

  // The WebContents for this page load. |this| cannot outlive |web_contents|.
  content::WebContents* web_contents_ = nullptr;

  // The host to attribute savings to.
  std::string url_host_;

  // Whether a media element has been played on the page.
  bool media_page_load_ = false;

  // The cumulative network body bytes used so far.
  int64_t network_bytes_ = 0;

  // The amount of bytes when the data savings was last recorded.
  int64_t recorded_savings_ = 0;

  PageCappingState page_capping_state_ = PageCappingState::kInfoBarNotShown;

  // True once UKM has been recorded. This is recorded at the same time as
  // PageLoad UKM (during hidden, complete, or app background).
  bool ukm_recorded_ = false;

  // The randomly generated offset from the capping threshold.
  int64_t fuzzing_offset_ = 0;

  // Empty until the blacklist is queried and UMA is recorded about blacklist
  // reason. Once populated, whether the feature is blacklisted or not for the
  // user on the URL of this page. Incognito Profiles or Profiles with Data
  // Saver disabled will cause this to be false once populated.
  base::Optional<bool> blacklisted_;

  // If non-empty, a group of handles that are pausing subresource loads in the
  // render frames of this page.
  std::vector<blink::mojom::PauseSubresourceLoadingHandlePtr> handles_;

  base::Optional<base::TimeTicks> last_data_use_time_;

  // Default clock unless SetClockForTesting is called.
  const base::TickClock* clock_;

  base::WeakPtrFactory<PageCappingPageLoadMetricsObserver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PageCappingPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PAGE_CAPPING_PAGE_LOAD_METRICS_OBSERVER_H_
