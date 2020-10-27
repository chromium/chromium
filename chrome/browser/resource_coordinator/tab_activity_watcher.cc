// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_activity_watcher.h"

#include <limits>

#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/tab_metrics_logger.h"
#include "chrome/browser/resource_coordinator/tab_ranker/mru_features.h"
#include "chrome/browser/resource_coordinator/tab_ranker/tab_features.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/input/web_input_event.h"

namespace resource_coordinator {
namespace {
using tab_ranker::TabFeatures;

// Used for decay Frecency scores.
constexpr float kFrecencyScoreDecay = 0.8f;
// Records how many tab reactivations till now.
static int32_t reactivation_index = 0;
// Used for generating label_ids and query_ids.
int64_t internal_id_for_logging = 0;
// Returns an int64_t number as label_id or query_id.
int64_t NewInt64ForLabelIdOrQueryId() {
  // The id is shifted 16 bits so that the lower bits are reserved for counting
  // multiple queries.
  // We choose 16 so that the lower bits for counting multiple queries and
  // higher bits for labeling queries are both unlikely to overflow. (lower bits
  // only overflows when we have more than 65536 queries without labeling
  // events; higher bits only overflow when we have more than 100 billion
  // discards.
  constexpr int kIdShiftBits = 16;
  return (++internal_id_for_logging) << kIdShiftBits;
}

}  // namespace

// Per-WebContents helper class that observes its WebContents, notifying
// TabActivityWatcher when interesting events occur. Also provides
// per-WebContents data that TabActivityWatcher uses to log the tab.
class TabActivityWatcher::WebContentsData
    : public content::WebContentsObserver,
      public content::WebContentsUserData<WebContentsData>,
      public content::RenderWidgetHost::InputEventObserver {
 public:
  ~WebContentsData() override = default;

  // Calculates the tab reactivation score for a background tab. Returns nullopt
  // if the score could not be calculated, e.g. because the tab is in the
  // foreground.
  base::Optional<float> CalculateReactivationScore() {
    if (web_contents()->IsBeingDestroyed() || backgrounded_time_.is_null())
      return base::nullopt;

    // No log for CalculateReactivationScore.
    base::Optional<TabFeatures> tab = GetTabFeatures();
    if (!tab.has_value())
      return base::nullopt;

    float score = 0.0f;
    const tab_ranker::TabRankerResult result =
        TabActivityWatcher::GetInstance()->predictor_->ScoreTab(tab.value(),
                                                                &score);
    if (result == tab_ranker::TabRankerResult::kSuccess)
      return score;
    return base::nullopt;
  }

  // Call when the associated WebContents has been replaced.
  void WasReplaced() { was_replaced_ = true; }

  // Call when the associated WebContents has replaced the WebContents of
  // another tab. Copies info from the other WebContentsData so future events
  // can be logged consistently.
  void DidReplace(const WebContentsData& replaced_tab) {
    // Copy creation and foregrounded times to retain the replaced tab's MRU
    // position.
    creation_time_ = replaced_tab.creation_time_;
    foregrounded_time_ = replaced_tab.foregrounded_time_;

    // Copy background status so ForegroundOrClosed can potentially be logged.
    backgrounded_time_ = replaced_tab.backgrounded_time_;

    // Copy the replaced tab's stats.
    page_metrics_ = replaced_tab.page_metrics_;

    // Recover the ukm_source_id from the |replaced_tab|.
    ukm_source_id_ = replaced_tab.ukm_source_id_;

    // Copy the replaced label_id_.
    label_id_ = replaced_tab.label_id_;

    // Copy the frecency score.
    frecency_score_ = replaced_tab.frecency_score_;
  }

  // Call when the WebContents is detached from its tab. If the tab is later
  // re-inserted elsewhere, we use the state it had before being detached.
  void TabDetached() { is_detached_ = true; }

  // Call when the tab is inserted into a tab strip to update state.
  void TabInserted(bool foreground) {
    if (is_detached_) {
      is_detached_ = false;

      // Dragged tabs are normally inserted into their new tab strip in the
      // "background", then "activated", even though the user perceives the tab
      // staying active the whole time. So don't update |background_time_| here.
      //
      // TODO(michaelpg): If a background tab is dragged (as part of a group)
      // and inserted, it may be treated as being foregrounded (depending on tab
      // order). This is a small edge case, but can be fixed by the plan to
      // merge the ForegroundedOrClosed and TabMetrics events.
      return;
    }

    if (foreground) {
      foregrounded_time_ = NowTicks();
      UpdateFrecencyScoreOnReactivation();
    } else {
      // This is a new tab that was opened in the background.
      backgrounded_time_ = NowTicks();
    }
  }

  // Logs TabMetrics for the tab if it is considered to be backgrounded.
  void LogTabIfBackgrounded() {
    if (backgrounded_time_.is_null() || DisableBackgroundLogWithTabRanker())
      return;

    base::Optional<TabFeatures> tab = GetTabFeatures();
    if (tab.has_value()) {
      // Background time logging always logged with label_id == 0, since we
      // only use label_id for query time logging for now.
      TabActivityWatcher::GetInstance()->tab_metrics_logger_->LogTabMetrics(
          ukm_source_id_, tab.value(), web_contents(), 0);
    }
  }

  // Logs current TabFeatures; skips if current tab is null.
  void LogCurrentTabFeatures(const base::Optional<TabFeatures>& tab) {
    if (!tab.has_value())
      return;
    // Update label_id_: a new label_id is generated for this query if the
    // label_id_ is 0; otherwise the old label_id_ is incremented. This allows
    // us to better pairing TabMetrics with ForegroundedOrClosed events offline.
    // The same label_id_ will be logged with ForegroundedOrClosed event later
    // on so that TabFeatures can be paired with ForegroundedOrClosed.
    label_id_ = label_id_ ? label_id_ + 1 : NewInt64ForLabelIdOrQueryId();

    TabActivityWatcher::GetInstance()->tab_metrics_logger_->LogTabMetrics(
        ukm_source_id_, tab.value(), web_contents(), label_id_);
  }

  // Sets foregrounded_time_ to NowTicks() so this becomes the
  // most-recently-used tab.
  void TabWindowActivated() { foregrounded_time_ = NowTicks(); }

 private:
  friend class content::WebContentsUserData<WebContentsData>;
  friend class TabActivityWatcher;

  // A FrecencyScore is used as a measurement of both frequency and recency.
  // (1) The score is decayed by kFrecencyScoreDecay every time any tab is
  // reactivated.
  // (2) The score is incremented by 1.0 - kFrecencyScoreDecay when this tab is
  // reactivated.
  struct FrecencyScore {
    int32_t update_index = 0;
    float score = 0.0f;
  };

  explicit WebContentsData(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {
    DCHECK(!web_contents->GetBrowserContext()->IsOffTheRecord());
    web_contents->GetMainFrame()
        ->GetRenderViewHost()
        ->GetWidget()
        ->AddInputEventObserver(this);

    creation_time_ = NowTicks();

    // A navigation may already have completed if this is a replacement tab.
    ukm_source_id_ = ukm::GetSourceIdForWebContentsDocument(web_contents);

    // When a tab is discarded, a new null_web_contents will be created (with
    // WasDiscarded set as true) applied as a replacement of the discarded tab.
    // We want to record this discarded state for later logging.
    discarded_since_backgrounded_ = web_contents->WasDiscarded();
  }

  void WasHidden() {
    // The tab may not be in the tabstrip if it's being moved or replaced.
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
    if (!browser)
      return;

    DCHECK(!browser->tab_strip_model()->closing_all());

    if (browser->tab_strip_model()->GetActiveWebContents() == web_contents() &&
        !browser->window()->IsMinimized()) {
      // The active tab is considered to be in the foreground unless its window
      // is minimized. It might still get hidden, e.g. when the browser is about
      // to close, but that shouldn't count as a backgrounded event.
      //
      // TODO(michaelpg): On Mac, hiding the application (e.g. via Cmd+H) should
      // log tabs as backgrounded. Check NSApplication's isHidden property.
      return;
    }

    backgrounded_time_ = NowTicks();
    discarded_since_backgrounded_ = false;
    LogTabIfBackgrounded();
  }

  void WasShown() {
    UpdateFrecencyScoreOnReactivation();

    if (backgrounded_time_.is_null())
      return;

    Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
    if (browser && browser->tab_strip_model()->closing_all())
      return;

    // Log the event before updating times.
    LogForegroundedOrClosedMetrics(true /* is_foregrounded */);

    backgrounded_time_ = base::TimeTicks();
    foregrounded_time_ = NowTicks();

    page_metrics_.num_reactivations++;
  }

  // content::WebContentsObserver:
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override {
    if (old_host != nullptr)
      old_host->GetWidget()->RemoveInputEventObserver(this);
    new_host->GetWidget()->AddInputEventObserver(this);
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->HasCommitted() ||
        !navigation_handle->IsInMainFrame() ||
        navigation_handle->IsSameDocument()) {
      return;
    }

    // Use the same SourceId that SourceUrlRecorderWebContentsObserver populates
    // and updates.
    ukm::SourceId new_source_id = ukm::ConvertToSourceId(
        navigation_handle->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
    DCHECK_NE(new_source_id, ukm_source_id_)
        << "Expected a unique Source ID for the navigation";
    ukm_source_id_ = new_source_id;

    // Reset the per-page data.
    page_metrics_ = {};

    // Update navigation info.
    page_metrics_.page_transition = navigation_handle->GetPageTransition();
  }

  // Logs metrics for the tab when it stops loading instead of immediately
  // after a navigation commits, so we can have some idea of its status and
  // contents.
  void DidStopLoading() override {
    // Ignore load events in foreground tabs. The tab state of a foreground tab
    // will be logged if/when it is backgrounded.
    LogTabIfBackgrounded();
  }

  void OnVisibilityChanged(content::Visibility visibility) override {
    // Record background tab UKMs and do associated bookkepping.
    if (!web_contents()->IsBeingDestroyed()) {
      // TODO(michaelpg): Consider treating occluded tabs as hidden.
      if (visibility == content::Visibility::HIDDEN) {
        WasHidden();
      } else {
        WasShown();
      }
    }
  }

  void WebContentsDestroyed() override {
    if (was_replaced_)
      return;

    // Log necessary metrics.
    TabActivityWatcher::GetInstance()->OnTabClosed(this);
  }

  // content::RenderWidgetHost::InputEventObserver:
  void OnInputEvent(const blink::WebInputEvent& event) override {
    if (blink::WebInputEvent::IsMouseEventType(event.GetType()))
      page_metrics_.mouse_event_count++;
    else if (blink::WebInputEvent::IsKeyboardEventType(event.GetType()))
      page_metrics_.key_event_count++;
    else if (blink::WebInputEvent::IsTouchEventType(event.GetType()))
      page_metrics_.touch_event_count++;
  }

  // Iterates through tabstrips to determine the index of |contents| in
  // most-recently-used order out of all non-incognito tabs.
  // Linear in the number of tabs (most users have <10 tabs open).
  tab_ranker::MRUFeatures GetMRUFeatures() {
    // If not in closing_all mode, calculate |mru_features_|.
    mru_features_.index = 0;
    mru_features_.total = 0;
    for (Browser* browser : *BrowserList::GetInstance()) {
      // Ignore incognito browsers.
      if (browser->profile()->IsOffTheRecord())
        continue;

      int count = browser->tab_strip_model()->count();
      mru_features_.total += count;

      // Increment the MRU index for each WebContents that was foregrounded more
      // recently than this one.
      for (int i = 0; i < count; i++) {
        auto* other = WebContentsData::FromWebContents(
            browser->tab_strip_model()->GetWebContentsAt(i));
        if (!other || this == other)
          continue;

        if (!MoreRecentlyUsed(this, other))
          mru_features_.index++;
      }
    }
    return mru_features_;
  }

  // Returns whether |webcontents_a| is more recently used than |webcontents_b|.
  // A webcontents is more recently used iff it has larger (later)
  // |foregrounded_time_|; or |creation_time_| if they were never foregrounded.
  static bool MoreRecentlyUsed(
      TabActivityWatcher::WebContentsData* webcontents_a,
      TabActivityWatcher::WebContentsData* const webcontents_b) {
    return webcontents_a->foregrounded_time_ >
               webcontents_b->foregrounded_time_ ||
           (webcontents_a->foregrounded_time_ ==
                webcontents_b->foregrounded_time_ &&
            webcontents_a->creation_time_ > webcontents_b->creation_time_);
  }

  // Returns the tabfeatures of current tab by combining TabMetrics,
  // WindowFeatures and MRUFeatures.
  // TODO(charleszhao): refactor TabMetricsLogger::GetTabFeatures to return a
  // full TabFeatures instead of a partial TabFeatures.
  base::Optional<TabFeatures> GetTabFeatures() {
    if (web_contents()->IsBeingDestroyed() || backgrounded_time_.is_null())
      return base::nullopt;
    // For tab features.
    base::Optional<TabFeatures> tab =
        TabMetricsLogger::GetTabFeatures(page_metrics_, web_contents());
    if (!tab.has_value())
      return tab;

    tab->time_from_backgrounded =
        backgrounded_time_.is_null()
            ? 0
            : (NowTicks() - backgrounded_time_).InMilliseconds();

    // For mru features.
    const tab_ranker::MRUFeatures& mru = GetMRUFeatures();
    tab->mru_index = mru.index;
    tab->total_tab_count = mru.total;

    // For frecency_score;
    tab->frecency_score = GetFrecencyScore();

    return tab;
  }

  // Collect current ForegroundedOrClosedMetrics and send to ukm.
  void LogForegroundedOrClosedMetrics(bool is_foregrounded) {
    // If background time logging is disabled, then we only log the case where
    // the label_id_ != 0 (a feature is logged and a label has not been logged).
    if (DisableBackgroundLogWithTabRanker() && label_id_ == 0)
      return;

    TabMetricsLogger::ForegroundedOrClosedMetrics metrics;
    metrics.is_foregrounded = is_foregrounded;
    metrics.is_discarded = discarded_since_backgrounded_;
    metrics.time_from_backgrounded =
        (NowTicks() - backgrounded_time_).InMilliseconds();
    metrics.label_id = label_id_;

    TabActivityWatcher::GetInstance()
        ->tab_metrics_logger_->LogForegroundedOrClosedMetrics(ukm_source_id_,
                                                              metrics);
    // label_id_ is reset whenever a label is logged.
    // A new label_id_ is generated when a query happens inside
    // CalculateReactivationScore, after that this ForegroundedOrClosed logging
    // can happen many times (tabs may get backgrounded and reactivated several
    // times). In such cases, we only count the first time as the true label,
    // the rest are considered to be query time logging irrelevant, for which we
    // log with label_id == 0.
    label_id_ = 0;
  }

  // Returns frecency score of this tab.
  // NOTE: we don't apply decay for all reactivations, instead we accumulate
  // them as reactivations_since_last_update and applied all together when the
  // score is queried.
  float GetFrecencyScore() {
    const int reactivations_since_last_update =
        reactivation_index - frecency_score_.update_index;
    if (reactivations_since_last_update > 0) {
      frecency_score_.score *=
          std::pow(kFrecencyScoreDecay, reactivations_since_last_update);
      frecency_score_.update_index = reactivation_index;
    }
    return frecency_score_.score;
  }

  // Updates frecency score of current tab when it is reactivated.
  void UpdateFrecencyScoreOnReactivation() {
    ++reactivation_index;
    // Updates the current score.
    frecency_score_.score = GetFrecencyScore() + 1.0f - kFrecencyScoreDecay;
  }

  // Updated when a navigation is finished.
  ukm::SourceId ukm_source_id_ = 0;

  // When the tab was created.
  base::TimeTicks creation_time_;

  // The most recent time the tab became backgrounded. This happens when a
  // different tab in the tabstrip is activated or the tab's window is hidden.
  base::TimeTicks backgrounded_time_;

  // The most recent time the tab became foregrounded. This happens when the
  // tab becomes the active tab in the tabstrip or when the active tab's window
  // is activated.
  base::TimeTicks foregrounded_time_;

  // Stores current page stats for the tab.
  TabMetricsLogger::PageMetrics page_metrics_;

  // Set to true when the WebContents has been detached from its tab.
  bool is_detached_ = false;

  // If true, future events such as the tab being destroyed won't be logged.
  bool was_replaced_ = false;

  // MRUFeatures of this WebContents, updated only before ForegroundedOrClosed
  // event is logged.
  tab_ranker::MRUFeatures mru_features_;

  // Whether this tab is currently in discarded state.
  bool discarded_since_backgrounded_ = false;

  // An int64 random label to pair TabFeatures with ForegroundedOrClosed event.
  int64_t label_id_ = 0;

  // Fecency score of this tab.
  FrecencyScore frecency_score_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(WebContentsData);
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabActivityWatcher::WebContentsData)

TabActivityWatcher::TabActivityWatcher()
    : tab_metrics_logger_(std::make_unique<TabMetricsLogger>()),
      browser_tab_strip_tracker_(this, this),
      predictor_(std::make_unique<tab_ranker::TabScorePredictor>()) {
  BrowserList::AddObserver(this);
  browser_tab_strip_tracker_.Init();
}

TabActivityWatcher::~TabActivityWatcher() {
  BrowserList::RemoveObserver(this);
}

base::Optional<float> TabActivityWatcher::CalculateReactivationScore(
    content::WebContents* web_contents) {
  WebContentsData* web_contents_data =
      WebContentsData::FromWebContents(web_contents);
  if (!web_contents_data)
    return base::nullopt;
  return web_contents_data->CalculateReactivationScore();
}

void TabActivityWatcher::LogAndMaybeSortLifecycleUnitWithTabRanker(
    std::vector<LifecycleUnit*>* tabs) {
  // Set query_id so that all TabFeatures logged in this query can be joined.
  tab_metrics_logger_->set_query_id(NewInt64ForLabelIdOrQueryId());

  const bool should_sort_tabs =
      base::FeatureList::IsEnabled(features::kTabRanker);

  std::map<int32_t, base::Optional<TabFeatures>> tab_features;
  for (auto* lifecycle_unit : *tabs) {
    auto* lifecycle_unit_external =
        lifecycle_unit->AsTabLifecycleUnitExternal();
    // the lifecycle_unit_external is nullptr in the unit test
    // TabManagerDelegateTest::KillMultipleProcesses.
    if (!lifecycle_unit_external) {
      tab_features[lifecycle_unit->GetID()] = base::nullopt;
      continue;
    }
    WebContentsData* web_contents_data = WebContentsData::FromWebContents(
        lifecycle_unit_external->GetWebContents());

    // The web_contents_data can be nullptr in some cases.
    // TODO(crbug.com/1019482): move the creation of WebContentsData to
    // TabHelpers::AttachTabHelpers.
    if (!web_contents_data) {
      tab_features[lifecycle_unit->GetID()] = base::nullopt;
      continue;
    }

    const base::Optional<TabFeatures> tab = web_contents_data->GetTabFeatures();
    web_contents_data->LogCurrentTabFeatures(tab);

    // No reason to store TabFeatures if TabRanker is disabled.
    if (should_sort_tabs) {
      tab_features[lifecycle_unit->GetID()] = tab;
    }
  }

  // Directly return if TabRanker is disabled.
  if (!should_sort_tabs)
    return;

  const std::map<int32_t, float> reactivation_scores =
      predictor_->ScoreTabs(tab_features);
  // Sort with larger reactivation_score first (desending importance).
  std::sort(tabs->begin(), tabs->end(),
            [&reactivation_scores](const LifecycleUnit* const a,
                                   const LifecycleUnit* const b) {
              return reactivation_scores.at(a->GetID()) >
                     reactivation_scores.at(b->GetID());
            });
}

void TabActivityWatcher::OnBrowserSetLastActive(Browser* browser) {
  if (browser->tab_strip_model()->closing_all())
    return;

  content::WebContents* active_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!active_contents)
    return;

  // Don't assume the WebContentsData already exists in case activation happens
  // before the tabstrip is fully updated.
  WebContentsData* web_contents_data =
      WebContentsData::FromWebContents(active_contents);
  if (web_contents_data)
    web_contents_data->TabWindowActivated();
}

void TabActivityWatcher::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  switch (change.type()) {
    case TabStripModelChange::kInserted: {
      for (const auto& contents : change.GetInsert()->contents) {
        // Ensure the WebContentsData is created to observe this WebContents
        // since it may represent a newly created tab.
        WebContentsData::CreateForWebContents(contents.contents);
        WebContentsData::FromWebContents(contents.contents)
            ->TabInserted(selection.new_contents == contents.contents);
      }
      break;
    }
    case TabStripModelChange::kRemoved: {
      for (const auto& contents : change.GetRemove()->contents)
        WebContentsData::FromWebContents(contents.contents)->TabDetached();
      break;
    }
    case TabStripModelChange::kReplaced: {
      auto* replace = change.GetReplace();
      WebContentsData* old_web_contents_data =
          WebContentsData::FromWebContents(replace->old_contents);
      old_web_contents_data->WasReplaced();

      // Ensure the WebContentsData is created to observe this WebContents
      // since it likely hasn't been inserted into a tabstrip before.
      WebContentsData::CreateForWebContents(replace->new_contents);

      WebContentsData::FromWebContents(replace->new_contents)
          ->DidReplace(*old_web_contents_data);
      break;
    }
    case TabStripModelChange::kMoved:
    case TabStripModelChange::kSelectionOnly:
      break;
  }
}

void TabActivityWatcher::TabPinnedStateChanged(TabStripModel* tab_strip_model,
                                               content::WebContents* contents,
                                               int index) {
  WebContentsData::FromWebContents(contents)->LogTabIfBackgrounded();
}

bool TabActivityWatcher::ShouldTrackBrowser(Browser* browser) {
  // Don't track incognito browsers. This is also enforced by UKM.
  // TODO(michaelpg): Keep counters for incognito browsers so we can score them
  // using the TabScorePredictor. We should be able to do this without logging
  // these values.
  return !browser->profile()->IsOffTheRecord();
}

void TabActivityWatcher::ResetForTesting() {
  tab_metrics_logger_ = std::make_unique<TabMetricsLogger>();
  predictor_ = std::make_unique<tab_ranker::TabScorePredictor>();
  internal_id_for_logging = 0;
}

// static
TabActivityWatcher* TabActivityWatcher::GetInstance() {
  static base::NoDestructor<TabActivityWatcher> instance;
  return instance.get();
}

void TabActivityWatcher::OnTabClosed(WebContentsData* web_contents_data) {
  // Log ForegroundedOrClosed event.
  if (!web_contents_data->backgrounded_time_.is_null()) {
    web_contents_data->LogForegroundedOrClosedMetrics(
        false /*is_foregrounded */);
  }
}

}  // namespace resource_coordinator
