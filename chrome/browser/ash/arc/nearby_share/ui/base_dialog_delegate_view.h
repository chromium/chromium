// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_UI_BASE_DIALOG_DELEGATE_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_UI_BASE_DIALOG_DELEGATE_VIEW_H_

#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class View;
}  // namespace views

namespace arc {

// This is a base class for showing ARC++ Nearby Share error dialogs:
// |LowDiskSpaceDialogView| and |ErrorDialogView|.
class BaseDialogDelegateView : public views::BubbleDialogDelegateView {
 public:
  explicit BaseDialogDelegateView(views::View* anchor_view);
  BaseDialogDelegateView(const BaseDialogDelegateView&) = delete;
  BaseDialogDelegateView& operator=(const BaseDialogDelegateView&) = delete;
  ~BaseDialogDelegateView() override;

  // Adds a dialog message with the given |text|.
  void AddDialogMessage(std::u16string text);
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_UI_BASE_DIALOG_DELEGATE_VIEW_H_
