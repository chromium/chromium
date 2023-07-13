// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_ACTION_VIEW_H_
#define ASH_WM_DESKS_DESK_ACTION_VIEW_H_

#include <memory>

#include "ash/style/blurred_background_shield.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_observer.h"

namespace ash {

class CloseButton;

// A view that holds buttons that act on a single DeskMiniView instance, such as
// combining two desks or closing a desk and all of its windows.
class DeskActionView : public views::BoxLayoutView, views::ViewObserver {
 public:
  METADATA_HEADER(DeskActionView);

  DeskActionView(const std::u16string& initial_combine_desks_target_name,
                 base::RepeatingClosure combine_desks_callback,
                 base::RepeatingClosure close_all_callback,
                 base::RepeatingClosure focus_change_callback);
  DeskActionView(const DeskActionView&) = delete;
  DeskActionView& operator=(const DeskActionView&) = delete;
  ~DeskActionView() override;

  const CloseButton* close_all_button() const { return close_all_button_; }
  CloseButton* close_all_button() { return close_all_button_; }

  const CloseButton* combine_desks_button() const {
    return combine_desks_button_;
  }

  // Returns true if either `combine_desks_button_` or `close_all_button_` has
  // focus.
  bool ChildHasFocus() const;

  // Changes the tooltip assigned to `combine_desks_button_` to use
  // `new_combine_desks_target_name` as the name of the target desk where
  // windows will be moved if `combine_desks_button_` is pressed.
  void UpdateCombineDesksTooltip(
      const std::u16string& new_combine_desks_target_name);

  // Changes the visibility of the combine desks button so that it can reflect
  // whether there are windows on the desk.
  void SetCombineDesksButtonVisibility(bool visible);

 private:
  // views::ViewObserver:
  void OnViewFocused(views::View* observed) override;
  void OnViewBlurred(views::View* observed) override;

  raw_ptr<CloseButton, ExperimentalAsh> combine_desks_button_;
  raw_ptr<CloseButton, ExperimentalAsh> close_all_button_;

  // Maintains blurred rounded rect background without clipping. Useful when
  // generating a focus ring that will partially extend outside of the view.
  std::unique_ptr<BlurredBackgroundShield> blurred_background_;

  // This is set on creation and notifies the mini view when a child view is
  // focused or blurred.
  base::RepeatingClosure focus_change_callback_;
};

}  // namespace ash

#endif