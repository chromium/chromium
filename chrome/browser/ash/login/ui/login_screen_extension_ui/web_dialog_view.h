// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_SCREEN_EXTENSION_UI_WEB_DIALOG_VIEW_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_SCREEN_EXTENSION_UI_WEB_DIALOG_VIEW_H_

#include <memory>
#include <string>

#include "ash/public/cpp/system_tray_observer.h"
#include "base/macros.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace chromeos {

namespace login_screen_extension_ui {

class DialogDelegate;

// A WebDialogView used by chrome.loginScreenUi API calls. It hides the close
// button if `DialogDelegate::CanCloseDialog()` is false.
class WebDialogView : public views::WebDialogView,
                      public ash::SystemTrayObserver {
 public:
  METADATA_HEADER(WebDialogView);
  explicit WebDialogView(
      content::BrowserContext* context,
      DialogDelegate* delegate,
      std::unique_ptr<ui::WebDialogWebContentsDelegate::WebContentsHandler>
          handler);
  WebDialogView(const WebDialogView&) = delete;
  WebDialogView& operator=(const WebDialogView&) = delete;
  ~WebDialogView() override;

  // views::WebDialogView
  bool TakeFocus(content::WebContents* source, bool reverse) override;

  // ash::SystemTrayObserver
  void OnFocusLeavingSystemTray(bool reverse) override;

 private:
  DialogDelegate* delegate_ = nullptr;
};

}  // namespace login_screen_extension_ui

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_SCREEN_EXTENSION_UI_WEB_DIALOG_VIEW_H_
