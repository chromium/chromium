// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/tab_underline_view_controller_impl.h"

#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "chrome/browser/glic/browser_ui/tab_underline_view.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "components/contextual_tasks/public/features.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

TabUnderlineViewControllerImpl::TabUnderlineViewControllerImpl() = default;

TabUnderlineViewControllerImpl::~TabUnderlineViewControllerImpl() {
  if (glic_service_ && !GlicEnabling::IsMultiInstanceEnabled()) {
    glic_service_->GetSingleInstanceWindowController().RemoveStateObserver(
        this);
  }
}

// This implementation makes many references to "pinned" tabs. All of these
// refer to tabs that are selected to be shared with Gemini under the glic
// multitab feature. This is different from the older existing notion of
// "pinned" tabs in the tabstrip, which is the UI treatment that fixes a Tab
// view to one side with a reduced visual. Separate terminology should be used
// for the glic multitab concept in order to disambiguate, but landed code
// already adopts the "pinning" term and so that continues to be used here.
// TODO(crbug.com/433131600): update glic multitab sharing code to use less
// conflicting terminology.
void TabUnderlineViewControllerImpl::Initialize(
    TabUnderlineView* underline_view,
    Browser* browser) {
  underline_view_ = underline_view;
  browser_ = browser;

  if (ShouldUseSignalsForGlicUnderlines()) {
    glic_service_ =
        GlicKeyedServiceFactory::GetGlicKeyedService(browser_->GetProfile());

    GlicSharingManager& sharing_manager = glic_service_->sharing_manager();

    if (!GlicEnabling::IsMultiInstanceEnabled()) {
      // Subscribe to changes in the focused tab.
      focus_change_subscription_ =
          sharing_manager.AddFocusedTabChangedCallback(base::BindRepeating(
              &TabUnderlineViewControllerImpl::OnFocusedTabChanged,
              base::Unretained(this)));
      // Subscribe to changes in the context access indicator status.
      indicator_change_subscription_ =
          glic_service_->AddContextAccessIndicatorStatusChangedCallback(
              base::BindRepeating(
                  &TabUnderlineViewControllerImpl::OnIndicatorStatusChanged,
                  base::Unretained(this)));

      // Observe changes in the floaty state.
      glic_service_->GetSingleInstanceWindowController().AddStateObserver(this);
    }

    // Subscribe to changes in the set of pinned tabs.
    pinned_tabs_change_subscription_ =
        sharing_manager.AddPinnedTabsChangedCallback(base::BindRepeating(
            &TabUnderlineViewControllerImpl::OnPinnedTabsChanged,
            base::Unretained(this)));

    // Subscribe to when new requests are made by glic.
    user_input_submitted_subscription_ =
        glic_service_->AddUserInputSubmittedCallback(base::BindRepeating(
            &TabUnderlineViewControllerImpl::OnUserInputSubmitted,
            base::Unretained(this)));
  }

  if (ShouldUseSignalsForContextualTasks()) {
    contextual_tasks::ActiveTaskContextProvider* active_task_context_provider =
        browser_->browser_window_features()
            ->contextual_tasks_active_task_context_provider();
    contextual_task_observation_.Observe(active_task_context_provider);
  }

  if (glic_service_) {
    // Fetch the latest context access indicator status from service. We can't
    // assume the WebApp always updates the status on the service (thus the new
    // subscribers not getting the latest value).
    OnIndicatorStatusChanged(
        glic_service_->is_context_access_indicator_enabled());
  }
}

void TabUnderlineViewControllerImpl::OnFocusedTabChanged(
    const FocusedTabData& focused_tab_data) {
  tabs::TabInterface* tab = focused_tab_data.focus();
  auto* previous_focus = glic_current_focused_contents_.get();

  if (tab) {
    glic_current_focused_contents_ = tab->GetContents()->GetWeakPtr();
  } else {
    glic_current_focused_contents_.reset();
  }
  auto* current_focus = glic_current_focused_contents_.get();

  base::WeakPtr<content::WebContents> underline_contents;
  if (auto tab_interface = GetTabInterface()) {
    underline_contents = tab_interface->GetContents()->GetWeakPtr();
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

void TabUnderlineViewControllerImpl::OnIndicatorStatusChanged(bool enabled) {
  if (context_access_indicator_enabled_ == enabled) {
    return;
  }
  context_access_indicator_enabled_ = enabled;
  UpdateUnderlineView(context_access_indicator_enabled_
                          ? UpdateUnderlineReason::kContextAccessIndicatorOn
                          : UpdateUnderlineReason::kContextAccessIndicatorOff);
}

void TabUnderlineViewControllerImpl::OnPinnedTabsChanged(
    const std::vector<content::WebContents*>& pinned_contents) {
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

void TabUnderlineViewControllerImpl::OnContextTabsChanged(
    const std::set<tabs::TabHandle>& context_tabs) {
  auto tab_interface = GetTabInterface();
  if (!tab_interface) {
    // If the TabInterface is invalid at this point, there is no relevant UI
    // to handle.
    return;
  }

  bool should_underline =
      base::Contains(context_tabs, tab_interface->GetHandle());
  UpdateUnderlineView(
      should_underline
          ? UpdateUnderlineReason::kContextualTask_TabInContext
          : UpdateUnderlineReason::kContextualTask_TabNotInContext);
}

void TabUnderlineViewControllerImpl::PanelStateChanged(
    const glic::mojom::PanelState& panel_state,
    const GlicWindowController::PanelStateContext& context) {
  UpdateUnderlineView(
      panel_state.kind == mojom::PanelStateKind::kHidden
          ? UpdateUnderlineReason::kPanelStateChanged_PanelHidden
          : UpdateUnderlineReason::kPanelStateChanged_PanelShowing);
}

void TabUnderlineViewControllerImpl::OnUserInputSubmitted() {
  UpdateUnderlineView(UpdateUnderlineReason::kUserInputSubmitted);
}

base::WeakPtr<tabs::TabInterface>
TabUnderlineViewControllerImpl::GetTabInterface() {
  return underline_view_ ? underline_view_->GetTabInterface() : nullptr;
}

bool TabUnderlineViewControllerImpl::IsUnderlineTabPinned() {
  if (auto tab_interface = GetTabInterface()) {
    return glic_service_ && glic_service_->sharing_manager().IsTabPinned(
                                tab_interface->GetHandle());
  }
  return false;
}

bool TabUnderlineViewControllerImpl::IsUnderlineTabSharedThroughActiveFollow() {
  if (!glic_service_) {
    return false;
  }

  if (auto tab_interface = GetTabInterface()) {
    return (glic_service_->sharing_manager().GetFocusedTabData().focus() ==
            tab_interface.get()) &&
           context_access_indicator_enabled_;
  }
  return false;
}

void TabUnderlineViewControllerImpl::UpdateUnderlineView(
    UpdateUnderlineReason reason) {
  AddReasonForDebugging(reason);
  auto reasons_string = UpdateReasonsToString();
  SCOPED_CRASH_KEY_STRING1024("crbug-398319435", "update_reasons",
                              reasons_string);
  SCOPED_CRASH_KEY_BOOL("crbug-398319435", "access_indicator",
                        context_access_indicator_enabled_);
  SCOPED_CRASH_KEY_BOOL("crbug-398319435", "glic_focused_contents",
                        !!glic_current_focused_contents_);
  SCOPED_CRASH_KEY_BOOL("crbug-398319435", "is_glic_window_showing",
                        glic_service_ && IsGlicWindowShowing());

  switch (reason) {
    case UpdateUnderlineReason::kContextAccessIndicatorOn: {
      // Active follow tab underline should be newly shown, pinned tabs should
      // re-animate or be newly shown if not already visible.
      if (IsUnderlineTabSharedThroughActiveFollow()) {
        ShowAndAnimateUnderline();
      }
      ShowOrAnimatePinnedUnderline();
      break;
    }
    case UpdateUnderlineReason::kContextAccessIndicatorOff: {
      // Underline should be hidden, with exception to pinned tabs while the
      // glic panel remains open.
      if (IsUnderlineTabPinned() &&
          (GlicEnabling::IsMultiInstanceEnabled() || IsGlicWindowShowing())) {
        break;
      }
      HideUnderline();
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
        ShowAndAnimateUnderline();
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
        HideUnderline();
      }
      break;
    }
    case UpdateUnderlineReason::kFocusedTabChanged_ChromeGainedFocus:
      // Active follow tab underline should be newly shown, pinned tabs should
      // re-animate or be newly shown if not already visible.
      if (IsUnderlineTabSharedThroughActiveFollow()) {
        ShowAndAnimateUnderline();
      }
      ShowOrAnimatePinnedUnderline();
      break;
    case UpdateUnderlineReason::kFocusedTabChanged_ChromeLostFocus:
      // Underline should be hidden, with exception to pinned tabs.
      if (!IsUnderlineTabPinned()) {
        HideUnderline();
      }
      break;
    case UpdateUnderlineReason::kPinnedTabsChanged_TabInPinnedSet:
      if (GlicEnabling::IsMultiInstanceEnabled()) {
        ShowAndAnimateUnderline();
      } else {
        // If `underline_view_` is not visible, then this tab was just added
        // to the set of pinned tabs.
        if (!underline_view_->IsShowing()) {
          // Pinned tab underlines should only be visible while the glic panel
          // is open. For multi-instance this is controlled via the pinned
          // tabs api.
          if (IsGlicWindowShowing()) {
            ShowAndAnimateUnderline();
          }
        } else {
          // This tab was already pinned - re-animate to reflect the change in
          // the set of pinned tabs.
          AnimateUnderline();
        }
      }
      break;
    case UpdateUnderlineReason::kPinnedTabsChanged_TabNotInPinnedSet:
      // Re-animate to reflect the change in the set of pinned tabs.
      if (IsUnderlineTabSharedThroughActiveFollow()) {
        AnimateUnderline();
        return;
      }
      // This tab may have just been removed from the pinned set.
      HideUnderline();
      break;
    case UpdateUnderlineReason::kPanelStateChanged_PanelShowing:
      // Visibility of underlines of pinned tabs should follow visibility of
      // the glic panel.
      if (IsUnderlineTabPinned()) {
        ShowAndAnimateUnderline();
      }
      break;
    case UpdateUnderlineReason::kPanelStateChanged_PanelHidden:
      // Visibility of underlines of pinned tabs should follow visibility of
      // the glic panel.
      if (IsUnderlineTabPinned()) {
        HideUnderline();
      }
      break;
    case UpdateUnderlineReason::kUserInputSubmitted:
      if (underline_view_->IsShowing()) {
        AnimateUnderline();
      }
      break;
    case UpdateUnderlineReason::kContextualTask_TabInContext:
      if (!underline_view_->IsShowing()) {
        ShowAndAnimateUnderline();
      }
      break;
    case UpdateUnderlineReason::kContextualTask_TabNotInContext:
      HideUnderline();
      break;
  }
}

void TabUnderlineViewControllerImpl::ShowAndAnimateUnderline() {
  underline_view_->StopShowing();
  underline_view_->Show();
}

void TabUnderlineViewControllerImpl::HideUnderline() {
  if (underline_view_->IsShowing()) {
    underline_view_->StartRampingDown();
  }
}

void TabUnderlineViewControllerImpl::AnimateUnderline() {
  if (!underline_view_->IsShowing()) {
    // There is be a chance that the underline view has already stopped showing.
    // In that case, gracefully handle the crash case in crbug.com/398319435 by
    // closing(minimizing) the glic window.
    glic_service_->window_controller().Close();
  }

  underline_view_->ResetAnimationCycle();
}

void TabUnderlineViewControllerImpl::ShowOrAnimatePinnedUnderline() {
  if (!IsUnderlineTabPinned()) {
    return;
  }
  // For multi-instance, we rely on the umbrella sharing manager behavior to
  // determine when to show or not show underlines via the pinned tabs api.
  if (!GlicEnabling::IsMultiInstanceEnabled()) {
    // Pinned underlines should never be visible if the glic window is closed.
    if (!IsGlicWindowShowing()) {
      return;
    }
  }
  if (underline_view_->IsShowing()) {
    AnimateUnderline();
  } else {
    ShowAndAnimateUnderline();
  }
}

bool TabUnderlineViewControllerImpl::IsGlicWindowShowing() const {
  return glic_service_ && glic_service_->IsWindowShowing();
}

bool TabUnderlineViewControllerImpl::IsTabInCurrentWindow(
    const content::WebContents* tab) const {
  auto* model = browser_->GetTabStripModel();
  CHECK(model);
  int index = model->GetIndexOfWebContents(tab);
  return index != TabStripModel::kNoTab;
}

std::string TabUnderlineViewControllerImpl::UpdateReasonToString(
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
    case UpdateUnderlineReason::kPanelStateChanged_PanelShowing:
      return "PanelShowing";
    case UpdateUnderlineReason::kPanelStateChanged_PanelHidden:
      return "PanelHidden";
    case UpdateUnderlineReason::kUserInputSubmitted:
      return "UserInputSubmitted";
  }
}

void TabUnderlineViewControllerImpl::AddReasonForDebugging(
    UpdateUnderlineReason reason) {
  underline_update_reasons_.push_back(UpdateReasonToString(reason));
  if (underline_update_reasons_.size() > kNumReasonsToKeep) {
    underline_update_reasons_.pop_front();
  }
}

std::string TabUnderlineViewControllerImpl::UpdateReasonsToString() const {
  std::ostringstream oss;
  for (const auto& r : underline_update_reasons_) {
    oss << r << ",";
  }
  return oss.str();
}

bool TabUnderlineViewControllerImpl::ShouldUseSignalsForGlicUnderlines() {
  return base::FeatureList::IsEnabled(features::kGlicMultitabUnderlines) &&
         glic::GlicEnabling::IsProfileEligible(browser_->GetProfile());
}

bool TabUnderlineViewControllerImpl::ShouldUseSignalsForContextualTasks() {
  return base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks);
}

}  // namespace glic
