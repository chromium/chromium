// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CLIPBOARD_NOTIFICATION_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CLIPBOARD_NOTIFICATION_HELPER_H_

#include "base/strings/string16.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {
class DataTransferEndpoint;
}

namespace policy {

class DlpClipboardNotificationHelper : public views::WidgetObserver,
                                       public ui::ClipboardObserver {
 public:
  DlpClipboardNotificationHelper();
  ~DlpClipboardNotificationHelper() override;

  DlpClipboardNotificationHelper(const DlpClipboardNotificationHelper&) =
      delete;
  void operator=(const DlpClipboardNotificationHelper&) = delete;

  // Shows a bubble that clipboard paste is not allowed. If the type of
  // `data_dst` is kCrostini, kPluginVm or kArc, it will show a toast instead of
  // a notification.
  void NotifyBlockedPaste(const ui::DataTransferEndpoint* const data_src,
                          const ui::DataTransferEndpoint* const data_dst);
  // Shows a bubble that warns the user that clipboard paste is not recommended.
  // If the type of `data_dst` is kCrostini, kPluginVm or kArc, it will show a
  // toast instead of a notification.
  void WarnOnPaste(const ui::DataTransferEndpoint* const data_src,
                   const ui::DataTransferEndpoint* const data_dst);

 private:
  virtual void ShowClipboardBlockBubble(const base::string16& text);
  virtual void ShowClipboardBlockToast(const std::string& id,
                                       const base::string16& text);
  virtual void ShowClipboardWarnBubble(const base::string16& text);

  // views::WidgetObserver
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroyed(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // ui::ClipboardObserver
  void OnClipboardDataChanged() override;

  void InitWidget();

  void ResizeAndShowWidget(const gfx::Size& bubble_size,
                           int timeout_duration_ms);

  void CloseWidget(views::Widget* widget, views::Widget::ClosedReason reason);

  views::UniqueWidgetPtr widget_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CLIPBOARD_NOTIFICATION_HELPER_H_
