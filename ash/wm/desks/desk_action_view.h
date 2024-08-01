// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_ACTION_VIEW_H_
#define ASH_WM_DESKS_DESK_ACTION_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/style/blurred_background_shield.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_observer.h"

namespace ash {

class DeskActionButton;
class DeskMiniView;

// A view that holds buttons that act on a single DeskMiniView instance, such as
// combining two desks or closing a desk and all of its windows.
class ASH_EXPORT DeskActionView : public views::BoxLayoutView,
                                  public views::ViewObserver {
  METADATA_HEADER(DeskActionView, views::BoxLayoutView)

 public:
  DeskActionView(const std::u16string& combine_desks_target_name,
                 const std::u16string& close_all_target_name,
                 base::RepeatingClosure context_menu_callback,
                 base::RepeatingClosure combine_desks_callback,
                 base::RepeatingClosure close_all_callback,
                 base::RepeatingClosure focus_change_callback,
                 DeskMiniView* mini_view);
  DeskActionView(const DeskActionView&) = delete;
  DeskActionView& operator=(const DeskActionView&) = delete;
  ~DeskActionView() override;

  const DeskActionButton* context_menu_button() const {
    return context_menu_button_;
  }
  DeskActionButton* context_menu_button() { return context_menu_button_; }

  const DeskActionButton* close_all_button() const { return close_all_button_; }
  DeskActionButton* close_all_button() { return close_all_button_; }

  const DeskActionButton* combine_desks_button() const {
    return combine_desks_button_;
  }
  DeskActionButton* combine_desks_button() { return combine_desks_button_; }

  DeskMiniView* mini_view() { return mini_view_; }

  // Indicates if any child has focus. If in overview, it means
  // `OverviewFocusableView` is focused; otherwise it means `ui::View` has
  // focus.
  bool ChildHasFocus() const;

 private:
  // views::ViewObserver:
  void OnViewFocused(views::View* observed) override;
  void OnViewBlurred(views::View* observed) override;

  void OnFocusChange();

  // Only one of the following two buttons will be shown, based on if the Forest
  // feature is enabled.
  raw_ptr<DeskActionButton> context_menu_button_;
  raw_ptr<DeskActionButton> combine_desks_button_;

  raw_ptr<DeskActionButton> close_all_button_;

  // Maintains blurred rounded rect background without clipping. Useful when
  // generating a focus ring that will partially extend outside of the view.
  std::unique_ptr<BlurredBackgroundShield> blurred_background_;

  // This is set on creation and notifies the mini view when a child view is
  // focused or blurred.
  base::RepeatingClosure focus_change_callback_;

  // Desk mini view that owns this desk action view.
  raw_ptr<DeskMiniView> mini_view_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_ACTION_VIEW_H_
