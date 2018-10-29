// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/process/process_metrics.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/intervention_policy_database.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store_factory.h"
#include "chrome/browser/resource_coordinator/tab_activity_watcher.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/usb/usb_tab_helper.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace resource_coordinator {

namespace {

using StateChangeReason = LifecycleUnitStateChangeReason;

bool IsDiscardedOrPendingDiscard(LifecycleUnitState state) {
  return state == LifecycleUnitState::DISCARDED ||
         state == LifecycleUnitState::PENDING_DISCARD;
}

bool IsFrozenOrPendingFreeze(LifecycleUnitState state) {
  return state == LifecycleUnitState::FROZEN ||
         state == LifecycleUnitState::PENDING_FREEZE;
}

// Returns true if it is valid to transition from |from| to |to| for |reason|.
bool IsValidStateChange(LifecycleUnitState from,
                        LifecycleUnitState to,
                        StateChangeReason reason) {
  switch (from) {
    case LifecycleUnitState::ACTIVE: {
      switch (to) {
        // Freeze() is called.
        case LifecycleUnitState::PENDING_FREEZE:
        // Discard(PROACTIVE) is called.
        case LifecycleUnitState::PENDING_DISCARD:
          return reason == StateChangeReason::BROWSER_INITIATED;
        // Discard(URGENT|EXTERNAL) is called.
        case LifecycleUnitState::DISCARDED: {
          return reason == StateChangeReason::SYSTEM_MEMORY_PRESSURE ||
                 reason == StateChangeReason::EXTENSION_INITIATED;
        }
        default:
          return false;
      }
    }
    case LifecycleUnitState::THROTTLED: {
      return false;
    }
    case LifecycleUnitState::PENDING_FREEZE: {
      switch (to) {
        // Discard(PROACTIVE) is called.
        case LifecycleUnitState::PENDING_DISCARD:
          return reason == StateChangeReason::BROWSER_INITIATED;
        // Discard(URGENT|EXTERNAL) is called.
        case LifecycleUnitState::DISCARDED:
          return reason == StateChangeReason::SYSTEM_MEMORY_PRESSURE ||
                 reason == StateChangeReason::EXTENSION_INITIATED;
        // The renderer notifies the browser that the page has been frozen.
        case LifecycleUnitState::FROZEN:
          return reason == StateChangeReason::RENDERER_INITIATED;
        // Unfreeze() is called.
        case LifecycleUnitState::PENDING_UNFREEZE:
          return reason == StateChangeReason::BROWSER_INITIATED;
        default:
          return false;
      }
    }
    case LifecycleUnitState::FROZEN: {
      switch (to) {
        // The renderer notifies the browser that the page was unfrozen after
        // it became visible.
        case LifecycleUnitState::ACTIVE: {
          return reason == StateChangeReason::RENDERER_INITIATED;
        }
        // Discard(PROACTIVE|URGENT|EXTERNAL) is called.
        case LifecycleUnitState::DISCARDED: {
          return reason == StateChangeReason::BROWSER_INITIATED ||
                 reason == StateChangeReason::SYSTEM_MEMORY_PRESSURE ||
                 reason == StateChangeReason::EXTENSION_INITIATED;
        }
        // Unfreeze() is called.
        case LifecycleUnitState::PENDING_UNFREEZE: {
          return reason == StateChangeReason::BROWSER_INITIATED;
        }
        default:
          return false;
      }
    }
    case LifecycleUnitState::PENDING_DISCARD: {
      switch (to) {
        // - Discard(URGENT|EXTERNAL) is called, or,
        // - The proactive discard can be completed because:
        //   - The freeze timeout expires, or,
        //   - The renderer notifies the browser that the page has been frozen.
        case LifecycleUnitState::DISCARDED:
          return reason == StateChangeReason::BROWSER_INITIATED;
        // The WebContents is focused.
        case LifecycleUnitState::PENDING_FREEZE:
          return reason == StateChangeReason::USER_INITIATED;
        default:
          return false;
      }
    }
    case LifecycleUnitState::DISCARDED: {
      switch (to) {
        // The WebContents is focused or reloaded.
        case LifecycleUnitState::ACTIVE:
          return reason == StateChangeReason::USER_INITIATED;
        default:
          return false;
      }
    }
    case LifecycleUnitState::PENDING_UNFREEZE: {
      switch (to) {
        // The renderer notifies the browser that the page was unfrozen after
        // Unfreeze() was called in the browser.
        case LifecycleUnitState::ACTIVE: {
          return reason == StateChangeReason::RENDERER_INITIATED;
        }
        // Discard(URGENT|EXTERNAL) is called.
        case LifecycleUnitState::DISCARDED: {
          return reason == StateChangeReason::SYSTEM_MEMORY_PRESSURE ||
                 reason == StateChangeReason::EXTENSION_INITIATED;
        }
        default: { return false; }
      }
    }
  }
}

StateChangeReason DiscardReasonToStateChangeReason(
    LifecycleUnitDiscardReason reason) {
  switch (reason) {
    case LifecycleUnitDiscardReason::EXTERNAL:
      return StateChangeReason::EXTENSION_INITIATED;
    case LifecycleUnitDiscardReason::PROACTIVE:
      return StateChangeReason::BROWSER_INITIATED;
    case LifecycleUnitDiscardReason::URGENT:
      return StateChangeReason::SYSTEM_MEMORY_PRESSURE;
  }
}

struct FeatureUsageEntry {
  SiteFeatureUsage usage;
  DecisionFailureReason failure_reason;
};

void CheckFeatureUsage(const SiteCharacteristicsDataReader* reader,
                       DecisionDetails* details) {
  const FeatureUsageEntry features[] = {
      {reader->UsesAudioInBackground(), DecisionFailureReason::HEURISTIC_AUDIO},
      {reader->UpdatesFaviconInBackground(),
       DecisionFailureReason::HEURISTIC_FAVICON},
      {reader->UsesNotificationsInBackground(),
       DecisionFailureReason::HEURISTIC_NOTIFICATIONS},
      {reader->UpdatesTitleInBackground(),
       DecisionFailureReason::HEURISTIC_TITLE}};

  // Avoid adding 'insufficient observation' reason multiple times.
  bool insufficient_observation = false;

  const auto* last = features + base::size(features);
  for (const auto* f = features; f != last; f++) {
    if (f->usage == SiteFeatureUsage::kSiteFeatureInUse) {
      details->AddReason(f->failure_reason);
    } else if (f->usage == SiteFeatureUsage::kSiteFeatureUsageUnknown) {
      insufficient_observation = true;
    }
  }

  if (insufficient_observation) {
    details->AddReason(
        DecisionFailureReason::HEURISTIC_INSUFFICIENT_OBSERVATION);
  }
}

void CheckIfTabCanCommunicateWithUserWhileInBackground(
    const content::WebContents* web_contents,
    DecisionDetails* details) {
  DCHECK(details);

  if (!web_contents->GetLastCommittedURL().is_valid() ||
      web_contents->GetLastCommittedURL().is_empty()) {
    return;
  }

  if (!URLShouldBeStoredInLocalDatabase(web_contents->GetLastCommittedURL()))
    return;

  SiteCharacteristicsDataStore* data_store =
      LocalSiteCharacteristicsDataStoreFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (!data_store)
    return;

  auto reader = data_store->GetReaderForOrigin(
      url::Origin::Create(web_contents->GetLastCommittedURL()));

  // TODO(sebmarchand): Add a failure reason for when the data isn't ready yet.

  CheckFeatureUsage(reader.get(), details);
}

}  // namespace

class TabLifecycleUnitExternalImpl : public TabLifecycleUnitExternal {
 public:
  explicit TabLifecycleUnitExternalImpl(
      TabLifecycleUnitSource::TabLifecycleUnit* tab_lifecycle_unit)
      : tab_lifecycle_unit_(tab_lifecycle_unit) {}

  // TabLifecycleUnitExternal:

  content::WebContents* GetWebContents() const override {
    return tab_lifecycle_unit_->web_contents();
  }

  bool IsMediaTab() const override { return tab_lifecycle_unit_->IsMediaTab(); }

  bool IsAutoDiscardable() const override {
    return tab_lifecycle_unit_->IsAutoDiscardable();
  }

  void SetAutoDiscardable(bool auto_discardable) override {
    tab_lifecycle_unit_->SetAutoDiscardable(auto_discardable);
  }

  bool DiscardTab() override {
    return tab_lifecycle_unit_->Discard(LifecycleUnitDiscardReason::EXTERNAL);
  }

  bool IsDiscarded() const override {
    // External code does not need to know about the intermediary
    // PENDING_DISCARD state. To external callers, the tab is discarded while in
    // the PENDING_DISCARD state.
    return IsDiscardedOrPendingDiscard(tab_lifecycle_unit_->GetState());
  }

  int GetDiscardCount() const override {
    return tab_lifecycle_unit_->GetDiscardCount();
  }

 private:
  TabLifecycleUnitSource::TabLifecycleUnit* tab_lifecycle_unit_ = nullptr;
};

TabLifecycleUnitSource::TabLifecycleUnit::TabLifecycleUnit(
    TabLifecycleUnitSource* source,
    base::ObserverList<TabLifecycleObserver>::Unchecked* observers,
    UsageClock* usage_clock,
    content::WebContents* web_contents,
    TabStripModel* tab_strip_model)
    : LifecycleUnitBase(source, web_contents->GetVisibility(), usage_clock),
      content::WebContentsObserver(web_contents),
      observers_(observers),
      tab_strip_model_(tab_strip_model) {
  DCHECK(observers_);
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
  if (GetVisibility() == content::Visibility::VISIBLE)
    last_focused_time_ = NowTicks();
  else
    last_focused_time_ = web_contents->GetLastActiveTime();
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
  const bool was_focused = last_focused_time_ == base::TimeTicks::Max();
  if (focused == was_focused)
    return;
  last_focused_time_ = focused ? base::TimeTicks::Max() : NowTicks();

  if (!focused)
    return;

  switch (GetState()) {
    case LifecycleUnitState::DISCARDED: {
      // Reload the tab.
      SetState(LifecycleUnitState::ACTIVE, StateChangeReason::USER_INITIATED);
      bool loaded = Load();
      DCHECK(loaded);
      break;
    }

    case LifecycleUnitState::PENDING_DISCARD: {
      // PENDING_DISCARD indicates that a freeze request is being processed by
      // the renderer and that the page should be discarded as soon as it is
      // frozen. On focus, we transition the state to PENDING_FREEZE and we stop
      // the freeze timeout timer to indicate that a freeze request is being
      // processed, but that the page should not be discarded once frozen. After
      // the renderer has processed the freeze request, it will realize that the
      // page is focused, unfreeze it and initiate a transition to ACTIVE.
      freeze_timeout_timer_->Stop();
      SetState(LifecycleUnitState::PENDING_FREEZE,
               StateChangeReason::USER_INITIATED);
      break;
    }

    default:
      break;
  }
}

void TabLifecycleUnitSource::TabLifecycleUnit::SetRecentlyAudible(
    bool recently_audible) {
  if (recently_audible)
    recently_audible_time_ = base::TimeTicks::Max();
  else if (recently_audible_time_ == base::TimeTicks::Max())
    recently_audible_time_ = NowTicks();
}

void TabLifecycleUnitSource::TabLifecycleUnit::UpdateLifecycleState(
    mojom::LifecycleState state) {
  switch (state) {
    case mojom::LifecycleState::kFrozen: {
      switch (GetState()) {
        case LifecycleUnitState::PENDING_DISCARD: {
          freeze_timeout_timer_->Stop();
          FinishDiscard(GetDiscardReason());
          break;
        }
        case LifecycleUnitState::PENDING_UNFREEZE: {
          // By the time the kFrozen state message arrive the tab might have
          // been reloaded by the user (by right-clicking on the tab) and might
          // have been switched to the PENDING_UNFREEZE state. It's safe to just
          // ignore this message here as an unfreeze signal has been sent to the
          // tab and so a |kRunning| transition will follow this one.
          break;
        }
        default: {
          SetState(LifecycleUnitState::FROZEN,
                   StateChangeReason::RENDERER_INITIATED);
        }
      }
      break;
    }

    case mojom::LifecycleState::kRunning: {
      SetState(LifecycleUnitState::ACTIVE,
               StateChangeReason::RENDERER_INITIATED);
      break;
    }

    default: {
      NOTREACHED();
      break;
    }
  }
}

void TabLifecycleUnitSource::TabLifecycleUnit::RequestFreezeForDiscard(
    LifecycleUnitDiscardReason reason) {
  DCHECK_EQ(reason, LifecycleUnitDiscardReason::PROACTIVE);

  SetState(LifecycleUnitState::PENDING_DISCARD,
           DiscardReasonToStateChangeReason(reason));
  EnsureFreezeTimeoutTimerInitialized();
  freeze_timeout_timer_->Start(
      FROM_HERE, kProactiveDiscardFreezeTimeout,
      base::BindRepeating(&TabLifecycleUnit::FinishDiscard,
                          base::Unretained(this), reason));
  web_contents()->SetPageFrozen(true);
}

TabLifecycleUnitExternal*
TabLifecycleUnitSource::TabLifecycleUnit::AsTabLifecycleUnitExternal() {
  // Create an impl the first time this is called.
  if (!external_impl_)
    external_impl_ = std::make_unique<TabLifecycleUnitExternalImpl>(this);
  return external_impl_.get();
}

base::string16 TabLifecycleUnitSource::TabLifecycleUnit::GetTitle() const {
  return web_contents()->GetTitle();
}

base::TimeTicks TabLifecycleUnitSource::TabLifecycleUnit::GetLastFocusedTime()
    const {
  return last_focused_time_;
}

base::ProcessHandle TabLifecycleUnitSource::TabLifecycleUnit::GetProcessHandle()
    const {
  content::RenderFrameHost* main_frame = web_contents()->GetMainFrame();
  if (!main_frame)
    return base::ProcessHandle();
  content::RenderProcessHost* process = main_frame->GetProcess();
  if (!process)
    return base::ProcessHandle();
  return process->GetProcess().Handle();
}

LifecycleUnit::SortKey TabLifecycleUnitSource::TabLifecycleUnit::GetSortKey()
    const {
  if (base::FeatureList::IsEnabled(features::kTabRanker)) {
    base::Optional<float> reactivation_score =
        resource_coordinator::TabActivityWatcher::GetInstance()
            ->CalculateReactivationScore(web_contents());
    if (reactivation_score.has_value())
      return SortKey(reactivation_score.value(), last_focused_time_);
    return SortKey(SortKey::kMaxScore, last_focused_time_);
  }

  return SortKey(last_focused_time_);
}

content::Visibility TabLifecycleUnitSource::TabLifecycleUnit::GetVisibility()
    const {
  return web_contents()->GetVisibility();
}

LifecycleUnitLoadingState
TabLifecycleUnitSource::TabLifecycleUnit::GetLoadingState() const {
  return TabLoadTracker::Get()->GetLoadingState(web_contents());
}

bool TabLifecycleUnitSource::TabLifecycleUnit::Load() {
  if (GetLoadingState() != LifecycleUnitLoadingState::UNLOADED)
    return false;

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

int TabLifecycleUnitSource::TabLifecycleUnit::
    GetEstimatedMemoryFreedOnDiscardKB() const {
  return GetPrivateMemoryKB(GetProcessHandle());
}

bool TabLifecycleUnitSource::TabLifecycleUnit::CanPurge() const {
  // A renderer can be purged if it's not playing media.
  return !IsMediaTabImpl(nullptr);
}

bool TabLifecycleUnitSource::TabLifecycleUnit::CanFreeze(
    DecisionDetails* decision_details) const {
  DCHECK(decision_details->reasons().empty());

  // Leave the |decision_details| empty and return immediately for "trivial"
  // rejection reasons. These aren't worth reporting about, as they have nothing
  // to do with the content itself.

  if (!IsValidStateChange(GetState(), LifecycleUnitState::PENDING_FREEZE,
                          StateChangeReason::BROWSER_INITIATED)) {
    return false;
  }

  // Allow a page to load fully before freezing it.
  if (TabLoadTracker::Get()->GetLoadingState(web_contents()) !=
      TabLoadTracker::LoadingState::LOADED) {
    return false;
  }

  bool check_heuristics = !GetStaticProactiveTabFreezeAndDiscardParams()
                               .disable_heuristics_protections;

  if (check_heuristics)
    CanFreezeHeuristicsChecks(decision_details);

  if (web_contents()->GetVisibility() == content::Visibility::VISIBLE)
    decision_details->AddReason(DecisionFailureReason::LIVE_STATE_VISIBLE);

  if (check_heuristics) {
    CheckIfTabIsUsedInBackground(decision_details,
                                 InterventionType::kProactive);
  }

  if (decision_details->reasons().empty()) {
    decision_details->AddReason(
        DecisionSuccessReason::HEURISTIC_OBSERVED_TO_BE_SAFE);
  }

  return decision_details->IsPositive();
}

bool TabLifecycleUnitSource::TabLifecycleUnit::CanDiscard(
    LifecycleUnitDiscardReason reason,
    DecisionDetails* decision_details) const {
  DCHECK(decision_details->reasons().empty());

  // Leave the |decision_details| empty and return immediately for "trivial"
  // rejection reasons. These aren't worth reporting about, as they have nothing
  // to do with the content itself.

  // Can't discard a tab that isn't in a TabStripModel.
  if (!tab_strip_model_)
    return false;

  const LifecycleUnitState target_state =
      reason == LifecycleUnitDiscardReason::PROACTIVE &&
              GetState() != LifecycleUnitState::FROZEN
          ? LifecycleUnitState::PENDING_DISCARD
          : LifecycleUnitState::DISCARDED;
  if (!IsValidStateChange(GetState(), target_state,
                          DiscardReasonToStateChangeReason(reason))) {
    return false;
  }

  if (web_contents()->IsCrashed())
    return false;

  // Do not discard tabs that don't have a valid URL (most probably they have
  // just been opened and discarding them would lose the URL).
  // TODO(fdoray): Look into a workaround to be able to kill the tab without
  // losing the pending navigation.
  if (!web_contents()->GetLastCommittedURL().is_valid() ||
      web_contents()->GetLastCommittedURL().is_empty()) {
    return false;
  }

// Fix for urgent discarding woes in crbug.com/883071. These protections only
// apply on non-ChromeOS desktop platforms (Linux, Mac, Win).
// NOTE: These do not currently provide DecisionDetails!
#if !defined(OS_CHROMEOS)
  if (reason == LifecycleUnitDiscardReason::URGENT) {
    // Limit urgent discarding to once only.
    if (GetDiscardCount() > 0)
      return false;
    // Protect non-visible tabs from urgent discarding for a period of time.
    if (web_contents()->GetVisibility() != content::Visibility::VISIBLE) {
      base::TimeDelta time_in_bg = NowTicks() - GetWallTimeWhenHidden();
      if (time_in_bg < kBackgroundUrgentProtectionTime)
        return false;
    }
  }
#endif

  bool check_heuristics = !GetStaticProactiveTabFreezeAndDiscardParams()
                               .disable_heuristics_protections;

  if (check_heuristics)
    CanDiscardHeuristicsChecks(decision_details, reason);

#if defined(OS_CHROMEOS)
  if (web_contents()->GetVisibility() == content::Visibility::VISIBLE)
    decision_details->AddReason(DecisionFailureReason::LIVE_STATE_VISIBLE);
#else
  // Do not discard the tab if it is currently active in its window.
  if (tab_strip_model_->GetActiveWebContents() == web_contents())
    decision_details->AddReason(DecisionFailureReason::LIVE_STATE_VISIBLE);
#endif  // defined(OS_CHROMEOS)

  // Do not discard tabs in which the user has entered text in a form.
  if (web_contents()->GetPageImportanceSignals().had_form_interaction)
    decision_details->AddReason(DecisionFailureReason::LIVE_STATE_FORM_ENTRY);

  // Do not discard PDFs as they might contain entry that is not saved and they
  // don't remember their scrolling positions. See crbug.com/547286 and
  // crbug.com/65244.
  // TODO(fdoray): Remove this workaround when the bugs are fixed.
  if (web_contents()->GetContentsMimeType() == "application/pdf")
    decision_details->AddReason(DecisionFailureReason::LIVE_STATE_IS_PDF);

  // Do not discard a tab that was explicitly disallowed to.
  if (!auto_discardable_) {
    decision_details->AddReason(
        DecisionFailureReason::LIVE_STATE_EXTENSION_DISALLOWED);
  }

  if (check_heuristics) {
    CheckIfTabIsUsedInBackground(decision_details,
                                 reason == LifecycleUnitDiscardReason::PROACTIVE
                                     ? InterventionType::kProactive
                                     : InterventionType::kExternalOrUrgent);
  }

  if (decision_details->reasons().empty()) {
    decision_details->AddReason(
        DecisionSuccessReason::HEURISTIC_OBSERVED_TO_BE_SAFE);
    DCHECK(decision_details->IsPositive());
  }
  return decision_details->IsPositive();
}

bool TabLifecycleUnitSource::TabLifecycleUnit::Freeze() {
  if (!IsValidStateChange(GetState(), LifecycleUnitState::PENDING_FREEZE,
                          StateChangeReason::BROWSER_INITIATED)) {
    return false;
  }

  // WebContents::SetPageFrozen() DCHECKs if the page is visible.
  if (web_contents()->GetVisibility() == content::Visibility::VISIBLE)
    return false;

  SetState(LifecycleUnitState::PENDING_FREEZE,
           StateChangeReason::BROWSER_INITIATED);
  web_contents()->SetPageFrozen(true);
  return true;
}

bool TabLifecycleUnitSource::TabLifecycleUnit::Unfreeze() {
  if (!IsValidStateChange(GetState(), LifecycleUnitState::PENDING_UNFREEZE,
                          StateChangeReason::BROWSER_INITIATED)) {
    return false;
  }

  // WebContents::SetPageFrozen() DCHECKs if the page is visible.
  if (web_contents()->GetVisibility() == content::Visibility::VISIBLE)
    return false;

  SetState(LifecycleUnitState::PENDING_UNFREEZE,
           StateChangeReason::BROWSER_INITIATED);
  web_contents()->SetPageFrozen(false);
  return true;
}

ukm::SourceId TabLifecycleUnitSource::TabLifecycleUnit::GetUkmSourceId() const {
  resource_coordinator::ResourceCoordinatorTabHelper* observer =
      resource_coordinator::ResourceCoordinatorTabHelper::FromWebContents(
          web_contents());
  if (!observer)
    return ukm::kInvalidSourceId;
  return observer->ukm_source_id();
}

bool TabLifecycleUnitSource::TabLifecycleUnit::IsMediaTab() const {
  return IsMediaTabImpl(nullptr);
}

bool TabLifecycleUnitSource::TabLifecycleUnit::IsAutoDiscardable() const {
  return auto_discardable_;
}

void TabLifecycleUnitSource::TabLifecycleUnit::SetAutoDiscardable(
    bool auto_discardable) {
  if (auto_discardable_ == auto_discardable)
    return;
  auto_discardable_ = auto_discardable;
  for (auto& observer : *observers_)
    observer.OnAutoDiscardableStateChange(web_contents(), auto_discardable_);
}

void TabLifecycleUnitSource::TabLifecycleUnit::FinishDiscard(
    LifecycleUnitDiscardReason discard_reason) {
  UMA_HISTOGRAM_BOOLEAN(
      "TabManager.Discarding.DiscardedTabHasBeforeUnloadHandler",
      web_contents()->NeedToFireBeforeUnload());

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
  std::unique_ptr<content::WebContents> null_contents =
      content::WebContents::Create(create_params);
  content::WebContents* raw_null_contents = null_contents.get();

  // Attach the ResourceCoordinatorTabHelper. In production code this has
  // already been attached by now due to AttachTabHelpers, but there's a long
  // tail of tests that don't add these helpers. This ensures that the various
  // DCHECKs in the state transition machinery don't fail.
  ResourceCoordinatorTabHelper::CreateForWebContents(raw_null_contents);

  // Copy over the state from the navigation controller to preserve the
  // back/forward history and to continue to display the correct title/favicon.
  //
  // Set |needs_reload| to false so that the tab is not automatically reloaded
  // when activated. If it was true, there would be an immediate reload when the
  // active tab of a non-visible window is discarded. SetFocused() will take
  // care of reloading the tab when it becomes active in a focused window.
  null_contents->GetController().CopyStateFrom(old_contents->GetController(),
                                               /* needs_reload */ false);

  // First try to fast-kill the process, if it's just running a single tab.
  bool fast_shutdown_success =
      GetRenderProcessHost()->FastShutdownIfPossible(1u, false);

#if defined(OS_CHROMEOS)
  if (!fast_shutdown_success &&
      discard_reason == LifecycleUnitDiscardReason::URGENT) {
    content::RenderFrameHost* main_frame = old_contents->GetMainFrame();
    // We avoid fast shutdown on tabs with beforeunload handlers on the main
    // frame, as that is often an indication of unsaved user state.
    DCHECK(main_frame);
    if (!main_frame->GetSuddenTerminationDisablerState(
            blink::kBeforeUnloadHandler)) {
      fast_shutdown_success = GetRenderProcessHost()->FastShutdownIfPossible(
          1u, /* skip_unload_handlers */ true);
    }
    UMA_HISTOGRAM_BOOLEAN(
        "TabManager.Discarding.DiscardedTabCouldUnsafeFastShutdown",
        fast_shutdown_success);
  }
#endif
  UMA_HISTOGRAM_BOOLEAN("TabManager.Discarding.DiscardedTabCouldFastShutdown",
                        fast_shutdown_success);

  // Replace the discarded tab with the null version.
  const int index = tab_strip_model_->GetIndexOfWebContents(old_contents);
  DCHECK_NE(index, TabStripModel::kNoTab);

  // This ensures that on reload after discard, the document has
  // "WasDiscarded" set to true.
  // The "WasDiscarded" state is also sent to tab_strip_model.
  null_contents->SetWasDiscarded(true);

  std::unique_ptr<content::WebContents> old_contents_deleter =
      tab_strip_model_->ReplaceWebContentsAt(index, std::move(null_contents));
  DCHECK_EQ(web_contents(), raw_null_contents);

  // Discard the old tab's renderer.
  // TODO(jamescook): This breaks script connections with other tabs. Find a
  // different approach that doesn't do that, perhaps based on
  // RenderFrameProxyHosts.
  old_contents_deleter.reset();

  SetState(LifecycleUnitState::DISCARDED,
           DiscardReasonToStateChangeReason(discard_reason));
  DCHECK_EQ(GetLoadingState(), LifecycleUnitLoadingState::UNLOADED);
}

bool TabLifecycleUnitSource::TabLifecycleUnit::DiscardImpl(
    LifecycleUnitDiscardReason reason) {
  // Can't discard a tab when it isn't in a tabstrip.
  if (!tab_strip_model_)
    return false;

  const LifecycleUnitState target_state =
      reason == LifecycleUnitDiscardReason::PROACTIVE &&
              GetState() != LifecycleUnitState::FROZEN
          ? LifecycleUnitState::PENDING_DISCARD
          : LifecycleUnitState::DISCARDED;
  if (!IsValidStateChange(GetState(), target_state,
                          DiscardReasonToStateChangeReason(reason))) {
    return false;
  }

  // If the tab is not going through an urgent discard, it should be frozen
  // first. Freeze the tab and set a timer to callback to FinishDiscard() in
  // case the freeze callback takes too long.
  //
  // TODO(fdoray): Request a freeze for EXTERNAL discards too once that doesn't
  // cause asynchronous change of tab id. https://crbug.com/632839
  if (target_state == LifecycleUnitState::PENDING_DISCARD)
    RequestFreezeForDiscard(reason);
  else
    FinishDiscard(reason);

  return true;
}

TabLifecycleUnitSource* TabLifecycleUnitSource::TabLifecycleUnit::GetTabSource()
    const {
  return static_cast<TabLifecycleUnitSource*>(GetSource());
}

bool TabLifecycleUnitSource::TabLifecycleUnit::IsMediaTabImpl(
    DecisionDetails* decision_details) const {
  // TODO(fdoray): Consider being notified of audible, capturing and mirrored
  // state changes via WebContentsDelegate::NavigationStateChanged() and/or
  // WebContentsObserver::OnAudioStateChanged and/or RecentlyAudibleHelper.
  // https://crbug.com/775644

  bool is_media_tab = false;

  if (recently_audible_time_ == base::TimeTicks::Max() ||
      (!recently_audible_time_.is_null() &&
       NowTicks() - recently_audible_time_ < kTabAudioProtectionTime)) {
    is_media_tab = true;
    if (decision_details) {
      decision_details->AddReason(
          DecisionFailureReason::LIVE_STATE_PLAYING_AUDIO);
    }
  }

  scoped_refptr<MediaStreamCaptureIndicator> media_indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();

  if (media_indicator->IsCapturingUserMedia(web_contents())) {
    is_media_tab = true;
    if (decision_details)
      decision_details->AddReason(DecisionFailureReason::LIVE_STATE_CAPTURING);
  }

  if (media_indicator->IsBeingMirrored(web_contents())) {
    is_media_tab = true;
    if (decision_details)
      decision_details->AddReason(DecisionFailureReason::LIVE_STATE_MIRRORING);
  }

  if (media_indicator->IsCapturingDesktop(web_contents())) {
    is_media_tab = true;
    if (decision_details) {
      decision_details->AddReason(
          DecisionFailureReason::LIVE_STATE_DESKTOP_CAPTURE);
    }
  }

  return is_media_tab;
}

content::RenderProcessHost*
TabLifecycleUnitSource::TabLifecycleUnit::GetRenderProcessHost() const {
  return web_contents()->GetMainFrame()->GetProcess();
}

void TabLifecycleUnitSource::TabLifecycleUnit::
    EnsureFreezeTimeoutTimerInitialized() {
  if (!freeze_timeout_timer_) {
    freeze_timeout_timer_ =
        std::make_unique<base::OneShotTimer>(GetTickClock());
  }
}

void TabLifecycleUnitSource::TabLifecycleUnit::OnLifecycleUnitStateChanged(
    LifecycleUnitState last_state,
    LifecycleUnitStateChangeReason reason) {
  DCHECK(IsValidStateChange(last_state, GetState(), reason))
      << "Cannot transition TabLifecycleUnit state from " << last_state
      << " to " << GetState() << " with reason " << reason;

  // Invoke OnDiscardedStateChange() if necessary.
  const bool was_discarded = IsDiscardedOrPendingDiscard(last_state);
  const bool is_discarded = IsDiscardedOrPendingDiscard(GetState());
  if (was_discarded != is_discarded) {
    for (auto& observer : *observers_)
      observer.OnDiscardedStateChange(web_contents(), is_discarded);
  }
}

void TabLifecycleUnitSource::TabLifecycleUnit::DidStartLoading() {
  if (IsDiscardedOrPendingDiscard(GetState())) {
    // This happens when a discarded tab is explicitly reloaded without being
    // focused first (right-click > Reload).
    SetState(LifecycleUnitState::ACTIVE, StateChangeReason::USER_INITIATED);
  } else if (IsFrozenOrPendingFreeze(GetState())) {
    // This happens when a frozen tab is explicitly reloaded without being
    // focused first (right-click > Reload).
    Unfreeze();

    if (freeze_timeout_timer_)
      freeze_timeout_timer_->Stop();
  }
}

void TabLifecycleUnitSource::TabLifecycleUnit::OnVisibilityChanged(
    content::Visibility visibility) {
  OnLifecycleUnitVisibilityChanged(visibility);
}

void TabLifecycleUnitSource::TabLifecycleUnit::CheckIfTabIsUsedInBackground(
    DecisionDetails* decision_details,
    InterventionType intervention_type) const {
  DCHECK(decision_details);

  // We deliberately run through all of the logic without early termination.
  // This ensures that the decision details lists all possible reasons that the
  // transition can be denied.

  // Do not freeze tabs that are casting/mirroring/playing audio.
  IsMediaTabImpl(decision_details);

  if (GetStaticProactiveTabFreezeAndDiscardParams()
          .should_protect_tabs_sharing_browsing_instance) {
    if (web_contents()->GetSiteInstance()->GetRelatedActiveContentsCount() >
        1U) {
      decision_details->AddReason(
          DecisionFailureReason::LIVE_STATE_SHARING_BROWSING_INSTANCE);
    }
  }

  // Consult the local database to see if this tab could try to communicate with
  // the user while in background (don't check for the visibility here as
  // there's already a check for that above). Only do this for proactive
  // interventions.
  if (intervention_type == InterventionType::kProactive) {
    CheckIfTabCanCommunicateWithUserWhileInBackground(web_contents(),
                                                      decision_details);
  }

  // Do not freeze/discard a tab that has active WebUSB connections.
  if (auto* usb_tab_helper = UsbTabHelper::FromWebContents(web_contents())) {
    if (usb_tab_helper->IsDeviceConnected()) {
      decision_details->AddReason(
          DecisionFailureReason::LIVE_STATE_USING_WEB_USB);
    }
  }

  // Do not freeze tabs that are currently using DevTools.
  if (DevToolsWindow::GetInstanceForInspectedWebContents(web_contents())) {
    decision_details->AddReason(
        DecisionFailureReason::LIVE_STATE_DEVTOOLS_OPEN);
  }
}

void TabLifecycleUnitSource::TabLifecycleUnit::CanFreezeHeuristicsChecks(
    DecisionDetails* decision_details) const {
  if (!GetTabSource()->tab_lifecycles_enterprise_policy()) {
    decision_details->AddReason(
        DecisionFailureReason::LIFECYCLES_ENTERPRISE_POLICY_OPT_OUT);
  }

  auto intervention_policy =
      GetTabSource()->intervention_policy_database()->GetFreezingPolicy(
          url::Origin::Create(web_contents()->GetLastCommittedURL()));

  switch (intervention_policy) {
    case OriginInterventions::OPT_IN:
      decision_details->AddReason(DecisionSuccessReason::GLOBAL_WHITELIST);
      break;
    case OriginInterventions::OPT_OUT:
      decision_details->AddReason(DecisionFailureReason::GLOBAL_BLACKLIST);
      break;
    case OriginInterventions::DEFAULT:
      break;
  }
}

void TabLifecycleUnitSource::TabLifecycleUnit::CanDiscardHeuristicsChecks(
    DecisionDetails* decision_details,
    LifecycleUnitDiscardReason reason) const {
  // We deliberately run through all of the logic without early termination.
  // This ensures that the decision details lists all possible reasons that
  // the transition can be denied.
  if (!GetTabSource()->tab_lifecycles_enterprise_policy()) {
    decision_details->AddReason(
        DecisionFailureReason::LIFECYCLES_ENTERPRISE_POLICY_OPT_OUT);
  }

  if (reason == LifecycleUnitDiscardReason::PROACTIVE) {
    auto intervention_policy =
        GetTabSource()->intervention_policy_database()->GetDiscardingPolicy(
            url::Origin::Create(web_contents()->GetLastCommittedURL()));

    switch (intervention_policy) {
      case OriginInterventions::OPT_IN:
        decision_details->AddReason(DecisionSuccessReason::GLOBAL_WHITELIST);
        break;
      case OriginInterventions::OPT_OUT:
        decision_details->AddReason(DecisionFailureReason::GLOBAL_BLACKLIST);
        break;
      case OriginInterventions::DEFAULT:
        break;
    }
  }
}

}  // namespace resource_coordinator
