// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_ACTION_VIEW_H_
#define ASH_WM_DESKS_DESK_ACTION_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

class CloseButton;

// A view that holds buttons that act on a single DeskMiniView instance, such as
// combining two desks or closing a desk and all of its windows.
class DeskActionView : public views::BoxLayoutView {
 public:
  METADATA_HEADER(DeskActionView);

  DeskActionView(const std::u16string& initial_combine_desks_target_name,
                 base::RepeatingClosure combine_desks_callback,
                 base::RepeatingClosure close_all_callback);

  const CloseButton* close_all_button() const { return close_all_button_; }

  const CloseButton* combine_desks_button() const {
    return combine_desks_button_;
  }

  // Changes the tooltip assigned to `combine_desks_button_` to use
  // `new_combine_desks_target_name` as the name of the target desk where
  // windows will be moved if `combine_desks_button_` is pressed.
  void UpdateCombineDesksTooltip(
      const std::u16string& new_combine_desks_target_name);

  // Changes the visibility of the combine desks button so that it can reflect
  // whether there are windows on the desk.
  void SetCombineDesksButtonVisibility(bool visible);

 private:
  raw_ptr<CloseButton, ExperimentalAsh> combine_desks_button_;
  raw_ptr<CloseButton, ExperimentalAsh> close_all_button_;
};

}  // namespace ash

#endif