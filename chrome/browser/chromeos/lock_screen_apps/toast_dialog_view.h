// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_TOAST_DIALOG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_TOAST_DIALOG_VIEW_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace lock_screen_apps {

// The system modal bubble dialog shown to the user when a lock screen app is
// first launched from the lock screen. The dialog will block the app UI until
// the user closes it.
class ToastDialogView : public views::BubbleDialogDelegateView {
 public:
  ToastDialogView(const base::string16& app_name,
                  base::OnceClosure dismissed_callback);
  ~ToastDialogView() override;

  // views::BubbleDialogDelegateView:
  void AddedToWidget() override;
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;

 private:
  // Callback to be called when the user closes the dialog.
  base::OnceClosure dismissed_callback_;

  DISALLOW_COPY_AND_ASSIGN(ToastDialogView);
};

}  // namespace lock_screen_apps

#endif  // CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_TOAST_DIALOG_VIEW_H_
