// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_TOAST_DIALOG_VIEW_H_
#define CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_TOAST_DIALOG_VIEW_H_

#include <string>

#include "base/functional/callback.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace lock_screen_apps {

// The system modal bubble dialog shown to the user when a lock screen app is
// first launched from the lock screen. The dialog will block the app UI until
// the user closes it.
class ToastDialogView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(ToastDialogView, views::BubbleDialogDelegateView)

 public:
  ToastDialogView(const std::u16string& app_name,
                  base::OnceClosure dismissed_callback);
  ToastDialogView(const ToastDialogView&) = delete;
  ToastDialogView& operator=(const ToastDialogView&) = delete;
  ~ToastDialogView() override;

  // views::BubbleDialogDelegateView:
  void AddedToWidget() override;
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;

 private:
  // Callback to be called when the user closes the dialog.
  base::OnceClosure dismissed_callback_;
};

}  // namespace lock_screen_apps

#endif  // CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_TOAST_DIALOG_VIEW_H_
