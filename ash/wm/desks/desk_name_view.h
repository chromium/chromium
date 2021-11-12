// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_NAME_VIEW_H_
#define ASH_WM_DESKS_DESK_NAME_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/desks/label_textfield.h"
#include "ash/wm/overview/overview_highlightable_view.h"
#include "ash/wm/wm_highlight_item_border.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class DeskMiniView;

// Defines a textfield styled to normally look like a label. Allows modifying
// the name of its corresponding desk. It can be highlighted and activated by
// the OverviewHighlightController. Inherits an API to elide long desk names.
// TODO(richui): In a follow up CL, refactor the renaming logic, and see if
// there are more functions we can extract into `LabelTextfield`.
class ASH_EXPORT DeskNameView : public LabelTextfield,
                                public OverviewHighlightableView {
 public:
  METADATA_HEADER(DeskNameView);

  explicit DeskNameView(DeskMiniView* mini_view);
  DeskNameView(const DeskNameView&) = delete;
  DeskNameView& operator=(const DeskNameView&) = delete;
  ~DeskNameView() override;

  // The max number of characters (UTF-16) allowed for desks' names.
  static constexpr size_t kMaxLength = 300;

  // Commits an on-going desk name change (if any) by bluring the focus away
  // from any view on |widget|, where |widget| should be the desks bar widget.
  static void CommitChanges(views::Widget* widget);

  // LabelTextfield:
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // OverviewHighlightableView:
  views::View* GetView() override;
  void MaybeActivateHighlightedView() override;
  void MaybeCloseHighlightedView() override;
  void MaybeSwapHighlightedView(bool right) override;
  void OnViewHighlighted() override;
  void OnViewUnhighlighted() override;

 private:
  void UpdateBorderState();

  // The mini view that associated with this name view.
  DeskMiniView* const mini_view_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_NAME_VIEW_H_
