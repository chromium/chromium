// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/process/process_metrics.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/tab_contents/form_interaction_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/device_event_log/device_event_log.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/permissions/permission_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/mojom/frame/sudden_termination_disabler_type.mojom.h"
#include "url/gurl.h"

namespace resource_coordinator {

namespace {

using StateChangeReason = LifecycleUnitStateChangeReason;


// Returns true if it is valid to transition from |from| to |to| for |reason|.
bool IsValidStateChange(LifecycleUnitState from,
                        LifecycleUnitState to,
                        StateChangeReason reason) {
  switch (from) {
    case LifecycleUnitState::ACTIVE: {
      switch (to) {
        // Discard(URGENT|EXTERNAL) is called.
        case LifecycleUnitState::DISCARDED: {
          return reason == StateChangeReason::BROWSER_INITIATED ||
                 reason == StateChangeReason::SYSTEM_MEMORY_PRESSURE ||
                 reason == StateChangeReason::EXTENSION_INITIATED;
        }
        case LifecycleUnitState::FROZEN: {
          // Render-initiated freezing, which happens when freezing a page
          // through ChromeDriver.
          return reason == StateChangeReason::RENDERER_INITIATED;
        }
        default:
          return false;
      }
    }
    case LifecycleUnitState::THROTTLED: {
      return false;
    }
    case LifecycleUnitState::FROZEN: {
      switch (to) {
        // The renderer notifies the browser that the page was unfrozen after
        // it became visible.
        case LifecycleUnitState::ACTIVE: {
          return reason == StateChangeReason::RENDERER_INITIATED;
        }
        // Discard(URGENT|EXTERNAL) is called.
        case LifecycleUnitState::DISCARDED: {
          return reason == StateChangeReason::BROWSER_INITIATED ||
                 reason == StateChangeReason::SYSTEM_MEMORY_PRESSURE ||
                 reason == StateChangeReason::EXTENSION_INITIATED;
        }
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
  }
}

StateChangeReason DiscardReasonToStateChangeReason(
    LifecycleUnitDiscardReason reason) {
  switch (reason) {
    case LifecycleUnitDiscardReason::EXTERNAL:
      return StateChangeReason::EXTENSION_INITIATED;
    case LifecycleUnitDiscardReason::URGENT:
      return StateChangeReason::SYSTEM_MEMORY_PRESSURE;
    case LifecycleUnitDiscardReason::PROACTIVE:
      return StateChangeReason::BROWSER_INITIATED;
    case LifecycleUnitDiscardReason::SUGGESTED:
      return StateChangeReason::BROWSER_INITIATED;
  }
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

  bool IsAutoDiscardable() const override {
    return tab_lifecycle_unit_->IsAutoDiscardable();
  }

  void SetAutoDiscardable(bool auto_discardable) override {
    tab_lifecycle_unit_->SetAutoDiscardable(auto_discardable);
  }

  bool DiscardTab(LifecycleUnitDiscardReason reason,
                  uint64_t memory_footprint_estimate) override {
    return tab_lifecycle_unit_->Discard(reason, memory_footprint_estimate);
  }

  bool IsDiscarded() const override {
    return tab_lifecycle_unit_->GetState() == LifecycleUnitState::DISCARDED;
  }

  int GetDiscardCount() const override {
    return tab_lifecycle_unit_->GetDiscardCount();
  }

  base::Time GetLastFocusedTime() const override {
    return tab_lifecycle_unit_->GetLastFocusedTime();
  }

 private:
  raw_ptr<TabLifecycleUnitSource::TabLifecycleUnit> tab_lifecycle_unit_ =
      nullptr;
};

TabLifecycleUnitSource::TabLifecycleUnit::TabLifecycleUnit(
    TabLifecycleUnitSource* source,
    base::ObserverList<TabLifecycleObserver>::UncheckedAndDanglingUntriaged*
        observers,
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
  if (GetVisibility() == content::Visibility::VISIBLE) {
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

  switch (GetState()) {
    case LifecycleUnitState::DISCARDED: {
      // Transition to the active state.
      SetState(LifecycleUnitState::ACTIVE, StateChangeReason::USER_INITIATED);

      // Load the tab if it's discarded. It will typically be discarded, but
      // might not be if this is invoked as part of reloading the tab explicitly
      // and we haven't been notified of the ongoing load yet
      // (crbug.com/40075246).
      if (web_contents()->WasDiscarded()) {
        bool loaded = Load();
        DCHECK(loaded);
      }
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
    performance_manager::mojom::LifecycleState state) {
  switch (state) {
    case performance_manager::mojom::LifecycleState::kFrozen: {
      SetState(LifecycleUnitState::FROZEN,
               StateChangeReason::RENDERER_INITIATED);
      break;
    }

    case performance_manager::mojom::LifecycleState::kRunning: {
      SetState(LifecycleUnitState::ACTIVE,
               StateChangeReason::RENDERER_INITIATED);
      break;
    }

    default: {
      NOTREACHED_IN_MIGRATION();
      break;
    }
  }
}

TabLifecycleUnitExternal*
TabLifecycleUnitSource::TabLifecycleUnit::AsTabLifecycleUnitExternal() {
  // Create an impl the first time this is called.
  if (!external_impl_)
    external_impl_ = std::make_unique<TabLifecycleUnitExternalImpl>(this);
  return external_impl_.get();
}

std::u16string TabLifecycleUnitSource::TabLifecycleUnit::GetTitle() const {
  return web_contents()->GetTitle();
}

base::TimeTicks
TabLifecycleUnitSource::TabLifecycleUnit::GetLastFocusedTimeTicks() const {
  return last_focused_time_ticks_;
}

base::Time TabLifecycleUnitSource::TabLifecycleUnit::GetLastFocusedTime()
    const {
  return last_focused_time_;
}

base::ProcessHandle TabLifecycleUnitSource::TabLifecycleUnit::GetProcessHandle()
    const {
  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  if (!main_frame)
    return base::ProcessHandle();
  content::RenderProcessHost* process = main_frame->GetProcess();
  if (!process)
    return base::ProcessHandle();
  return process->GetProcess().Handle();
}

LifecycleUnit::SortKey TabLifecycleUnitSource::TabLifecycleUnit::GetSortKey()
    const {
  return SortKey(last_focused_time_ticks_);
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
  web_contents()->Focus();
  return true;
}

int TabLifecycleUnitSource::TabLifecycleUnit::
    GetEstimatedMemoryFreedOnDiscardKB() const {
  return GetPrivateMemoryKB(GetProcessHandle());
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

  if (!IsValidStateChange(GetState(), LifecycleUnitState::DISCARDED,
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
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (reason == LifecycleUnitDiscardReason::URGENT) {
    // Limit urgent discarding to once only, unless discarding for the
    // enterprise memory limit feature.
    if (GetDiscardCount() > 0 &&
        !GetTabSource()->memory_limit_enterprise_policy())
      return false;
    // Protect non-visible tabs from urgent discarding for a period of time.
    if (web_contents()->GetVisibility() != content::Visibility::VISIBLE) {
      base::TimeDelta time_in_bg = NowTicks() - GetWallTimeWhenHidden();
      // TODO(sebmarchand): Check if this should be lowered when the enterprise
      // memory limit feature is set.
      if (time_in_bg < kBackgroundUrgentProtectionTime)
        return false;
    }
  }
#endif

  // IMPORTANT: Only the first reason added to |decision_details| determines
  // whether the tab can be discarded. Additional reasons can be added for
  // reporting purposes, but do not affect whether the tab can be discarded.

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (web_contents()->GetVisibility() == content::Visibility::VISIBLE)
    decision_details->AddReason(DecisionFailureReason::LIVE_STATE_VISIBLE);
#else
  // Do not discard the tab if it is currently active in its window.
  if (tab_strip_model_->GetActiveWebContents() == web_contents())
    decision_details->AddReason(DecisionFailureReason::LIVE_STATE_VISIBLE);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Do not discard tabs in which the user has entered text in a form.

  // The FormInteractionTabHelper isn't available in some unit tests.
  if (auto* form_interaction_helper =
          FormInteractionTabHelper::FromWebContents(web_contents())) {
    if (form_interaction_helper->had_form_interaction())
      decision_details->AddReason(DecisionFailureReason::LIVE_STATE_FORM_ENTRY);
  }

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

  // Do not discard tabs using media.
  CheckMediaUsage(decision_details);

  // Do not discard tabs using device APIs, as this usually breaks
  // functionality.
  CheckDeviceUsage(decision_details);

  // Do not discard tabs that are currently using DevTools, as this can break a
  // debugging session.
  if (DevToolsWindow::GetInstanceForInspectedWebContents(web_contents())) {
    decision_details->AddReason(
        DecisionFailureReason::LIVE_STATE_DEVTOOLS_OPEN);
  }

  web_app::WebAppProvider* web_app_provider =
      web_app::WebAppProvider::GetForWebContents(web_contents());
  if (web_app_provider &&
      web_app_provider->ui_manager().IsInAppWindow(web_contents())) {
    // Do not discard Desktop PWA windows. Preserve native-app experience.
    decision_details->AddReason(DecisionFailureReason::LIVE_WEB_APP);
  }

  if (web_contents()->HasPictureInPictureVideo() ||
      web_contents()->HasPictureInPictureDocument()) {
    decision_details->AddReason(DecisionFailureReason::LIVE_PICTURE_IN_PICTURE);
  }

  if (decision_details->reasons().empty()) {
    decision_details->AddReason(
        DecisionSuccessReason::HEURISTIC_OBSERVED_TO_BE_SAFE);
    DCHECK(decision_details->IsPositive());
  }
  return decision_details->IsPositive();
}

ukm::SourceId TabLifecycleUnitSource::TabLifecycleUnit::GetUkmSourceId() const {
  resource_coordinator::ResourceCoordinatorTabHelper* observer =
      resource_coordinator::ResourceCoordinatorTabHelper::FromWebContents(
          web_contents());
  if (!observer)
    return ukm::kInvalidSourceId;
  return observer->ukm_source_id();
}

bool TabLifecycleUnitSource::TabLifecycleUnit::IsAutoDiscardable() const {
  return auto_discardable_;
}

void TabLifecycleUnitSource::TabLifecycleUnit::SetAutoDiscardable(
    bool auto_discardable) {
  if (auto_discardable_ == auto_discardable)
    return;
  auto_discardable_ = auto_discardable;

  performance_manager::PageLiveStateDecorator::SetIsAutoDiscardable(
      web_contents(), auto_discardable_);

  for (auto& observer : *observers_)
    observer.OnAutoDiscardableStateChange(web_contents(), auto_discardable_);
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

  SetState(LifecycleUnitState::DISCARDED,
           DiscardReasonToStateChangeReason(discard_reason));
  DCHECK_EQ(GetLoadingState(), LifecycleUnitLoadingState::UNLOADED);

  web_contents()->NotifyWasDiscarded();
}

void TabLifecycleUnitSource::TabLifecycleUnit::
    FinishDiscardAndPreserveWebContents(
        LifecycleUnitDiscardReason discard_reason,
        uint64_t tab_memory_footprint_estimate) {
  UpdatePreDiscardResourceUsage(web_contents(), discard_reason,
                                tab_memory_footprint_estimate);

  AttemptFastKillForDiscard(web_contents(), discard_reason);

  web_contents()->Discard();
  tab_strip_model_->UpdateWebContentsStateAt(
      tab_strip_model_->GetIndexOfWebContents(web_contents()),
      TabChangeType::kAll);

  SetState(LifecycleUnitState::DISCARDED,
           DiscardReasonToStateChangeReason(discard_reason));
}

void TabLifecycleUnitSource::TabLifecycleUnit::AttemptFastKillForDiscard(
    content::WebContents* web_contents,
    LifecycleUnitDiscardReason discard_reason) {
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  CHECK(main_frame);
  content::RenderProcessHost* render_process_host = main_frame->GetProcess();
  CHECK(render_process_host);

  // First try to fast-kill the process, if it's just running a single tab.
#if BUILDFLAG(IS_CHROMEOS)
  if (!render_process_host->FastShutdownIfPossible(1u, false) &&
      discard_reason == LifecycleUnitDiscardReason::URGENT) {
    // We avoid fast shutdown on tabs with beforeunload handlers on the main
    // frame, as that is often an indication of unsaved user state.
    if (!main_frame->GetSuddenTerminationDisablerState(
            blink::mojom::SuddenTerminationDisablerType::
                kBeforeUnloadHandler)) {
      render_process_host->FastShutdownIfPossible(
          1u, /*skip_unload_handlers=*/true);
    }
  }
#else
  render_process_host->FastShutdownIfPossible(1u, false);
#endif
}

bool TabLifecycleUnitSource::TabLifecycleUnit::Discard(
    LifecycleUnitDiscardReason reason,
    uint64_t tab_memory_footprint_estimate) {
  // Can't discard a tab when it isn't in a tabstrip.
  if (!tab_strip_model_) {
    // Logs are used to diagnose user feedback reports.
    MEMORY_LOG(ERROR) << "Skipped discarding unit " << GetID()
                      << " because it isn't in a tab strip.";
    return false;
  }

  if (!IsValidStateChange(GetState(), LifecycleUnitState::DISCARDED,
                          DiscardReasonToStateChangeReason(reason))) {
    // Logs are used to diagnose user feedback reports.
    MEMORY_LOG(ERROR) << "Skipped discarding unit " << GetID()
                      << " because a transition from " << GetState()
                      << "to discarded is not allowed.";
    return false;
  }

  // Can't discard a tab displayed in a picture-in-picture window. We check this
  // here instead of in `CanDiscard` as not all calls to `Discard` check
  // `CanDiscard` and discarding a picture-in-picture WebContents leaves the
  // window in a bad state.
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  if (browser && browser->is_type_picture_in_picture()) {
    return false;
  }

  discard_reason_ = reason;

  if (base::FeatureList::IsEnabled(features::kWebContentsDiscard)) {
    FinishDiscardAndPreserveWebContents(reason, tab_memory_footprint_estimate);
  } else {
    FinishDiscard(reason, tab_memory_footprint_estimate);
  }

  return true;
}

TabLifecycleUnitSource* TabLifecycleUnitSource::TabLifecycleUnit::GetTabSource()
    const {
  return static_cast<TabLifecycleUnitSource*>(GetSource());
}

void TabLifecycleUnitSource::TabLifecycleUnit::CheckMediaUsage(
    DecisionDetails* decision_details) const {
  // TODO(fdoray): Consider being notified of audible, capturing and mirrored
  // state changes via WebContentsDelegate::NavigationStateChanged() and/or
  // WebContentsObserver::OnAudioStateChanged and/or RecentlyAudibleHelper.
  // https://crbug.com/775644

  if (recently_audible_time_ == base::TimeTicks::Max() ||
      (!recently_audible_time_.is_null() &&
       NowTicks() - recently_audible_time_ < kTabAudioProtectionTime)) {
      decision_details->AddReason(
          DecisionFailureReason::LIVE_STATE_PLAYING_AUDIO);
  }

  scoped_refptr<MediaStreamCaptureIndicator> media_indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();

  if (media_indicator->IsCapturingUserMedia(web_contents())) {
      decision_details->AddReason(DecisionFailureReason::LIVE_STATE_CAPTURING);
  }

  if (media_indicator->IsBeingMirrored(web_contents())) {
      decision_details->AddReason(DecisionFailureReason::LIVE_STATE_MIRRORING);
  }

  if (media_indicator->IsCapturingWindow(web_contents()) ||
      media_indicator->IsCapturingDisplay(web_contents())) {
    decision_details->AddReason(
        DecisionFailureReason::LIVE_STATE_DESKTOP_CAPTURE);
  }
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
            web_contents, tab_memory_footprint_estimate, discard_reason);
  } else {
    pre_discard_resource_usage->UpdateDiscardInfo(tab_memory_footprint_estimate,
                                                  discard_reason);
  }
}

void TabLifecycleUnitSource::TabLifecycleUnit::OnLifecycleUnitStateChanged(
    LifecycleUnitState last_state,
    LifecycleUnitStateChangeReason reason) {
  DCHECK(IsValidStateChange(last_state, GetState(), reason))
      << "Cannot transition TabLifecycleUnit state from " << last_state
      << " to " << GetState() << " with reason " << reason;

  // Invoke OnDiscardedStateChange() if necessary.
  const bool was_discarded = last_state == LifecycleUnitState::DISCARDED;
  const bool is_discarded = GetState() == LifecycleUnitState::DISCARDED;
  if (was_discarded != is_discarded) {
    for (auto& observer : *observers_) {
      observer.OnDiscardedStateChange(web_contents(), GetDiscardReason(),
                                      is_discarded);
    }
  }
}

void TabLifecycleUnitSource::TabLifecycleUnit::DidStartLoading() {
  if (GetState() == LifecycleUnitState::DISCARDED) {
    // This happens when a discarded tab is explicitly reloaded without being
    // focused first (right-click > Reload).
    SetState(LifecycleUnitState::ACTIVE, StateChangeReason::USER_INITIATED);
  }
}

void TabLifecycleUnitSource::TabLifecycleUnit::OnVisibilityChanged(
    content::Visibility visibility) {
  OnLifecycleUnitVisibilityChanged(visibility);
}

void TabLifecycleUnitSource::TabLifecycleUnit::CheckDeviceUsage(
    DecisionDetails* decision_details) const {
  DCHECK(decision_details);

  if (web_contents()->IsConnectedToUsbDevice()) {
    decision_details->AddReason(
        DecisionFailureReason::LIVE_STATE_USING_WEB_USB);
  }

  if (web_contents()->IsConnectedToBluetoothDevice()) {
    decision_details->AddReason(
        DecisionFailureReason::LIVE_STATE_USING_BLUETOOTH);
  }
}

LifecycleUnitDiscardReason
TabLifecycleUnitSource::TabLifecycleUnit::GetDiscardReason() const {
  return discard_reason_;
}

}  // namespace resource_coordinator
