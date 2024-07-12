// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_SETUP_VIEW_OLD_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_SETUP_VIEW_OLD_H_

#include "ash/style/icon_button.h"
#include "ash/style/system_toast_style.h"
#include "ash/wm/overview/overview_focusable_view.h"
#include "base/functional/callback.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

class SplitViewSetupViewOldToast;

// A settings button in split view overview setup.
class SplitViewSetupViewOldSettingsButton : public IconButton,
                                            public OverviewFocusableView {
  METADATA_HEADER(SplitViewSetupViewOldSettingsButton, IconButton)

 public:
  explicit SplitViewSetupViewOldSettingsButton(
      base::RepeatingClosure settings_callback);
  SplitViewSetupViewOldSettingsButton(
      const SplitViewSetupViewOldSettingsButton&) = delete;
  SplitViewSetupViewOldSettingsButton& operator=(
      const SplitViewSetupViewOldSettingsButton&) = delete;
  ~SplitViewSetupViewOldSettingsButton() override;

  // OverviewFocusableView:
  views::View* GetView() override;
  void MaybeActivateFocusedView() override;
  void MaybeCloseFocusedView(bool primary_action) override;
  void MaybeSwapFocusedView(bool right) override;
  void OnFocusableViewFocused() override;
  void OnFocusableViewBlurred() override;
};

// A container for the split view toast and settings button.
class ASH_EXPORT SplitViewSetupViewOld : public views::BoxLayoutView {
  METADATA_HEADER(SplitViewSetupViewOld, views::BoxLayoutView)

 public:
  SplitViewSetupViewOld(base::RepeatingClosure skip_callback,
                        base::RepeatingClosure settings_callback);
  SplitViewSetupViewOld(const SplitViewSetupViewOld&) = delete;
  SplitViewSetupViewOld& operator=(const SplitViewSetupViewOld&) = delete;
  ~SplitViewSetupViewOld() override = default;

  OverviewFocusableView* GetToast();
  views::LabelButton* GetDismissButton();
  SplitViewSetupViewOldSettingsButton* settings_button() {
    return settings_button_;
  }

 private:
  raw_ptr<SplitViewSetupViewOldToast> toast_ = nullptr;
  raw_ptr<SplitViewSetupViewOldSettingsButton> settings_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_SETUP_VIEW_OLD_H_
