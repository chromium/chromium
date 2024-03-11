// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_FASTER_SPLIT_VIEW_H_
#define ASH_WM_SPLITVIEW_FASTER_SPLIT_VIEW_H_

#include "ash/style/icon_button.h"
#include "ash/style/system_toast_style.h"
#include "ash/wm/overview/overview_focusable_view.h"
#include "base/functional/callback.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

class FasterSplitViewToast;

// A settings button in faster splitscreen setup.
class FasterSplitViewSettingsButton : public IconButton,
                                      public OverviewFocusableView {
  METADATA_HEADER(FasterSplitViewSettingsButton, IconButton)

 public:
  explicit FasterSplitViewSettingsButton(
      base::RepeatingClosure settings_callback);
  FasterSplitViewSettingsButton(const FasterSplitViewSettingsButton&) = delete;
  FasterSplitViewSettingsButton& operator=(
      const FasterSplitViewSettingsButton&) = delete;
  ~FasterSplitViewSettingsButton() override;

  // OverviewFocusableView:
  views::View* GetView() override;
  void MaybeActivateFocusedView() override;
  void MaybeCloseFocusedView(bool primary_action) override;
  void MaybeSwapFocusedView(bool right) override;
  void OnFocusableViewFocused() override;
  void OnFocusableViewBlurred() override;
};

// A container for the contents view of the faster splitscreen setup widget.
// TODO(b/324347613): Find a better name for this class.
class ASH_EXPORT FasterSplitView : public views::BoxLayoutView {
  METADATA_HEADER(FasterSplitView, views::BoxLayoutView)

 public:
  FasterSplitView(base::RepeatingClosure skip_callback,
                  base::RepeatingClosure settings_callback);
  FasterSplitView(const FasterSplitView&) = delete;
  FasterSplitView& operator=(const FasterSplitView&) = delete;
  ~FasterSplitView() override = default;

  OverviewFocusableView* GetToast();
  views::LabelButton* GetDismissButton();
  FasterSplitViewSettingsButton* settings_button() { return settings_button_; }

 private:
  raw_ptr<FasterSplitViewToast> toast_ = nullptr;
  raw_ptr<FasterSplitViewSettingsButton> settings_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_FASTER_SPLIT_VIEW_H_
