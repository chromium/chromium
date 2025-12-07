// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/byte_count.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process_metrics.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/tab_contents/form_interaction_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "components/device_event_log/device_event_log.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/mojom/lifecycle.mojom.h"
#include "components/permissions/permission_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "url/gurl.h"

namespace resource_coordinator {

TabLifecycleUnitSource::TabLifecycleUnit::TabLifecycleUnit(
    TabLifecycleUnitSource* source,
    content::WebContents* web_contents,
    TabStripModel* tab_strip_model)
    : LifecycleUnitBase(source),
      content::WebContentsObserver(web_contents),
      tab_strip_model_(tab_strip_model),
      wall_time_when_hidden_(web_contents->GetVisibility() ==
                                     content::Visibility::VISIBLE
                                 ? base::TimeTicks::Max()
                                 : NowTicks()) {
  DCHECK(web_contents);
  DCHECK(tab_strip_model_);

  // Attach the ResourceCoordinatorTabHelper. In production code this has
  // already been attached by now due to AttachTabHelpers, but there's a long
  // tail of tests that don't add these helpers. This ensures that the various
  // DCHECKs in the state transition machinery don't fail.
  ResourceCoordinatorTabHelper::CreateForWebContents(web_contents);

  // Visible tabs are treated as having been immediately focused, while
  // non-visible tabs have their focus set to the last active time (the time at
  // which they stopped being the active tab in a tabstrip).
  if (web_contents->GetVisibility() == content::Visibility::VISIBLE) {
    last_focused_time_ticks_ = NowTicks();
    last_focused_time_ = Now();
  } else {
    last_focused_time_ticks_ = web_contents->GetLastActiveTimeTicks();
    last_focused_time_ = web_contents->GetLastActiveTime();
  }
}

TabLifecycleUnitSource::TabLifecycleUnit::~TabLifecycleUnit() {
  OnLifecycleUnitDestroyed();
}

void TabLifecycleUnitSource::TabLifecycleUnit::SetTabStripModel(
    TabStripModel* tab_strip_model) {
  tab_strip_model_ = tab_strip_model;
}

void TabLifecycleUnitSource::TabLifecycleUnit::SetWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  Observe(web_contents);
}

void TabLifecycleUnitSource::TabLifecycleUnit::SetFocused(bool focused) {
  const bool was_focused = last_focused_time_ticks_ == base::TimeTicks::Max();

  if (focused == was_focused) {
    return;
  }

  last_focused_time_ticks_ = focused ? base::TimeTicks::Max() : NowTicks();
  last_focused_time_ = focused ? base::Time::Max() : Now();

  if (!focused) {
    return;
  }

  bool success = MaybeLoad();
  if (success) {
    web_contents()->Focus();
  }
}

bool TabLifecycleUnitSource::TabLifecycleUnit::MaybeLoad() {
  if (is_discarded_) {
    // Transition to the active state.
    is_discarded_ = false;
    RecomputeLifecycleUnitState();

    // Load the tab if it's discarded. It will typically be discarded, but
    // might not be if this is invoked as part of reloading the tab explicitly
    // and we haven't been notified of the ongoing load yet
    // (crbug.com/40075246).
    //
    // With "WebContentsDiscard", loading on activation occurs from:
    //     content::NavigationControllerImpl::SetActive
    //     content::WebContentsImpl::UpdateVisibilityAndNotifyPageAndView
    //     content::WebContentsImpl::UpdateWebContentsVisibility
    // With `content::NavigationControllerImpl::needs_reload_` having been
    // set from `content::FrameTree::Discard`. So it is undesired to trigger
    // it explicitly from here.
    if (web_contents()->WasDiscarded() &&
        !base::FeatureList::IsEnabled(features::kWebContentsDiscard)) {
      CHECK(Load());
      return true;
    }
  }

  return false;
}

void TabLifecycleUnitSource::TabLifecycleUnit::SetRecentlyAudible(
    bool recently_audible) {
  if (recently_audible)
    recently_audible_time_ = base::TimeTicks::Max();
  else if (recently_audible_time_ == base::TimeTicks::Max())
    recently_audible_time_ = NowTicks();
}

void TabLifecycleUnitSource::TabLifecycleUnit::UpdateLifecycleState(
    performance_manager::mojom::LifecycleState state) {
  page_lifecycle_state_ = state;
  RecomputeLifecycleUnitState();
}

TabLifecycleUnitExternal*
TabLifecycleUnitSource::TabLifecycleUnit::AsTabLifecycleUnitExternal() {
  return this;
}

base::TimeTicks
TabLifecycleUnitSource::TabLifecycleUnit::GetLastFocusedTimeTicks() const {
  return last_focused_time_ticks_;
}

base::Time TabLifecycleUnitSource::TabLifecycleUnit::GetLastFocusedTime()
    const {
  return last_focused_time_;
}

LifecycleUnitLoadingState
TabLifecycleUnitSource::TabLifecycleUnit::GetLoadingState() const {
  return TabLoadTracker::Get()->GetLoadingState(web_contents());
}

bool TabLifecycleUnitSource::TabLifecycleUnit::Load() {
  if (GetLoadingState() != LifecycleUnitLoadingState::UNLOADED) {
    return false;
  }

  // TODO(chrisha): Make this work more elegantly in the case of background tab
  // loading as well, which uses a NavigationThrottle that can be released.

  // See comment in Discard() for an explanation of why "needs reload" is
  // false when a tab is discarded.
  // TODO(fdoray): Remove NavigationControllerImpl::needs_reload_ once
  // session restore is handled by LifecycleManager.
  web_contents()->GetController().SetNeedsReload();
  web_contents()->GetController().LoadIfNecessary();
  return true;
}

bool TabLifecycleUnitSource::TabLifecycleUnit::IsAutoDiscardable() const {
  return auto_discardable_;
}

void TabLifecycleUnitSource::TabLifecycleUnit::SetAutoDiscardable(
    bool auto_discardable) {
  if (auto_discardable_ == auto_discardable) {
    return;
  }
  auto_discardable_ = auto_discardable;

  performance_manager::PageLiveStateDecorator::SetIsAutoDiscardable(
      web_contents(), auto_discardable_);
}

void TabLifecycleUnitSource::TabLifecycleUnit::FinishDiscard(
    LifecycleUnitDiscardReason discard_reason,
    uint64_t tab_memory_footprint_estimate) {
  content::WebContents* const old_contents = web_contents();
  content::WebContents::CreateParams create_params(tab_strip_model_->profile());
  // TODO(fdoray): Consider setting |initially_hidden| to true when the tab is
  // OCCLUDED. Will require checking that the tab reload correctly when it
  // becomes VISIBLE.
  create_params.initially_hidden =
      old_contents->GetVisibility() == content::Visibility::HIDDEN;
  create_params.desired_renderer_state =
      content::WebContents::CreateParams::kNoRendererProcess;
  create_params.last_active_time = old_contents->GetLastActiveTime();
  create_params.last_active_time_ticks = old_contents->GetLastActiveTimeTicks();
  std::unique_ptr<content::WebContents> null_contents =
      content::WebContents::Create(create_params);
  content::WebContents* raw_null_contents = null_contents.get();

  UpdatePreDiscardResourceUsage(raw_null_contents, discard_reason,
                                tab_memory_footprint_estimate);

  // Attach the ResourceCoordinatorTabHelper. In production code this has
  // already been attached by now due to AttachTabHelpers, but there's a long
  // tail of tests that don't add these helpers. This ensures that the various
  // DCHECKs in the state transition machinery don't fail.
  ResourceCoordinatorTabHelper::CreateForWebContents(raw_null_contents);

  // Send the notification to WebContentsObservers that the old content is about
  // to be discarded and replaced with `null_contents`.
  old_contents->AboutToBeDiscarded(null_contents.get());

  // Copy over the state from the navigation controller to preserve the
  // back/forward history and to continue to display the correct title/favicon.
  //
  // Set |needs_reload| to false so that the tab is not automatically reloaded
  // when activated. If it was true, there would be an immediate reload when the
  // active tab of a non-visible window is discarded. SetFocused() will take
  // care of reloading the tab when it becomes active in a focused window.
  null_contents->GetController().CopyStateFrom(&old_contents->GetController(),
                                               /* needs_reload */ false);

  AttemptFastKillForDiscard(old_contents, discard_reason);

  // Replace the discarded tab with the null version.
  const int index = tab_strip_model_->GetIndexOfWebContents(old_contents);
  DCHECK_NE(index, TabStripModel::kNoTab);

  // This ensures that on reload after discard, the document has
  // "WasDiscarded" set to true.
  // The "WasDiscarded" state is also sent to tab_strip_model.
  null_contents->SetWasDiscarded(true);

  std::unique_ptr<content::WebContents> old_contents_deleter =
      tab_strip_model_->DiscardWebContentsAt(index, std::move(null_contents));
  DCHECK_EQ(web_contents(), raw_null_contents);

  // Discard the old tab's renderer.
  // TODO(jamescook): This breaks script connections with other tabs. Find a
  // different approach that doesn't do that, perhaps based on
  // RenderFrameProxyHosts.
  old_contents_deleter.reset();

  is_discarded_ = true;
  RecomputeLifecycleUnitState();
  DCHECK_EQ(GetLoadingState(), LifecycleUnitLoadingState::UNLOADED);

  web_contents()->NotifyWasDiscarded();
}

void TabLifecycleUnitSource::TabLifecycleUnit::
    FinishDiscardAndPreserveWebContents(
        LifecycleUnitDiscardReason discard_reason,
        uint64_t tab_memory_footprint_estimate,
        const base::TimeTicks discard_start_time) {
  UpdatePreDiscardResourceUsage(web_contents(), discard_reason,
                                tab_memory_footprint_estimate);

  AttemptFastKillForDiscard(web_contents(), discard_reason);

  web_contents()->Discard(base::BindOnce(
      [](const base::TimeTicks start_time) {
        base::UmaHistogramTimes("Discarding.TabLifecycleUnit.DiscardLatency",
                                NowTicks() - start_time);
      },
      discard_start_time));
  tab_strip_model_->UpdateWebContentsStateAt(
      tab_strip_model_->GetIndexOfWebContents(web_contents()),
      TabChangeType::kAll);

  is_discarded_ = true;
  RecomputeLifecycleUnitState();
}

bool TabLifecycleUnitSource::TabLifecycleUnit::Discard(
    LifecycleUnitDiscardReason reason,
    uint64_t tab_memory_footprint_estimate) {
  const base::TimeTicks discard_start_time = NowTicks();
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(DiscardTabOutcome)
  enum class DiscardTabOutcome {
    kSuccess = 0,
    kNotInTabStripModel = 1,
    kAlreadyDiscarded = 2,
    kMaxValue = kAlreadyDiscarded
  };
  // LINT.ThenChange(/tools/metrics/histograms/metadata/tab/enums.xml:DiscardTabOutcome)

  std::optional<DiscardTabOutcome> outcome;
  absl::Cleanup record_discard_outcome = [&]() {
    CHECK(outcome.has_value());
    base::UmaHistogramEnumeration("Discarding.DiscardTabOutcome",
                                  outcome.value());
  };

  // Can't discard a tab when it isn't in a tabstrip.
  if (!tab_strip_model_) {
    // Logs are used to diagnose user feedback reports.
    MEMORY_LOG(ERROR) << "Skipped discarding unit " << GetID()
                      << " because it isn't in a tab strip.";
    outcome = DiscardTabOutcome::kNotInTabStripModel;
    return false;
  }

  if (is_discarded_) {
    // Logs are used to diagnose user feedback reports.
    MEMORY_LOG(ERROR) << "Skipped discarding unit " << GetID()
                      << " because it's already discarded.";
    outcome = DiscardTabOutcome::kAlreadyDiscarded;
    return false;
  }

  discard_reason_ = reason;

  if (base::FeatureList::IsEnabled(features::kWebContentsDiscard)) {
    FinishDiscardAndPreserveWebContents(reason, tab_memory_footprint_estimate,
                                        discard_start_time);
  } else {
    FinishDiscard(reason, tab_memory_footprint_estimate);
    base::UmaHistogramTimes("Discarding.TabLifecycleUnit.DiscardLatency",
                            NowTicks() - discard_start_time);
  }

  outcome = DiscardTabOutcome::kSuccess;
  return true;
}

content::WebContents* TabLifecycleUnitSource::TabLifecycleUnit::GetWebContents()
    const {
  return web_contents();
}

bool TabLifecycleUnitSource::TabLifecycleUnit::DiscardTab(
    mojom::LifecycleUnitDiscardReason reason,
    uint64_t memory_footprint_estimate) {
  return Discard(reason, memory_footprint_estimate);
}

mojom::LifecycleUnitState
TabLifecycleUnitSource::TabLifecycleUnit::GetTabState() const {
  return GetState();
}

void TabLifecycleUnitSource::TabLifecycleUnit::RecomputeLifecycleUnitState() {
  performance_manager::PageLiveStateDecorator::SetIsDiscarded(web_contents(),
                                                              is_discarded_);
  if (is_discarded_) {
    SetState(mojom::LifecycleUnitState::DISCARDED);
  } else if (page_lifecycle_state_ ==
             performance_manager::mojom::LifecycleState::kFrozen) {
    SetState(mojom::LifecycleUnitState::FROZEN);
  } else {
    SetState(mojom::LifecycleUnitState::ACTIVE);
  }
}

TabLifecycleUnitSource* TabLifecycleUnitSource::TabLifecycleUnit::GetTabSource()
    const {
  return static_cast<TabLifecycleUnitSource*>(GetSource());
}

void TabLifecycleUnitSource::TabLifecycleUnit::UpdatePreDiscardResourceUsage(
    content::WebContents* web_contents,
    LifecycleUnitDiscardReason discard_reason,
    uint64_t tab_memory_footprint_estimate) {
  auto* const pre_discard_resource_usage =
      performance_manager::user_tuning::UserPerformanceTuningManager::
          PreDiscardResourceUsage::PreDiscardResourceUsage::FromWebContents(
              web_contents);
  if (pre_discard_resource_usage == nullptr) {
    performance_manager::user_tuning::UserPerformanceTuningManager::
        PreDiscardResourceUsage::CreateForWebContents(
            web_contents, base::KiB(tab_memory_footprint_estimate),
            discard_reason);
  } else {
    pre_discard_resource_usage->UpdateDiscardInfo(
        base::KiB(tab_memory_footprint_estimate), discard_reason);
  }
}

void TabLifecycleUnitSource::TabLifecycleUnit::DidStartLoading() {
  // It's possible for a discarded tab to receive this notification without
  // being focused first (e.g. right-click > Reload).
  is_discarded_ = false;
  RecomputeLifecycleUnitState();
}

void TabLifecycleUnitSource::TabLifecycleUnit::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::VISIBLE) {
    wall_time_when_hidden_ = base::TimeTicks::Max();
  } else if (wall_time_when_hidden_.is_max()) {
    wall_time_when_hidden_ = NowTicks();
  }
}

LifecycleUnitDiscardReason
TabLifecycleUnitSource::TabLifecycleUnit::GetDiscardReason() const {
  return discard_reason_;
}

}  // namespace resource_coordinator
