// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/tab_underline_controller.h"

#include "base/debug/crash_logging.h"
#include "build/build_config.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "components/contextual_tasks/public/features.h"
#include "components/tabs/public/tab_interface.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/dictation/dictation_keyed_service.h"
#endif

namespace glic {

TabUnderlineController::TabUnderlineController(tabs::TabHandle tab_handle)
    : tab_handle_(tab_handle) {}

TabUnderlineController::~TabUnderlineController() = default;

// This implementation makes many references to "pinned" tabs. All of these
// refer to tabs that are selected to be shared with Gemini under the glic
// multitab feature. This is different from the older existing notion of
// "pinned" tabs in the tabstrip, which is the UI treatment that fixes a Tab
// view to one side with a reduced visual. Separate terminology should be used
// for the glic multitab concept in order to disambiguate, but landed code
// already adopts the "pinning" term and so that continues to be used here.
// TODO(crbug.com/433131600): update glic multitab sharing code to use less
// conflicting terminology.
void TabUnderlineController::Initialize(UiDelegate* ui_delegate) {
  ui_delegate_ = ui_delegate;

  if (ShouldUseSignalsForGlicUnderlines()) {
    tabs::TabInterface* tab = GetTabInterface();
    CHECK(tab);
    glic_service_ =
        GlicKeyedServiceFactory::GetGlicKeyedService(tab->GetProfile());

    GlicSharingManagerInternal& sharing_manager =
        glic_service_->active_instance_sharing_manager();

    // Subscribe to changes in the set of pinned tabs.
    pinned_tabs_change_subscription_ =
        sharing_manager.AddPinnedTabsChangedCallback(
            base::BindRepeating(&TabUnderlineController::OnPinnedTabsChanged,
                                base::Unretained(this)));

    // Subscribe to when new requests are made by glic.
    user_input_submitted_subscription_ =
        glic_service_->AddUserInputSubmittedCallback(
            base::BindRepeating(&TabUnderlineController::OnUserInputSubmitted,
                                base::Unretained(this)));
  }

  MaybeObserveContextualTasks();
  MaybeObserveDictation();

  if (glic_service_) {
    // Fetch the latest context access indicator status from service. We can't
    // assume the WebApp always updates the status on the service (thus the new
    // subscribers not getting the latest value).
    OnIndicatorStatusChanged(
        glic_service_->is_context_access_indicator_enabled());
  }
}

void TabUnderlineController::OnUiReady() {
  if (!glic_service_ || !ShouldUseSignalsForGlicUnderlines()) {
    return;
  }

  // During cases such as tabstrip attachment, the underline controller consumes
  // changes in pinned state before the underline view is reconstructed. Check
  // consistency with pinned state post-construction to ensure the UI state of
  // the underline is correct.
  OnPinnedTabsChanged(
      glic_service_->active_instance_sharing_manager().GetPinnedTabs());
}

void TabUnderlineController::OnFocusedTabChanged(
    const FocusedTabData& focused_tab_data) {
  tabs::TabInterface* tab = focused_tab_data.focus();
  auto* previous_focus = glic_current_focused_contents_.get();

  if (tab && tab->GetContents()) {
    glic_current_focused_contents_ = tab->GetContents()->GetWeakPtr();
  } else {
    glic_current_focused_contents_.reset();
  }
  auto* current_focus = glic_current_focused_contents_.get();

  base::WeakPtr<content::WebContents> underline_contents;
  if (auto* tab_interface = GetTabInterface()) {
    if (auto* contents = tab_interface->GetContents()) {
      underline_contents = contents->GetWeakPtr();
    }
  } else {
    return;
  }

  bool focus_changed = previous_focus != current_focus;
  bool tab_switch =
      previous_focus && glic_current_focused_contents_ && focus_changed;
  bool this_tab_gained_focus =
      (underline_contents.get() == current_focus) && focus_changed;
  bool this_tab_lost_focus =
      (underline_contents.get() == previous_focus) && focus_changed;

  bool window_gained_focus = !previous_focus && glic_current_focused_contents_;
  bool window_lost_focus = previous_focus && !glic_current_focused_contents_;

  if (tab_switch) {
    if (this_tab_gained_focus) {
      UpdateUnderlineView(
          UpdateUnderlineReason::kFocusedTabChanged_TabGainedFocus);
    } else if (this_tab_lost_focus) {
      UpdateUnderlineView(
          UpdateUnderlineReason::kFocusedTabChanged_TabLostFocus);
    } else {
      UpdateUnderlineView(
          UpdateUnderlineReason::kFocusedTabChanged_NoFocusChange);
    }
  } else {
    if (window_gained_focus) {
      UpdateUnderlineView(
          UpdateUnderlineReason::kFocusedTabChanged_ChromeGainedFocus);
    } else if (window_lost_focus) {
      UpdateUnderlineView(
          UpdateUnderlineReason::kFocusedTabChanged_ChromeLostFocus);
    }
  }
}

void TabUnderlineController::OnIndicatorStatusChanged(bool enabled) {
  if (context_access_indicator_enabled_ == enabled) {
    return;
  }
  context_access_indicator_enabled_ = enabled;
  UpdateUnderlineView(context_access_indicator_enabled_
                          ? UpdateUnderlineReason::kContextAccessIndicatorOn
                          : UpdateUnderlineReason::kContextAccessIndicatorOff);
}

void TabUnderlineController::OnPinnedTabsChanged(
    const std::vector<tabs::TabInterface*>& pinned_tabs) {
  if (!GetTabInterface()) {
    // If the TabInterface is invalid at this point, there is no relevant UI
    // to handle.
    return;
  }

  // Triggering is handled based on whether the tab is in the pinned set.
  if (IsUnderlineTabPinned()) {
    UpdateUnderlineView(
        UpdateUnderlineReason::kPinnedTabsChanged_TabInPinnedSet);
    return;
  }
  UpdateUnderlineView(
      UpdateUnderlineReason::kPinnedTabsChanged_TabNotInPinnedSet);
}

void TabUnderlineController::OnContextTabsChanged(
    const std::set<tabs::TabHandle>& context_tabs) {
  auto* tab_interface = GetTabInterface();
  if (!tab_interface) {
    // If the TabInterface is invalid at this point, there is no relevant UI
    // to handle.
    return;
  }

  bool should_underline = context_tabs.contains(tab_interface->GetHandle());
  UpdateUnderlineView(
      should_underline
          ? UpdateUnderlineReason::kContextualTask_TabInContext
          : UpdateUnderlineReason::kContextualTask_TabNotInContext);
}

void TabUnderlineController::OnActiveTaskContextProviderDestroyed() {
  contextual_task_observation_.Reset();
}

void TabUnderlineController::OnDictationTabChanged(
    tabs::TabInterface* active_dictation_tab) {
  tabs::TabInterface* this_tab = GetTabInterface();
  if (!this_tab) {
    return;
  }

  UpdateUnderlineView(active_dictation_tab == this_tab
                          ? UpdateUnderlineReason::kDictation_TabActive
                          : UpdateUnderlineReason::kDictation_TabInactive);
}

void TabUnderlineController::PanelStateChanged(
    const glic::mojom::PanelState& panel_state) {
  UpdateUnderlineView(
      panel_state.kind == mojom::PanelStateKind::kHidden
          ? UpdateUnderlineReason::kPanelStateChanged_PanelHidden
          : UpdateUnderlineReason::kPanelStateChanged_PanelShowing);
}

void TabUnderlineController::OnUserInputSubmitted() {
  UpdateUnderlineView(UpdateUnderlineReason::kUserInputSubmitted);
}

tabs::TabInterface* TabUnderlineController::GetTabInterface() {
  return tab_handle_.Get();
}

bool TabUnderlineController::IsUnderlineTabPinned() {
  if (auto* tab_interface = GetTabInterface()) {
    return glic_service_ &&
           glic_service_->active_instance_sharing_manager().IsTabPinned(
               tab_interface->GetHandle());
  }
  return false;
}

bool TabUnderlineController::IsUnderlineTabSharedThroughActiveFollow() {
  if (!glic_service_) {
    return false;
  }

  if (auto* tab_interface = GetTabInterface()) {
    return (glic_service_->active_instance_sharing_manager()
                .GetFocusedTabData()
                .focus() == tab_interface) &&
           context_access_indicator_enabled_;
  }
  return false;
}

void TabUnderlineController::UpdateUnderlineView(UpdateUnderlineReason reason) {
  AddReasonForDebugging(reason);
  auto reasons_string = UpdateReasonsToString();
  SCOPED_CRASH_KEY_STRING1024("crbug-398319435", "update_reasons",
                              reasons_string);
  SCOPED_CRASH_KEY_BOOL("crbug-398319435", "access_indicator",
                        context_access_indicator_enabled_);
  SCOPED_CRASH_KEY_BOOL("crbug-398319435", "glic_focused_contents",
                        !!glic_current_focused_contents_);
  SCOPED_CRASH_KEY_BOOL("crbug-398319435", "is_glic_window_showing",
                        glic_service_ && IsAnyGlicPanelShowing());

  switch (reason) {
    case UpdateUnderlineReason::kContextAccessIndicatorOn: {
      // Active follow tab underline should be newly shown, pinned tabs should
      // re-animate or be newly shown if not already visible.
      if (IsUnderlineTabSharedThroughActiveFollow()) {
        ShowAndAnimateUnderline(UnderlineSource::kGlic);
      }
      ShowOrAnimatePinnedUnderline();
      break;
    }
    case UpdateUnderlineReason::kContextAccessIndicatorOff: {
      // Underline should be hidden, with exception to pinned tabs while the
      // glic panel remains open.
      if (IsUnderlineTabPinned()) {
        break;
      }
      HideUnderline(UnderlineSource::kGlic);
      break;
    }
    case UpdateUnderlineReason::kFocusedTabChanged_NoFocusChange: {
      // Pinned tab underlines should re-animate if active follow sharing is
      // on.
      if (context_access_indicator_enabled_ && IsUnderlineTabPinned()) {
        AnimateUnderline();
      }
      break;
    }
    case UpdateUnderlineReason::kFocusedTabChanged_TabGainedFocus: {
      // Underline visibility corresponds to the focused tab during active
      // follow. Pinned tabs should not react as the set of shared tabs has
      // not changed.
      if (IsUnderlineTabSharedThroughActiveFollow()) {
        ShowAndAnimateUnderline(UnderlineSource::kGlic);
      }
      break;
    }
    case UpdateUnderlineReason::kFocusedTabChanged_TabLostFocus: {
      // Underline visibility corresponds to the focused tab during active
      // follow. Pinned tabs should re-animate if the set of shared tabs has
      // changed
      if (IsUnderlineTabPinned() && context_access_indicator_enabled_) {
        AnimateUnderline();
      } else if (!IsUnderlineTabPinned()) {
        HideUnderline(UnderlineSource::kGlic);
      }
      break;
    }
    case UpdateUnderlineReason::kFocusedTabChanged_ChromeGainedFocus:
      // Active follow tab underline should be newly shown, pinned tabs should
      // re-animate or be newly shown if not already visible.
      if (IsUnderlineTabSharedThroughActiveFollow()) {
        ShowAndAnimateUnderline(UnderlineSource::kGlic);
      }
      ShowOrAnimatePinnedUnderline();
      break;
    case UpdateUnderlineReason::kFocusedTabChanged_ChromeLostFocus:
      // Underline should be hidden, with exception to pinned tabs.
      if (!IsUnderlineTabPinned()) {
        HideUnderline(UnderlineSource::kGlic);
      }
      break;
    case UpdateUnderlineReason::kPinnedTabsChanged_TabInPinnedSet:
      ShowAndAnimateUnderline(UnderlineSource::kGlic);
      break;
    case UpdateUnderlineReason::kPinnedTabsChanged_TabNotInPinnedSet:
      // Re-animate to reflect the change in the set of pinned tabs.
      if (IsUnderlineTabSharedThroughActiveFollow()) {
        AnimateUnderline();
        return;
      }
      // This tab may have just been removed from the pinned set.
      HideUnderline(UnderlineSource::kGlic);
      break;
    case UpdateUnderlineReason::kPanelStateChanged_PanelShowing:
      // Visibility of underlines of pinned tabs should follow visibility of
      // the glic panel.
      if (IsUnderlineTabPinned()) {
        ShowAndAnimateUnderline(UnderlineSource::kGlic);
      }
      break;
    case UpdateUnderlineReason::kPanelStateChanged_PanelHidden:
      // Visibility of underlines of pinned tabs should follow visibility of
      // the glic panel.
      if (IsUnderlineTabPinned()) {
        HideUnderline(UnderlineSource::kGlic);
      }
      break;
    case UpdateUnderlineReason::kUserInputSubmitted:
      if (ui_delegate_->IsShowing()) {
        AnimateUnderline();
      }
      break;
    case UpdateUnderlineReason::kContextualTask_TabInContext:
      ShowAndAnimateUnderline(UnderlineSource::kContextualTasks);
      break;
    case UpdateUnderlineReason::kContextualTask_TabNotInContext:
      HideUnderline(UnderlineSource::kContextualTasks);
      break;
    case UpdateUnderlineReason::kDictation_TabActive:
      ShowAndAnimateUnderline(UnderlineSource::kDictation);
      break;
    case UpdateUnderlineReason::kDictation_TabInactive:
      HideUnderline(UnderlineSource::kDictation);
      break;
  }
}

void TabUnderlineController::ShowAndAnimateUnderline(UnderlineSource source) {
  AddSource(source);
  ui_delegate_->StopShowing();
  ui_delegate_->Show();
}

void TabUnderlineController::HideUnderline(UnderlineSource source) {
  RemoveSource(source);
  if (active_sources_ != UnderlineSource::kNone) {
    return;
  }

  // TODO(crbug.com/467739947): Consider reenabling hide animation for
  // contextual tasks.
  if (source == UnderlineSource::kGlic) {
    ui_delegate_->StartRampingDown();
  } else {
    ui_delegate_->StopShowing();
  }
}

void TabUnderlineController::AddSource(UnderlineSource source) {
  active_sources_ |= source;
}

void TabUnderlineController::RemoveSource(UnderlineSource source) {
  active_sources_ &= ~source;
}

void TabUnderlineController::AnimateUnderline() {
  if (!ui_delegate_->IsShowing()) {
    return;
  }

  ui_delegate_->ResetAnimationCycle();
}

void TabUnderlineController::ShowOrAnimatePinnedUnderline() {
  if (!IsUnderlineTabPinned()) {
    return;
  }
  // Pinned underlines should never be visible if the glic window is closed.
  if (!IsAnyGlicPanelShowing()) {
    return;
  }
  if (ui_delegate_->IsShowing()) {
    AnimateUnderline();
  } else {
    ShowAndAnimateUnderline(UnderlineSource::kGlic);
  }
}

bool TabUnderlineController::IsAnyGlicPanelShowing() const {
  return glic_service_ &&
         glic_service_->instance_coordinator().IsAnyPanelShowing();
}

std::string TabUnderlineController::UpdateReasonToString(
    UpdateUnderlineReason reason) {
  switch (reason) {
    case UpdateUnderlineReason::kContextAccessIndicatorOn:
      return "IndicatorOn";
    case UpdateUnderlineReason::kContextAccessIndicatorOff:
      return "IndicatorOff";
    case UpdateUnderlineReason::kFocusedTabChanged_NoFocusChange:
      return "TabFocusChange";
    case UpdateUnderlineReason::kFocusedTabChanged_TabGainedFocus:
      return "TabGainedFocus";
    case UpdateUnderlineReason::kFocusedTabChanged_TabLostFocus:
      return "TabLostFocus";
    case UpdateUnderlineReason::kFocusedTabChanged_ChromeGainedFocus:
      return "ChromeGainedFocus";
    case UpdateUnderlineReason::kFocusedTabChanged_ChromeLostFocus:
      return "ChromeLostFocus";
    case UpdateUnderlineReason::kPinnedTabsChanged_TabInPinnedSet:
      return "TabInPinnedSet";
    case UpdateUnderlineReason::kPinnedTabsChanged_TabNotInPinnedSet:
      return "TabNotInPinnedSet";
    case UpdateUnderlineReason::kContextualTask_TabInContext:
      return "TabInContext";
    case UpdateUnderlineReason::kContextualTask_TabNotInContext:
      return "TabNotInContext";
    case UpdateUnderlineReason::kDictation_TabActive:
      return "DictationTabActive";
    case UpdateUnderlineReason::kDictation_TabInactive:
      return "DictationTabInactive";
    case UpdateUnderlineReason::kPanelStateChanged_PanelShowing:
      return "PanelShowing";
    case UpdateUnderlineReason::kPanelStateChanged_PanelHidden:
      return "PanelHidden";
    case UpdateUnderlineReason::kUserInputSubmitted:
      return "UserInputSubmitted";
  }
}

void TabUnderlineController::AddReasonForDebugging(
    UpdateUnderlineReason reason) {
  underline_update_reasons_.push_back(UpdateReasonToString(reason));
  if (underline_update_reasons_.size() > kNumReasonsToKeep) {
    underline_update_reasons_.pop_front();
  }
}

std::string TabUnderlineController::UpdateReasonsToString() const {
  std::ostringstream oss;
  for (const auto& r : underline_update_reasons_) {
    oss << r << ",";
  }
  return oss.str();
}

bool TabUnderlineController::ShouldUseSignalsForGlicUnderlines() {
  tabs::TabInterface* tab = GetTabInterface();
  if (!tab) {
    return false;
  }
  return glic::GlicEnabling::IsProfileEligible(tab->GetProfile());
}

bool TabUnderlineController::ShouldUseSignalsForContextualTasks() {
  return contextual_tasks::IsContextualTasksUIEnabled();
}

BrowserWindowInterface* TabUnderlineController::GetBrowserWindowInterface() {
  tabs::TabInterface* tab = GetTabInterface();
  return tab ? tab->GetBrowserWindowInterface() : nullptr;
}

void TabUnderlineController::MaybeObserveContextualTasks() {
  BrowserWindowInterface* window = GetBrowserWindowInterface();
  if (!window) {
    return;
  }
  if (ShouldUseSignalsForContextualTasks() &&
      !contextual_task_observation_.IsObserving()) {
    if (auto* active_task_context_provider =
            contextual_tasks::ActiveTaskContextProvider::From(window)) {
      contextual_task_observation_.Observe(active_task_context_provider);
    }
  }
}

void TabUnderlineController::MaybeObserveDictation() {
#if !BUILDFLAG(IS_ANDROID)
  tabs::TabInterface* tab = GetTabInterface();
  if (!tab || dictation_tab_changed_subscription_) {
    return;
  }
  if (auto* dictation_service =
          dictation::DictationKeyedService::Get(tab->GetProfile())) {
    dictation_tab_changed_subscription_ =
        dictation_service->AddDictationTabChangedCallback(
            base::BindRepeating(&TabUnderlineController::OnDictationTabChanged,
                                base::Unretained(this)));
  }
#endif
}

}  // namespace glic
