// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_UI_ERROR_DIALOG_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_UI_ERROR_DIALOG_VIEW_H_

#include "chrome/browser/ash/arc/nearby_share/ui/base_dialog_delegate_view.h"

#include "base/functional/callback_forward.h"

namespace aura {
class Window;
}

namespace views {
class View;
}  // namespace views

namespace arc {

// The BubbleDialog view for non-actionable ARC++ Nearby Share errors.
class ErrorDialogView : public arc::BaseDialogDelegateView {
 public:
  ErrorDialogView(views::View* anchor_view, base::OnceClosure callback);
  ErrorDialogView(const ErrorDialogView&) = delete;
  ErrorDialogView& operator=(const ErrorDialogView&) = delete;
  ~ErrorDialogView() override;

  static void Show(aura::Window* arc_window, base::OnceClosure callback);
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_UI_ERROR_DIALOG_VIEW_H_
