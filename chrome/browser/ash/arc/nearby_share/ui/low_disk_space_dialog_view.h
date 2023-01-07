// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_UI_LOW_DISK_SPACE_DIALOG_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_UI_LOW_DISK_SPACE_DIALOG_VIEW_H_

#include "chrome/browser/ash/arc/nearby_share/ui/base_dialog_delegate_view.h"

#include "base/functional/callback_forward.h"

namespace views {
class View;
}  // namespace views

namespace arc {

// The BubbleDialog view when there's not enough disk space to stream files from
// the Android to the Chrome OS file system.
class LowDiskSpaceDialogView : public arc::BaseDialogDelegateView {
 public:
  using OnCloseCallback =
      base::OnceCallback<void(bool should_open_storage_settings)>;

  LowDiskSpaceDialogView(views::View* anchor_view,
                         int file_count,
                         int64_t required_disk_space,
                         OnCloseCallback callback);
  LowDiskSpaceDialogView(const LowDiskSpaceDialogView&) = delete;
  LowDiskSpaceDialogView& operator=(const LowDiskSpaceDialogView&) = delete;
  ~LowDiskSpaceDialogView() override;

  // Show the dialog
  static void Show(aura::Window* arc_window,
                   int file_count,
                   int64_t required_disk_space,
                   OnCloseCallback callback);

 private:
  OnCloseCallback close_callback_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_UI_LOW_DISK_SPACE_DIALOG_VIEW_H_
