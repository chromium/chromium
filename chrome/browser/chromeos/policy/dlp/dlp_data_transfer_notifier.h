// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_DATA_TRANSFER_NOTIFIER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_DATA_TRANSFER_NOTIFIER_H_

#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {
class DataTransferEndpoint;
}

namespace policy {

class DlpDataTransferNotifier : public views::WidgetObserver {
 public:
  DlpDataTransferNotifier();
  ~DlpDataTransferNotifier() override;

  DlpDataTransferNotifier(const DlpDataTransferNotifier&) = delete;
  void operator=(const DlpDataTransferNotifier&) = delete;

  // Notifies the user that the data transfer action is not allowed.
  virtual void NotifyBlockedAction(
      const ui::DataTransferEndpoint* const data_src,
      const ui::DataTransferEndpoint* const data_dst) = 0;

  // Warns the user that the data transfer action is not recommended.
  virtual void WarnOnAction(const ui::DataTransferEndpoint* const data_src,
                            const ui::DataTransferEndpoint* const data_dst) = 0;

 protected:
  virtual void ShowBlockBubble(const base::string16& text);

  virtual void ShowWarningBubble(
      const base::string16& text,
      base::RepeatingCallback<void(views::Widget*)> proceed_cb);

  void CloseWidget(views::Widget* widget, views::Widget::ClosedReason reason);

  views::Widget* GetWidgetForTesting() { return widget_.get(); }

  views::UniqueWidgetPtr widget_;

 private:
  // views::WidgetObserver
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroyed(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  void InitWidget();

  void ResizeAndShowWidget(const gfx::Size& bubble_size,
                           int timeout_duration_ms);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_DATA_TRANSFER_NOTIFIER_H_
