// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/tab_underline_view_controller.h"

#include "base/debug/crash_logging.h"
#include "chrome/browser/glic/browser_ui/tab_underline_view.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tabs/tab.h"

namespace glic {

TabUnderlineViewController::TabUnderlineViewController(
    Browser* browser,
    TabUnderlineView* underline_view)
    : underline_view_(underline_view), browser_(browser) {
  auto* glic_service = GetGlicKeyedService();
  GlicSharingManager& sharing_manager = glic_service->sharing_manager();

  if (!GlicEnabling::IsMultiInstanceEnabledByFlags()) {
    // Subscribe to changes in the focused tab.
    focus_change_subscription_ = sharing_manager.AddFocusedTabChangedCallback(
        base::BindRepeating(&TabUnderlineViewController::OnFocusedTabChanged,
                            base::Unretained(this)));
    // Subscribe to changes in the context access indicator status.
    indicator_change_subscription_ =
        glic_service->AddContextAccessIndicatorStatusChangedCallback(
            base::BindRepeating(
                &TabUnderlineViewController::OnIndicatorStatusChanged,
                base::Unretained(this)));

    // Observe changes in the floaty state.
    glic_service->GetSingleInstanceWindowController().AddStateObserver(this);
  }

  // Subscribe to changes in the set of pinned tabs.
  pinned_tabs_change_subscription_ =
      sharing_manager.AddPinnedTabsChangedCallback(
          base::BindRepeating(&TabUnderlineViewController::OnPinnedTabsChanged,
                              base::Unretained(this)));

  // Subscribe to when new requests are made by glic.
  user_input_submitted_subscription_ =
      glic_service->AddUserInputSubmittedCallback(
          base::BindRepeating(&TabUnderlineViewController::OnUserInputSubmitted,
                              base::Unretained(this)));
}

TabUnderlineViewController::~TabUnderlineViewController() {
  if (!GlicEnabling::IsMultiInstanceEnabledByFlags()) {
    GetGlicKeyedService()
        ->GetSingleInstanceWindowController()
        .RemoveStateObserver(this);
  }
}

void TabUnderlineViewController::OnFocusedTabChanged(
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

void TabUnderlineViewController::OnIndicatorStatusChanged(bool enabled) {
  if (context_access_indicator_enabled_ == enabled) {
    return;
  }
  context_access_indicator_enabled_ = enabled;
  UpdateUnderlineView(context_access_indicator_enabled_
                          ? UpdateUnderlineReason::kContextAccessIndicatorOn
                          : UpdateUnderlineReason::kContextAccessIndicatorOff);
}

void TabUnderlineViewController::OnPinnedTabsChanged(
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

void TabUnderlineViewController::PanelStateChanged(
    const glic::mojom::PanelState& panel_state,
    const GlicWindowController::PanelStateContext& context) {
  UpdateUnderlineView(
      panel_state.kind == mojom::PanelStateKind::kHidden
          ? UpdateUnderlineReason::kPanelStateChanged_PanelHidden
          : UpdateUnderlineReason::kPanelStateChanged_PanelShowing);
}

void TabUnderlineViewController::OnUserInputSubmitted() {
  UpdateUnderlineView(UpdateUnderlineReason::kUserInputSubmitted);
}

GlicKeyedService* TabUnderlineViewController::GetGlicKeyedService() {
  return GlicKeyedServiceFactory::GetGlicKeyedService(browser_->GetProfile());
}

base::WeakPtr<tabs::TabInterface>
TabUnderlineViewController::GetTabInterface() {
  if (underline_view_ && underline_view_->tab_) {
    if (auto tab_interface = underline_view_->tab_->data().tab_interface) {
      return tab_interface;
    }
  }
  return nullptr;
}

bool TabUnderlineViewController::IsUnderlineTabPinned() {
  if (auto tab_interface = GetTabInterface()) {
    if (auto* glic_service = GetGlicKeyedService()) {
      return glic_service->sharing_manager().IsTabPinned(
          tab_interface->GetHandle());
    }
  }
  return false;
}

bool TabUnderlineViewController::IsUnderlineTabSharedThroughActiveFollow() {
  if (auto tab_interface = GetTabInterface()) {
    if (auto* glic_service = GetGlicKeyedService()) {
      return (glic_service->sharing_manager().GetFocusedTabData().focus() ==
              tab_interface.get()) &&
             context_access_indicator_enabled_;
    }
  }
  return false;
}

void TabUnderlineViewController::UpdateUnderlineView(
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
                        IsGlicWindowShowing());

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
          (GlicEnabling::IsMultiInstanceEnabledByFlags() ||
           IsGlicWindowShowing())) {
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
      if (GlicEnabling::IsMultiInstanceEnabledByFlags()) {
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
  }
}

void TabUnderlineViewController::ShowAndAnimateUnderline() {
  underline_view_->StopShowing();
  underline_view_->Show();
}

void TabUnderlineViewController::HideUnderline() {
  if (underline_view_->IsShowing()) {
    underline_view_->StartRampingDown();
  }
}

void TabUnderlineViewController::AnimateUnderline() {
  underline_view_->ResetAnimationCycle();
}

void TabUnderlineViewController::ShowOrAnimatePinnedUnderline() {
  if (!IsUnderlineTabPinned()) {
    return;
  }
  // For multi-instance, we rely on the umbrella sharing manager behavior to
  // determine when to show or not show underlines via the pinned tabs api.
  if (!GlicEnabling::IsMultiInstanceEnabledByFlags()) {
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

bool TabUnderlineViewController::IsGlicWindowShowing() const {
  return underline_view_->GetGlicService()->IsWindowShowing();
}

bool TabUnderlineViewController::IsTabInCurrentWindow(
    const content::WebContents* tab) const {
  auto* model = browser_->GetTabStripModel();
  CHECK(model);
  int index = model->GetIndexOfWebContents(tab);
  return index != TabStripModel::kNoTab;
}

std::string TabUnderlineViewController::UpdateReasonToString(
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
    case UpdateUnderlineReason::kPanelStateChanged_PanelShowing:
      return "PanelShowing";
    case UpdateUnderlineReason::kPanelStateChanged_PanelHidden:
      return "PanelHidden";
    case UpdateUnderlineReason::kUserInputSubmitted:
      return "UserInputSubmitted";
  }
}

void TabUnderlineViewController::AddReasonForDebugging(
    UpdateUnderlineReason reason) {
  underline_update_reasons_.push_back(UpdateReasonToString(reason));
  if (underline_update_reasons_.size() > kNumReasonsToKeep) {
    underline_update_reasons_.pop_front();
  }
}

std::string TabUnderlineViewController::UpdateReasonsToString() const {
  std::ostringstream oss;
  for (const auto& r : underline_update_reasons_) {
    oss << r << ",";
  }
  return oss.str();
}

}  // namespace glic
