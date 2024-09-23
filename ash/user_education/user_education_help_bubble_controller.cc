// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_help_bubble_controller.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/user_education/user_education_delegate.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/user_education_util.h"
#include "ash/user_education/views/help_bubble_factory_views_ash.h"
#include "ash/user_education/views/help_bubble_view_ash.h"
#include "base/cancelable_callback.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "components/account_id/account_id.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_params.h"
#include "ui/aura/window.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// The singleton instance owned by the `UserEducationController`.
UserEducationHelpBubbleController* g_instance = nullptr;

// Helpers ---------------------------------------------------------------------

gfx::Rect GetAnchorBoundsInScreen(const HelpBubbleViewAsh* help_bubble_view) {
  return help_bubble_view->GetAnchorView()->GetAnchorBoundsInScreen();
}

aura::Window* GetAnchorRootWindow(const HelpBubbleViewAsh* help_bubble_view) {
  return help_bubble_view->GetAnchorView()
      ->GetWidget()
      ->GetNativeWindow()
      ->GetRootWindow();
}

}  // namespace

// UserEducationHelpBubbleController -------------------------------------------

UserEducationHelpBubbleController::UserEducationHelpBubbleController(
    UserEducationDelegate* delegate)
    : delegate_(delegate) {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

UserEducationHelpBubbleController::~UserEducationHelpBubbleController() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
UserEducationHelpBubbleController* UserEducationHelpBubbleController::Get() {
  // Should only be `nullptr` in testing.
  if (!g_instance) {
    CHECK_IS_TEST();
  }
  return g_instance;
}

std::optional<HelpBubbleId> UserEducationHelpBubbleController::GetHelpBubbleId(
    ui::ElementIdentifier element_id,
    ui::ElementContext element_context) const {
  if (help_bubble_ && help_bubble_->IsA<HelpBubbleViewsAsh>()) {
    // Cache the `bubble_view` with its associated anchor.
    auto* bubble_view = help_bubble_->AsA<HelpBubbleViewsAsh>()->bubble_view();
    auto* anchor_view = bubble_view->GetAnchorView();

    // Find all `tracked_views` matching `element_id` and `element_context`.
    const views::ElementTrackerViews::ViewList tracked_views =
        views::ElementTrackerViews::GetInstance()->GetAllMatchingViews(
            element_id, element_context);

    // A help bubble exists for a `tracked_view` if the `tracked_view` is the
    // `anchor_view` for the help bubble.
    for (const auto* tracked_view : tracked_views) {
      if (tracked_view == anchor_view) {
        return bubble_view->id();
      }
    }
  }
  return std::nullopt;
}

base::CallbackListSubscription
UserEducationHelpBubbleController::AddHelpBubbleAnchorBoundsChangedCallback(
    base::RepeatingClosure callback) {
  return help_bubble_anchor_bounds_changed_subscribers_.Add(
      std::move(callback));
}

base::CallbackListSubscription
UserEducationHelpBubbleController::AddHelpBubbleClosedCallback(
    base::RepeatingClosure callback) {
  return help_bubble_closed_subscribers_.Add(std::move(callback));
}

base::CallbackListSubscription
UserEducationHelpBubbleController::AddHelpBubbleShownCallback(
    base::RepeatingClosure callback) {
  return help_bubble_shown_subscribers_.Add(std::move(callback));
}

void UserEducationHelpBubbleController::NotifyHelpBubbleAnchorBoundsChanged(
    base::PassKey<HelpBubbleViewAsh>,
    const HelpBubbleViewAsh* help_bubble_view) {
  // Ignore event if the associated help bubble has not yet been shown.
  if (auto it = help_bubble_metadata_by_key_.find(help_bubble_view);
      it != help_bubble_metadata_by_key_.end()) {
    it->second.anchor_bounds_in_screen =
        GetAnchorBoundsInScreen(help_bubble_view);
    help_bubble_anchor_bounds_changed_subscribers_.Notify();
  }
}

void UserEducationHelpBubbleController::NotifyHelpBubbleClosed(
    base::PassKey<HelpBubbleViewAsh>,
    const HelpBubbleViewAsh* help_bubble_view) {
  // Ignore event if the associated help bubble has not yet been shown.
  if (auto it = help_bubble_metadata_by_key_.find(help_bubble_view);
      it != help_bubble_metadata_by_key_.end()) {
    help_bubble_metadata_by_key_.erase(it);
    help_bubble_closed_subscribers_.Notify();
  }
}

void UserEducationHelpBubbleController::NotifyHelpBubbleShown(
    base::PassKey<HelpBubbleViewAsh>,
    const HelpBubbleViewAsh* help_bubble_view) {
  // Ignore event if the associated help bubble has already been shown.
  if (!base::Contains(help_bubble_metadata_by_key_, help_bubble_view)) {
    help_bubble_metadata_by_key_.emplace(
        std::piecewise_construct, std::forward_as_tuple(help_bubble_view),
        std::forward_as_tuple(help_bubble_view,
                              GetAnchorRootWindow(help_bubble_view),
                              GetAnchorBoundsInScreen(help_bubble_view)));
    help_bubble_shown_subscribers_.Notify();
  }
}

}  // namespace ash
