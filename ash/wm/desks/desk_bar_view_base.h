// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_BAR_VIEW_BASE_H_
#define ASH_WM_DESKS_DESK_BAR_VIEW_BASE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Base class for desk bar views, including desk bar view within overview and
// desk bar view for the desk button.
class ASH_EXPORT DeskBarViewBase : public views::View,
                                   public DesksController::Observer {
 public:
  enum class Type {
    kOverview,
    kDeskButton,
  };

  enum class State {
    kZero,
    kExpanded,
  };

  DeskBarViewBase(aura::Window* root, Type type);
  DeskBarViewBase(const DeskBarViewBase&) = delete;
  DeskBarViewBase& operator=(const DeskBarViewBase&) = delete;
  ~DeskBarViewBase() override;

  // Returns the preferred height of the desk bar that exists on `root` with
  // `state`.
  static int GetPreferredBarHeight(aura::Window* root, Type type, State state);

  // Returns the preferred state for the desk bar given `type`.
  static State GetPerferredState(Type type);

  // Creates and returns the widget that contains the desk bar view of `type`.
  // The returned widget has no contents view yet, and hasn't been shown yet.
  static std::unique_ptr<views::Widget>
  CreateDeskWidget(aura::Window* root, const gfx::Rect& bounds, Type type);

  Type type() const { return type_; }
  State state() const { return state_; }

  // Returns true if it is currently in zero state.
  bool IsZeroState() const;

 protected:
  const Type type_ = Type::kOverview;
  State state_ = State::kZero;

 private:
  raw_ptr<aura::Window> root_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_BAR_VIEW_BASE_H_
