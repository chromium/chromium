// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CLIPBOARD_NOTIFICATION_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CLIPBOARD_NOTIFICATION_HELPER_H_

#include "base/strings/string16.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {
class DataTransferEndpoint;
}

namespace policy {

class DlpClipboardNotificationHelper : public views::WidgetObserver {
 public:
  DlpClipboardNotificationHelper() = default;
  ~DlpClipboardNotificationHelper() override = default;

  DlpClipboardNotificationHelper(const DlpClipboardNotificationHelper&) =
      delete;
  void operator=(const DlpClipboardNotificationHelper&) = delete;

  // Shows a bubble that clipboard paste is not allowed. If the type of
  // `data_dst` is kGuestOS or kArc, it will show a toast instead of a
  // notification.
  void NotifyBlockedPaste(const ui::DataTransferEndpoint* const data_src,
                          const ui::DataTransferEndpoint* const data_dst);

 private:
  virtual void ShowClipboardBlockBubble(const base::string16& text);
  virtual void ShowClipboardBlockToast(const std::string& id,
                                       const base::string16& text);

  // views::WidgetObserver
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroyed(views::Widget* widget) override;

  views::UniqueWidgetPtr widget_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CLIPBOARD_NOTIFICATION_HELPER_H_
