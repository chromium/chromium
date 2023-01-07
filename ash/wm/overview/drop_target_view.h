// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_DROP_TARGET_VIEW_H_
#define ASH_WM_OVERVIEW_DROP_TARGET_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
}

namespace ash {

// DropTargetView represents a transparent view with border in overview. It
// includes a background view and plus icon. Dragged window in tablet mode can
// be dragged into it and then dropped into overview.
class DropTargetView : public views::View {
 public:
  METADATA_HEADER(DropTargetView);

  explicit DropTargetView(bool has_plus_icon);
  DropTargetView(const DropTargetView&) = delete;
  DropTargetView& operator=(const DropTargetView&) = delete;
  ~DropTargetView() override = default;

  // Updates the visibility of |background_view_| since it is only shown when
  // drop target is selected in overview.
  void UpdateBackgroundVisibility(bool visible);

  // views::View:
  void Layout() override;

 private:
  views::View* background_view_ = nullptr;
  views::ImageView* plus_icon_ = nullptr;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_DROP_TARGET_VIEW_H_
