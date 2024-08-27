// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ash_session_restore_page_load_metrics_observer.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/app_restore/full_restore_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

namespace {

// Ensures first input delay is recorded at most once per program lifetime (the
// first time the user interacts with a restored browser window after logging
// in). Switching users afterwards is intentionally ignored.
bool g_can_record_first_input_delay = true;

}  // namespace

// static
bool AshSessionRestorePageLoadMetricsObserver::ShouldBeInstantiated(
    Profile* profile) {
  if (!g_can_record_first_input_delay) {
    return false;
  }
  // Minor optimization (not strictly necessary): Don't bother creating an
  // observer instance if the user's prefs don't allow a full restore to begin
  // with.
  CHECK(profile);
  if (!ash::full_restore::CanPerformRestore(profile->GetPrefs())) {
    g_can_record_first_input_delay = false;
    return false;
  }
  return true;
}

AshSessionRestorePageLoadMetricsObserver::
    AshSessionRestorePageLoadMetricsObserver(content::WebContents* web_contents)
    : web_contents_(web_contents),
      // `SessionRestore::IsRestoring()` be checked immediately here in the
      // constructor while the session restore is still in progress.
      is_web_contents_from_restore_(SessionRestore::IsRestoring(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()))) {}

AshSessionRestorePageLoadMetricsObserver::
    ~AshSessionRestorePageLoadMetricsObserver() = default;

const char* AshSessionRestorePageLoadMetricsObserver::GetObserverName() const {
  return "AshSessionRestorePageLoadMetricsObserver";
}

page_load_metrics::PageLoadMetricsObserverInterface::ObservePolicy
AshSessionRestorePageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return PageLoadMetricsObserverInterface::STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserverInterface::ObservePolicy
AshSessionRestorePageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return PageLoadMetricsObserverInterface::CONTINUE_OBSERVING;
}

void AshSessionRestorePageLoadMetricsObserver::OnFirstInputInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // If multiple browser tabs were restored, multiple
  // `AshSessionRestorePageLoadMetricsObserver`s can exist initially. The
  // first tab that the user interacts with causes this method to be called,
  // after which all future calls to this method should be no-ops.
  if (g_can_record_first_input_delay) {
    TryRecordFirstInputDelay(timing);
    // * If FirstInputDelay did get recorded above, the metric should not be
    //   recorded twice in a session, so the job is done.
    // * If FirstInputDelay did not get recorded above, that means either:
    //   * The first chrome tab that the user interacted with was not a
    //     restored window (ex: a manually opened tab or window).
    //   * Unexpected corner case (ex: the first input recording was somehow not
    //     for the currently active tab).
    //   For both of the above reasons, the spirit of the
    //   "Ash.FullRestore.Browser.FirstInputDelay" metric has been lost as it's
    //   meant to measure the latency between the user's input and when the user
    //   sees the webpage respond for a restored window shortly after login. The
    //   window of opportunity to capture this is gone in these cases, so don't
    //   record for the rest of this user session.
    g_can_record_first_input_delay = false;
  }
}

page_load_metrics::PageLoadMetricsObserverInterface::ObservePolicy
AshSessionRestorePageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  // Minor optimization: Once there's no need to record "FirstInputDelay"
  // anymore, return `STOP_OBSERVING` on the next navigation to prevent observer
  // notifications that we know will just be no-ops for the rest of the
  // WebContents' lifetime.
  return g_can_record_first_input_delay ? ObservePolicy::CONTINUE_OBSERVING
                                        : ObservePolicy::STOP_OBSERVING;
}

void AshSessionRestorePageLoadMetricsObserver::TryRecordFirstInputDelay(
    const page_load_metrics::mojom::PageLoadTiming& timing) const {
  // Only record metric for the active browser's active tab in a session restore
  // (not just a browser window the user manually opened).
  const Browser* const active_browser =
      BrowserList::GetInstance()->GetLastActive();
  if (!active_browser || !is_web_contents_from_restore_) {
    return;
  }
  const content::WebContents* const active_web_contents =
      active_browser->tab_strip_model()->GetActiveWebContents();
  if (!active_web_contents || web_contents_ != active_web_contents) {
    return;
  }
  base::UmaHistogramCustomTimes(
      kFirstInputDelayName,
      timing.interactive_timing->first_input_delay.value(),
      // Parameters taken from `UmaPageLoadMetricsObserver`.
      base::Milliseconds(1), base::Seconds(60), 50);
}
