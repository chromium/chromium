// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_metrics_logger.h"

#include <algorithm>
#include <string>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/tab_metrics_event.pb.h"
#include "chrome/browser/resource_coordinator/tab_ranker/mru_features.h"
#include "chrome/browser/resource_coordinator/tab_ranker/tab_features.h"
#include "chrome/browser/resource_coordinator/tab_ranker/window_features.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/tab_contents/form_interaction_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/mime_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/mojom/frame/sudden_termination_disabler_type.mojom.h"
#include "url/gurl.h"

using metrics::TabMetricsEvent;
using metrics::WindowMetricsEvent;

namespace {

// Returns the number of discards that happened to |contents|.
int GetDiscardCount(content::WebContents* contents) {
  auto* external =
      resource_coordinator::TabLifecycleUnitExternal::FromWebContents(contents);
  DCHECK(external);
  return external->GetDiscardCount();
}

// Populates navigation-related metrics.
void PopulatePageTransitionFeatures(ui::PageTransition page_transition,
                                    tab_ranker::TabFeatures* tab) {
  // We only report the following core types.
  // Note: Redirects unrelated to clicking a link still get the "link" type.
  if (ui::PageTransitionCoreTypeIs(page_transition, ui::PAGE_TRANSITION_LINK) ||
      ui::PageTransitionCoreTypeIs(page_transition,
                                   ui::PAGE_TRANSITION_AUTO_BOOKMARK) ||
      ui::PageTransitionCoreTypeIs(page_transition,
                                   ui::PAGE_TRANSITION_FORM_SUBMIT) ||
      ui::PageTransitionCoreTypeIs(page_transition,
                                   ui::PAGE_TRANSITION_RELOAD)) {
    tab->page_transition_core_type =
        ui::PageTransitionStripQualifier(page_transition);
  }

  tab->page_transition_from_address_bar =
      (page_transition & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) != 0;
  tab->page_transition_is_redirect =
      ui::PageTransitionIsRedirect(page_transition);
}

// Populates TabFeatures from |page_metrics|.
void PopulateTabFeaturesFromPageMetrics(
    const TabMetricsLogger::PageMetrics& page_metrics,
    tab_ranker::TabFeatures* tab) {
  static_assert(sizeof(TabMetricsLogger::PageMetrics) ==
                    sizeof(int) * 4 + sizeof(ui::PageTransition),
                "Make sure all fields in PageMetrics are considered here.");
  tab->key_event_count = page_metrics.key_event_count;
  tab->mouse_event_count = page_metrics.mouse_event_count;
  tab->num_reactivations = page_metrics.num_reactivations;
  tab->touch_event_count = page_metrics.touch_event_count;

  PopulatePageTransitionFeatures(page_metrics.page_transition, tab);
}

// Populates TabFeatures that can be calculated simply from |web_contents|.
void PopulateTabFeaturesFromWebContents(content::WebContents* web_contents,
                                        tab_ranker::TabFeatures* tab_features) {
  tab_features->has_before_unload_handler =
      web_contents->GetPrimaryMainFrame()->GetSuddenTerminationDisablerState(
          blink::mojom::SuddenTerminationDisablerType::kBeforeUnloadHandler);
  tab_features->has_form_entry =
      FormInteractionTabHelper::FromWebContents(web_contents)
          ->had_form_interaction();
  tab_features->host = web_contents->GetLastCommittedURL().host();
  tab_features->navigation_entry_count =
      web_contents->GetController().GetEntryCount();

  if (site_engagement::SiteEngagementService::IsEnabled()) {
    tab_features->site_engagement_score =
        TabMetricsLogger::GetSiteEngagementScore(web_contents);
  }

  // This checks if the tab was audible within the past two seconds, same as the
  // audio indicator in the tab strip.
  tab_features->was_recently_audible =
      RecentlyAudibleHelper::FromWebContents(web_contents)
          ->WasRecentlyAudible();

  tab_features->discard_count = GetDiscardCount(web_contents);
}

// Populates TabFeatures that calculated from |browser| including WindowMetrics
// and PinState.
void PopulateTabFeaturesFromBrowser(const Browser* browser,
                                    content::WebContents* web_contents,
                                    tab_ranker::TabFeatures* tab_features) {
  // For pin state.
  const TabStripModel* tab_strip_model = browser->tab_strip_model();
  int index = tab_strip_model->GetIndexOfWebContents(web_contents);
  DCHECK_NE(index, TabStripModel::kNoTab);
  tab_features->is_pinned = tab_strip_model->IsTabPinned(index);

  // For window features.
  tab_ranker::WindowFeatures window =
      TabMetricsLogger::CreateWindowFeatures(browser);
  tab_features->window_is_active = window.is_active;
  tab_features->window_show_state = window.show_state;
  tab_features->window_tab_count = window.tab_count;
  tab_features->window_type = window.type;
}

}  // namespace

TabMetricsLogger::TabMetricsLogger() = default;
TabMetricsLogger::~TabMetricsLogger() = default;

// static
int TabMetricsLogger::GetSiteEngagementScore(
    content::WebContents* web_contents) {
  if (!site_engagement::SiteEngagementService::IsEnabled())
    return -1;

  site_engagement::SiteEngagementService* service =
      site_engagement::SiteEngagementService::Get(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  DCHECK(service);

  // Scores range from 0 to 100. Round down to a multiple of 10 to conform to
  // privacy guidelines.
  double raw_score = service->GetScore(web_contents->GetVisibleURL());
  int rounded_score = static_cast<int>(raw_score / 10) * 10;
  DCHECK_LE(0, rounded_score);
  DCHECK_GE(100, rounded_score);
  return rounded_score;
}

// static
absl::optional<tab_ranker::TabFeatures> TabMetricsLogger::GetTabFeatures(
    const PageMetrics& page_metrics,
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return absl::nullopt;

  tab_ranker::TabFeatures tab;
  PopulateTabFeaturesFromWebContents(web_contents, &tab);
  PopulateTabFeaturesFromBrowser(browser, web_contents, &tab);
  PopulateTabFeaturesFromPageMetrics(page_metrics, &tab);

  return tab;
}

void TabMetricsLogger::LogTabMetrics(
    ukm::SourceId ukm_source_id,
    const tab_ranker::TabFeatures& tab_features,
    content::WebContents* web_contents,
    int64_t label_id) {
  if (!ukm_source_id)
    return;

  if (web_contents) {
    // UKM recording is disabled in OTR.
    if (web_contents->GetBrowserContext()->IsOffTheRecord())
      return;

    // Verify that the browser is not closing.
    const Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
    if (base::Contains(BrowserList::GetInstance()->currently_closing_browsers(),
                       browser)) {
      return;
    }

    const TabStripModel* tab_strip_model = browser->tab_strip_model();
    if (tab_strip_model->closing_all())
      return;
  }

  ukm::builders::TabManager_TabMetrics entry(ukm_source_id);
  PopulateTabFeaturesToUkmEntry(tab_features, &entry);
  entry.SetLabelId(label_id);
  entry.SetQueryId(query_id_);
  entry.Record(ukm::UkmRecorder::Get());
}

void TabMetricsLogger::LogForegroundedOrClosedMetrics(
    ukm::SourceId ukm_source_id,
    const ForegroundedOrClosedMetrics& metrics) {
  if (!ukm_source_id)
    return;

  ukm::builders::TabManager_Background_ForegroundedOrClosed(ukm_source_id)
      .SetLabelId(metrics.label_id)
      .SetIsForegrounded(metrics.is_foregrounded)
      .SetTimeFromBackgrounded(metrics.time_from_backgrounded)
      .SetIsDiscarded(metrics.is_discarded)
      .Record(ukm::UkmRecorder::Get());
}

void TabMetricsLogger::LogTabLifetime(ukm::SourceId ukm_source_id,
                                      base::TimeDelta time_since_navigation) {
  if (!ukm_source_id)
    return;
  ukm::builders::TabManager_TabLifetime(ukm_source_id)
      .SetTimeSinceNavigation(time_since_navigation.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}

// static
tab_ranker::WindowFeatures TabMetricsLogger::CreateWindowFeatures(
    const Browser* browser) {
  DCHECK(browser->window());

  WindowMetricsEvent::Type window_type = WindowMetricsEvent::TYPE_UNKNOWN;
  switch (browser->type()) {
    case Browser::TYPE_NORMAL:
      window_type = WindowMetricsEvent::TYPE_TABBED;
      break;
    case Browser::TYPE_POPUP:
      window_type = WindowMetricsEvent::TYPE_POPUP;
      break;
    case Browser::TYPE_APP:
    case Browser::TYPE_APP_POPUP:
      window_type = WindowMetricsEvent::TYPE_APP;
      break;
    case Browser::TYPE_DEVTOOLS:
      window_type = WindowMetricsEvent::TYPE_APP;
      break;
    default:
      NOTREACHED();
  }

  WindowMetricsEvent::ShowState show_state =
      WindowMetricsEvent::SHOW_STATE_UNKNOWN;
  if (browser->window()->IsFullscreen())
    show_state = WindowMetricsEvent::SHOW_STATE_FULLSCREEN;
  else if (browser->window()->IsMinimized())
    show_state = WindowMetricsEvent::SHOW_STATE_MINIMIZED;
  else if (browser->window()->IsMaximized())
    show_state = WindowMetricsEvent::SHOW_STATE_MAXIMIZED;
  else
    show_state = WindowMetricsEvent::SHOW_STATE_NORMAL;

  const bool is_active = browser->window()->IsActive();
  const int tab_count = browser->tab_strip_model()->count();

  return {window_type, show_state, is_active, tab_count};
}
