// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_COMMON_GLANCEABLES_PROGRESS_BAR_VIEW_H_
#define ASH_GLANCEABLES_COMMON_GLANCEABLES_PROGRESS_BAR_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

namespace views {
class ProgressBar;
}  // namespace views

namespace ash {

// Container for infinite `views::ProgressBar` used in glanceables bubbles.
class ASH_EXPORT GlanceablesProgressBarView : public views::FlexLayoutView {
  METADATA_HEADER(GlanceablesProgressBarView, views::FlexLayoutView)

 public:
  GlanceablesProgressBarView();
  GlanceablesProgressBarView(const GlanceablesProgressBarView&) = delete;
  GlanceablesProgressBarView& operator=(const GlanceablesProgressBarView&) =
      delete;
  ~GlanceablesProgressBarView() override = default;

  // Updates `progress_bar_` visibility keeping constant height of `this`.
  void UpdateProgressBarVisibility(bool visible);

 private:
  raw_ptr<views::ProgressBar> progress_bar_ = nullptr;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_COMMON_GLANCEABLES_PROGRESS_BAR_VIEW_H_
