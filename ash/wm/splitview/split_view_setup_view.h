// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_SETUP_VIEW_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_SETUP_VIEW_H_

#include "base/functional/callback.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

// A container for the split view toast and settings button.
class SplitViewSetupView : public views::BoxLayoutView {
  METADATA_HEADER(SplitViewSetupView, views::BoxLayoutView)

 public:
  static constexpr int kDismissButtonIDForTest = 3000;
  static constexpr int kSettingsButtonIDForTest = 3001;

  SplitViewSetupView(base::RepeatingClosure skip_callback,
                     base::RepeatingClosure settings_callback);
  SplitViewSetupView(const SplitViewSetupView&) = delete;
  SplitViewSetupView& operator=(const SplitViewSetupView&) = delete;
  ~SplitViewSetupView() override;
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_SETUP_VIEW_H_
