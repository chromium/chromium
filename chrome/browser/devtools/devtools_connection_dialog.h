// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_CONNECTION_DIALOG_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_CONNECTION_DIALOG_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace views {
class Widget;
}  // namespace views

namespace ui {
class Event;
}

class Browser;

// A self-destructing dialog to confirm the debugging connection.
class DevToolsConnectionDialog {
 public:
  using AcceptCallback = content::DevToolsManagerDelegate::AcceptCallback;

  static DevToolsConnectionDialog* Show(Browser* browser,
                                        AcceptCallback callback);

  base::WeakPtr<views::Widget> GetDialogWidgetForTesting() {
    return dialog_widget_;
  }

 private:
  explicit DevToolsConnectionDialog(Browser* browser, AcceptCallback callback);
  ~DevToolsConnectionDialog();

  void OnAccept();
  void OnCancel();
  void OnDisable(const ui::Event& event);
  void OnClose();
  void RunCallbackAndDie(
      content::DevToolsManagerDelegate::AcceptConnectionResult result);

  base::WeakPtr<views::Widget> dialog_widget_;

  raw_ptr<Browser> browser_;
  AcceptCallback callback_;
  bool handled_ = false;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_CONNECTION_DIALOG_H_
