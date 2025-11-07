// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/context_sharing_border_view_controller.h"

#include "base/debug/crash_logging.h"
#include "chrome/browser/actor/ui/actor_border_view_controller.h"
#include "chrome/browser/glic/browser_ui/context_sharing_border_view.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"

namespace glic {

ContextSharingBorderViewController::ContextSharingBorderViewController(
    ContextSharingBorderView* border_view,
    ContentsWebView* contents_web_view)
    : border_view_(border_view), contents_web_view_(contents_web_view) {
  auto* glic_service = border_view->GetGlicService();

  // Subscribe to glow updates from the actor border controller.
  if (features::kGlicActorUiBorderGlow.Get()) {
    actor_border_view_controller_subscription_ =
        ActorBorderViewController::From(border_view_->browser_)
            ->AddOnActorBorderGlowUpdatedCallback(base::BindRepeating(
                &ContextSharingBorderViewController::OnActorBorderGlowUpdated,
                base::Unretained(this)));
  }

  // Observe the contents web view for when it is deleting.
  contents_web_view_observation_.Observe(contents_web_view_);

  // Subscribe to changes in the focus tab.
  focus_change_subscription_ =
      glic_service->sharing_manager().AddFocusedTabChangedCallback(
          base::BindRepeating(
              &ContextSharingBorderViewController::OnFocusedTabChanged,
              base::Unretained(this)));

  // Subscribe to changes in the context access indicator status.
  indicator_change_subscription_ =
      glic_service->AddContextAccessIndicatorStatusChangedCallback(
          base::BindRepeating(
              &ContextSharingBorderViewController::OnIndicatorStatusChanged,
              base::Unretained(this)));
}

ContextSharingBorderViewController::~ContextSharingBorderViewController() =
    default;

void ContextSharingBorderViewController::OnFocusedTabChanged(
    const FocusedTabData& focused_tab_data) {
  tabs::TabInterface* tab = focused_tab_data.focus();
  auto* previous_focus = glic_focused_contents_in_current_view_.get();
  if (tab && IsTabInCurrentView(tab->GetContents())) {
    glic_focused_contents_in_current_view_ = tab->GetContents()->GetWeakPtr();
  } else {
    glic_focused_contents_in_current_view_.reset();
  }

  auto* current_focus = glic_focused_contents_in_current_view_.get();
  bool focus_changed = previous_focus != current_focus;

  bool tab_switch =
      previous_focus && glic_focused_contents_in_current_view_ && focus_changed;
  bool window_gained_focus =
      !previous_focus && glic_focused_contents_in_current_view_;
  bool window_lost_focus =
      previous_focus && !glic_focused_contents_in_current_view_;

  if (tab_switch) {
    MaybeRunBorderViewUpdate(
        UpdateBorderReason::kFocusedTabChanged_NoFocusChange);
  } else if (window_gained_focus) {
    MaybeRunBorderViewUpdate(UpdateBorderReason::kFocusedTabChanged_GainFocus);
  } else if (window_lost_focus) {
    MaybeRunBorderViewUpdate(UpdateBorderReason::kFocusedTabChanged_LostFocus);
  }
}

void ContextSharingBorderViewController::OnActorBorderGlowUpdated(
    tabs::TabInterface* tab,
    bool enabled) {
  if (!IsTabInCurrentView(tab->GetContents())) {
    return;
  }

  if (actor_border_glow_enabled_ == enabled) {
    return;
  }
  actor_border_glow_enabled_ = enabled;

  if (actor_border_glow_enabled_) {
    // Force the border to show, regardless of other states. This gives the
    // actor priority over other signals.
    border_view_->StopShowing();
    // If the standalone border glow param is enabled, don't actually just
    // suppress the glic_border_view from showing, as it is controlled by a
    // different component.
    if (!features::kGlicActorUiStandaloneBorderGlow.Get()) {
      border_view_->Show();
    }
  } else {
    // Revert to the last known state based on other signals like tab focus
    // or context access.
    if (last_mutating_update_reason_.has_value()) {
      UpdateBorderView(*last_mutating_update_reason_);
    } else {
      // No known state from before. We just ramp down.
      if (border_view_->IsShowing()) {
        border_view_->StartRampingDown();
      }
    }
  }
}

void ContextSharingBorderViewController::OnIndicatorStatusChanged(
    bool enabled) {
  if (context_access_indicator_enabled_ == enabled) {
    return;
  }
  context_access_indicator_enabled_ = enabled;
  MaybeRunBorderViewUpdate(
      context_access_indicator_enabled_
          ? UpdateBorderReason::kContextAccessIndicatorOn
          : UpdateBorderReason::kContextAccessIndicatorOff);
}

void ContextSharingBorderViewController::OnViewIsDeleting(
    views::View* observed_view) {
  contents_web_view_observation_.Reset();
  indicator_change_subscription_ = {};
  focus_change_subscription_ = {};
  actor_border_view_controller_subscription_ = {};
  contents_web_view_ = nullptr;
}

void ContextSharingBorderViewController::MaybeRunBorderViewUpdate(
    UpdateBorderReason reason) {
  // We only want to override the latest reason if it's one that would result
  // in showing vs hiding the border. `kFocusedTabChanged_NoFocusChange` only
  // replays an animation, it does not change the state.
  if (reason != UpdateBorderReason::kFocusedTabChanged_NoFocusChange) {
    last_mutating_update_reason_ = reason;
  }

  if (!actor_border_glow_enabled_) {
    UpdateBorderView(reason);
  }
}

void ContextSharingBorderViewController::UpdateBorderView(
    UpdateBorderReason reason) {
  AddReasonForDebugging(reason);
  auto reasons_string = UpdateReasonsToString();
  SCOPED_CRASH_KEY_STRING1024("crbug-398319435", "update_reasons",
                              reasons_string);
  SCOPED_CRASH_KEY_BOOL("crbug-398319435", "access_indicator",
                        context_access_indicator_enabled_);
  SCOPED_CRASH_KEY_BOOL("crbug-398319435", "glic_focused_contents",
                        !!glic_focused_contents_in_current_view_);
  SCOPED_CRASH_KEY_BOOL("crbug-398319435", "is_glic_window_showing",
                        IsGlicWindowShowing());

  switch (reason) {
    case UpdateBorderReason::kContextAccessIndicatorOn: {
      // Off to On. Throw away everything we have and start the animation from
      // the beginning.
      border_view_->StopShowing();
      if (ShouldShowBorderAnimation()) {
        border_view_->Show();
      }
      break;
    }
    case UpdateBorderReason::kContextAccessIndicatorOff: {
      if (border_view_->compositor_) {
        border_view_->StartRampingDown();
      }
      break;
    }
    case UpdateBorderReason::kFocusedTabChanged_NoFocusChange: {
      if (ShouldShowBorderAnimation()) {
        border_view_->ResetAnimationCycle();
      }
      break;
    }
    // This happens when the user has changed the focus from this chrome
    // window to a different chrome window or a different app.
    case UpdateBorderReason::kFocusedTabChanged_GainFocus: {
      border_view_->StopShowing();
      if (ShouldShowBorderAnimation()) {
        border_view_->Show();
      }
      break;
    }
    case UpdateBorderReason::kFocusedTabChanged_LostFocus: {
      if (border_view_->compositor_) {
        border_view_->StartRampingDown();
      }
      break;
    }
  }
}

bool ContextSharingBorderViewController::IsGlicWindowShowing() const {
  return border_view_->GetGlicService()->IsWindowShowing();
}

bool ContextSharingBorderViewController::IsTabInCurrentView(
    const content::WebContents* tab) const {
  return contents_web_view_->web_contents() == tab;
}

bool ContextSharingBorderViewController::ShouldShowBorderAnimation() {
  if (!glic_focused_contents_in_current_view_) {
    return false;
  }

  // Remaining single instance checks.
  if (!context_access_indicator_enabled_) {
    return false;
  }

  // For multi-instance we rely on the sharing manager signal for everything
  // else.
  if (GlicEnabling::IsMultiInstanceEnabledByFlags()) {
    return true;
  }

  return IsGlicWindowShowing();
}

std::string ContextSharingBorderViewController::UpdateReasonToString(
    UpdateBorderReason reason) {
  switch (reason) {
    case UpdateBorderReason::kContextAccessIndicatorOn:
      return "IndicatorOn";
    case UpdateBorderReason::kContextAccessIndicatorOff:
      return "IndicatorOff";
    case UpdateBorderReason::kFocusedTabChanged_NoFocusChange:
      return "TabFocusChange";
    case UpdateBorderReason::kFocusedTabChanged_GainFocus:
      return "WindowGainFocus";
    case UpdateBorderReason::kFocusedTabChanged_LostFocus:
      return "WindowLostFocus";
  }
  NOTREACHED();
}

void ContextSharingBorderViewController::AddReasonForDebugging(
    UpdateBorderReason reason) {
  border_update_reasons_.push_back(UpdateReasonToString(reason));
  if (border_update_reasons_.size() > kNumReasonsToKeep) {
    border_update_reasons_.pop_front();
  }
}

std::string ContextSharingBorderViewController::UpdateReasonsToString() const {
  std::ostringstream oss;
  for (const auto& r : border_update_reasons_) {
    oss << r << ",";
  }
  return oss.str();
}

}  // namespace glic
