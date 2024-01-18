// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_PANEL_VIEW_H_
#define ASH_SYSTEM_MAHI_MAHI_PANEL_VIEW_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

// The code for Mahi main panel view. This view is placed within
// `MahiPanelWidget`.
class ASH_EXPORT MahiPanelView : public views::BoxLayoutView {
 public:
  METADATA_HEADER(MahiPanelView);
  MahiPanelView();
  MahiPanelView(const MahiPanelView&) = delete;
  MahiPanelView& operator=(const MahiPanelView&) = delete;
  ~MahiPanelView() override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_MAHI_PANEL_VIEW_H_
