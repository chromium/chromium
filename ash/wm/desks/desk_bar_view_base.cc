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

bool DeskBarViewBase::IsZeroState() const {
  return state_ == DeskBarViewBase::State::kZero;
}

}  // namespace ash
