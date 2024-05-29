// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_FASTER_SPLIT_VIEW_H_
#define ASH_WM_SPLITVIEW_FASTER_SPLIT_VIEW_H_

#include "base/functional/callback.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

// A container for the contents view of the faster splitscreen setup widget.
// TODO(http://b/324347613): Find a better name for this class.
class FasterSplitView : public views::BoxLayoutView {
  METADATA_HEADER(FasterSplitView, views::BoxLayoutView)

 public:
  FasterSplitView(base::RepeatingClosure skip_callback,
                  base::RepeatingClosure settings_callback);
  FasterSplitView(const FasterSplitView&) = delete;
  FasterSplitView& operator=(const FasterSplitView&) = delete;
  ~FasterSplitView() override;
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_FASTER_SPLIT_VIEW_H_
