// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_DATA_TRANSFER_NOTIFIER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_DATA_TRANSFER_NOTIFIER_H_

#include "base/functional/callback_forward.h"
#include "base/timer/timer.h"
#include "base/types/optional_ref.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
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
      base::optional_ref<const ui::DataTransferEndpoint> data_src,
      base::optional_ref<const ui::DataTransferEndpoint> data_dst) = 0;

  void DeleteWidget(views::Widget* widget);

 protected:
  // Virtual for tests to override.
  virtual void ShowBlockBubble(const std::u16string& text);
  virtual void ShowWarningBubble(
      const std::u16string& text,
      base::OnceCallback<void(views::Widget*)> proceed_cb,
      base::OnceCallback<void(views::Widget*)> cancel_cb);
  virtual void CloseWidget(MayBeDangling<views::Widget> widget,
                           views::Widget::ClosedReason reason);
  virtual void SetPasteCallback(base::OnceCallback<void(bool)> paste_cb);
  virtual void RunPasteCallback();

  // views::WidgetObserver
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  std::unique_ptr<views::Widget> widget_;

 private:
  void InitWidget();

  // TODO(ayaelattar): Change `timeout_duration_ms` to TimeDelta.
  void ResizeAndShowWidget(const gfx::Size& bubble_size,
                           int timeout_duration_ms);

  base::OneShotTimer widget_closing_timer_;

  std::unique_ptr<views::WidgetDelegate> widget_delegate_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_DATA_TRANSFER_NOTIFIER_H_
