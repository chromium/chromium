// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/tab_strip_page_load_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/time/time_delta_from_string.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else  // BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif  // BUILDFLAG(IS_ANDROID)

#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"

namespace internal {

const char kTabsActiveAbsolutePosition[] = "Tabs.Active.AbsolutePosition";
const char kTabsActiveRelativePosition[] = "Tabs.Active.RelativePosition";
const char kTabsPageLoadTimeSinceActive[] = "Tabs.PageLoad.TimeSinceActive2";
const char kTabsPageLoadTimeSinceCreated[] = "Tabs.PageLoad.TimeSinceCreated2";

}  // namespace internal

namespace {

// TODO(crbug.com/40915391): Create an iterator abstraction that could be reused
// in other places we need to iterate across tabs for both Android and desktop.
std::vector<std::vector<content::WebContents*>> GetAllWebContents() {
  std::vector<std::vector<content::WebContents*>> all_web_contents = {};
#if BUILDFLAG(IS_ANDROID)
  for (const TabModel* model : TabModelList::models()) {
    std::vector<content::WebContents*> web_contents_for_tab_strip = {};
    for (int i = 0; i < model->GetTabCount(); ++i) {
#else   // BUILDFLAG(IS_ANDROID)
  for (Browser* browser : *BrowserList::GetInstance()) {
    std::vector<content::WebContents*> web_contents_for_tab_strip = {};
    TabStripModel* model = browser->tab_strip_model();
    for (int i = 0; i < model->count(); ++i) {
#endif  // BUILDFLAG(IS_ANDROID)
      web_contents_for_tab_strip.push_back(model->GetWebContentsAt(i));
    }
    all_web_contents.push_back(web_contents_for_tab_strip);
  }
  return all_web_contents;
}

void RecordTimeDeltaHistogram(const char histogram_name[],
                              base::TimeDelta value) {
  base::UmaHistogramCustomTimes(histogram_name, value, base::TimeDelta(),
                                base::Days(21), 50);
}

}  // namespace

TabStripPageLoadMetricsObserver::TabStripPageLoadMetricsObserver(
    content::WebContents* web_contents) {
  web_contents_ = web_contents;
}

TabStripPageLoadMetricsObserver::~TabStripPageLoadMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
TabStripPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  if (!started_in_foreground) {
    return CONTINUE_OBSERVING;
  }
  base::TimeTicks now = base::TimeTicks::Now();
  std::vector<std::vector<content::WebContents*>> all_web_contents =
      GetAllWebContents();
  for (std::vector<content::WebContents*> tab_strip_web_contents :
       all_web_contents) {
    for (content::WebContents* web_contents : tab_strip_web_contents) {
      if (web_contents) {
        base::TimeTicks last_active = web_contents->GetLastActiveTimeTicks();
        page_load_metrics::MetricsWebContentsObserver*
            metrics_web_contents_observer =
                page_load_metrics::MetricsWebContentsObserver::FromWebContents(
                    web_contents);
        base::TimeTicks created = metrics_web_contents_observer->GetCreated();
        RecordTimeDeltaHistogram(internal::kTabsPageLoadTimeSinceActive,
                                 now - last_active);
        RecordTimeDeltaHistogram(internal::kTabsPageLoadTimeSinceCreated,
                                 now - created);
      }
    }
  }
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
TabStripPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
TabStripPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
TabStripPageLoadMetricsObserver::OnShown() {
  std::vector<std::vector<content::WebContents*>> all_web_contents =
      GetAllWebContents();
  for (std::vector<content::WebContents*> tab_strip_web_contents :
       all_web_contents) {
    const int count = tab_strip_web_contents.size();
    for (int i = 0; i < count; i++) {
      if (tab_strip_web_contents.at(i) == web_contents_.get()) {
        const int absolute_sequence = i + 1;
        base::UmaHistogramCounts1000(internal::kTabsActiveAbsolutePosition,
                                     absolute_sequence);
        base::UmaHistogramPercentage(internal::kTabsActiveRelativePosition,
                                     absolute_sequence * 100 / count);
        break;
      }
    }
  }
  return CONTINUE_OBSERVING;
}
