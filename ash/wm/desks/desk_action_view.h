// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_ACTION_VIEW_H_
#define ASH_WM_DESKS_DESK_ACTION_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

class CloseButton;

// A view that holds buttons that act on a single DeskMiniView instance, such as
// combining two desks or closing a desk and all of its windows.
class DeskActionView : public views::BoxLayoutView {
 public:
  METADATA_HEADER(DeskActionView);

  DeskActionView(base::RepeatingClosure combine_desks_callback,
                 base::RepeatingClosure close_all_callback);

  CloseButton* close_all_button() { return close_all_button_; }

  CloseButton* combine_desks_button() { return combine_desks_button_; }

 private:
  CloseButton* close_all_button_;
  CloseButton* combine_desks_button_;
};

}  // namespace ash

#endif