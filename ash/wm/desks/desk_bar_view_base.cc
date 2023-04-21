// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_bar_view_base.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "base/check.h"
#include "base/notreached.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

DeskBarViewBase::DeskBarViewBase(aura::Window* root, Type type)
    : type_(type), state_(GetPerferredState(type)), root_(root) {
  CHECK(root && root->IsRootWindow());
}

DeskBarViewBase::~DeskBarViewBase() = default;

// static
int DeskBarViewBase::GetPreferredBarHeight(aura::Window* root,
                                           Type type,
                                           State state) {
  int height = 0;
  switch (type) {
    case Type::kDeskButton:
      CHECK_EQ(State::kExpanded, state);
      height =
          DeskPreviewView::GetHeight(root) + kDeskBarNonPreviewAllocatedHeight;
      break;
    case Type::kOverview:
      if (state == State::kZero) {
        height = kDeskBarZeroStateHeight;
      } else {
        height = DeskPreviewView::GetHeight(root) +
                 kDeskBarNonPreviewAllocatedHeight;
      }
      break;
  }

  return height;
}

// static
DeskBarViewBase::State DeskBarViewBase::GetPerferredState(Type type) {
  State state = State::kZero;
  switch (type) {
    case Type::kDeskButton:
      // Desk button desk bar is always expaneded.
      state = State::kExpanded;
      break;
    case Type::kOverview: {
      // Overview desk bar can be zero state if both conditions below are true.
      //   - there is only one desk;
      //   - not currently showing saved desk library;
      OverviewController* overview_controller =
          Shell::Get()->overview_controller();
      DesksController* desk_controller = DesksController::Get();
      if (desk_controller->GetNumberOfDesks() == 1 &&
          overview_controller->InOverviewSession() &&
          !overview_controller->overview_session()
               ->IsShowingSavedDeskLibrary()) {
        state = State::kZero;
      } else {
        state = State::kExpanded;
      }
      break;
    }
  }

  return state;
}

// static
std::unique_ptr<views::Widget> DeskBarViewBase::CreateDeskWidget(
    aura::Window* root,
    const gfx::Rect& bounds,
    Type type) {
  CHECK(root && root->IsRootWindow());

  std::unique_ptr<views::Widget> widget;
  switch (type) {
    case Type::kOverview: {
      widget = std::make_unique<views::Widget>();
      views::Widget::InitParams params(
          views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
      params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
      params.activatable = views::Widget::InitParams::Activatable::kYes;
      params.accept_events = true;
      params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
      // This widget will be parented to the currently-active desk container on
      // `root`.
      params.context = root;
      params.bounds = bounds;
      params.name = "OverviewDeskBarWidget";

      // Even though this widget exists on the active desk container, it should
      // not show up in the MRU list, and it should not be mirrored in the desks
      // mini_views.
      params.init_properties_container.SetProperty(kExcludeInMruKey, true);
      params.init_properties_container.SetProperty(kHideInDeskMiniViewKey,
                                                   true);
      widget->Init(std::move(params));

      auto* window = widget->GetNativeWindow();
      window->SetId(kShellWindowId_DesksBarWindow);
      ::wm::SetWindowVisibilityAnimationTransition(window, ::wm::ANIMATE_NONE);

      break;
    }
    case Type::kDeskButton:
      // TODO(b/274840033): Create desk bar widget for desk button.
      NOTREACHED();

      break;
  }

  return widget;
}

void DeskBarViewBase::UpdateNewMiniViews(bool initializing_bar_view,
                                         bool expanding_bar_view) {
  NOTREACHED();
}
void DeskBarViewBase::UpdateDeskButtonsVisibility() {
  NOTREACHED();
}
void DeskBarViewBase::SwitchToExpandedState() {
  NOTREACHED();
}
void DeskBarViewBase::NudgeDeskName(int desk_index) {
  NOTREACHED();
}
void DeskBarViewBase::HandlePressEvent(DeskMiniView* mini_view,
                                       const ui::LocatedEvent& event) {
  NOTREACHED();
}
void DeskBarViewBase::HandleLongPressEvent(DeskMiniView* mini_view,
                                           const ui::LocatedEvent& event) {
  NOTREACHED();
}
void DeskBarViewBase::HandleDragEvent(DeskMiniView* mini_view,
                                      const ui::LocatedEvent& event) {
  NOTREACHED();
}
bool DeskBarViewBase::HandleReleaseEvent(DeskMiniView* mini_view,
                                         const ui::LocatedEvent& event) {
  NOTREACHED_NORETURN();
}

bool DeskBarViewBase::IsZeroState() const {
  return state_ == DeskBarViewBase::State::kZero;
}

bool DeskBarViewBase::IsDraggingDesk() const {
  return drag_view_ != nullptr;
}

void DeskBarViewBase::ScrollToShowViewIfNecessary(const views::View* view) {
  CHECK(base::Contains(scroll_view_contents_->children(), view));
  const gfx::Rect visible_bounds = scroll_view_->GetVisibleRect();
  const gfx::Rect view_bounds = view->bounds();
  const bool beyond_left = view_bounds.x() < visible_bounds.x();
  const bool beyond_right = view_bounds.right() > visible_bounds.right();
  auto* scroll_bar = scroll_view_->horizontal_scroll_bar();
  if (beyond_left) {
    scroll_view_->ScrollToPosition(
        scroll_bar, view_bounds.right() - scroll_view_->bounds().width());
  } else if (beyond_right) {
    scroll_view_->ScrollToPosition(scroll_bar, view_bounds.x());
  }
}

DeskMiniView* DeskBarViewBase::FindMiniViewForDesk(const Desk* desk) const {
  for (auto* mini_view : mini_views_) {
    if (mini_view->desk() == desk) {
      return mini_view;
    }
  }

  return nullptr;
}

int DeskBarViewBase::GetMiniViewIndex(const DeskMiniView* mini_view) const {
  auto iter = base::ranges::find(mini_views_, mini_view);
  return (iter == mini_views_.cend())
             ? -1
             : std::distance(mini_views_.cbegin(), iter);
}

void DeskBarViewBase::UpdateScrollButtonsVisibility() {
  NOTREACHED();
}
void DeskBarViewBase::UpdateGradientMask() {
  NOTREACHED();
}

int DeskBarViewBase::GetFirstMiniViewXOffset() const {
  // `GetMirroredX` is used here to make sure the removing and adding a desk
  // transform is correct while in RTL layout.
  return mini_views_.empty() ? bounds().CenterPoint().x()
                             : mini_views_[0]->GetMirroredX();
}

void DeskBarViewBase::UpdateDeskButtonsVisibilityCrOSNext() {
  NOTREACHED();
}
void DeskBarViewBase::UpdateLibraryButtonVisibilityCrOSNext() {
  NOTREACHED();
}

}  // namespace ash
