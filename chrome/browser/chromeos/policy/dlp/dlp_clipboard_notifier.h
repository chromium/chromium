// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CLIPBOARD_NOTIFIER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CLIPBOARD_NOTIFIER_H_

#include "base/callback.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_data_transfer_notifier.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/clipboard/clipboard_observer.h"

namespace content {
class WebContents;
}

namespace policy {

class DlpClipboardNotifier : public DlpDataTransferNotifier,
                             public ui::ClipboardObserver,
                             public content::WebContentsObserver {
 public:
  DlpClipboardNotifier();
  ~DlpClipboardNotifier() override;

  DlpClipboardNotifier(const DlpClipboardNotifier&) = delete;
  void operator=(const DlpClipboardNotifier&) = delete;

  // DlpDataTransferNotifier::
  // If the type of `data_dst` is kCrostini, kPluginVm or kArc, it will show a
  // toast instead of a bubble.
  void NotifyBlockedAction(
      const ui::DataTransferEndpoint* const data_src,
      const ui::DataTransferEndpoint* const data_dst) override;

  // Warns the user that this paste action is not recommended.
  // If the type of `data_dst` is kCrostini, kPluginVm or kArc, it will show a
  // toast instead of a bubble.
  void WarnOnPaste(const ui::DataTransferEndpoint* const data_src,
                   const ui::DataTransferEndpoint* const data_dst);

  // Warns the user that this paste action in Blink is not recommended.
  void WarnOnBlinkPaste(const ui::DataTransferEndpoint* const data_src,
                        const ui::DataTransferEndpoint* const data_dst,
                        content::WebContents* web_contents,
                        base::OnceCallback<void(bool)> paste_cb);

  // Returns true if the user approved to paste the clipboard data to this
  // `data_dst` before.
  bool DidUserApproveDst(const ui::DataTransferEndpoint* const data_dst);

  // Returns true if the user cancelled pasting the clipboard data to this
  // `data_dst` before.
  bool DidUserCancelDst(const ui::DataTransferEndpoint* const data_dst);

  void SetBlinkPasteCallbackForTesting(base::OnceCallback<void(bool)> paste_cb);

 protected:
  // Exposed for tests to override.
  void ProceedPressed(const ui::DataTransferEndpoint& data_dst,
                      views::Widget* widget);
  void BlinkProceedPressed(const ui::DataTransferEndpoint& data_dst,
                           views::Widget* widget);
  void CancelWarningPressed(const ui::DataTransferEndpoint& data_dst,
                            views::Widget* widget);
  void ResetUserWarnSelection();

 private:
  // Virtual for tests to override.
  virtual void ShowToast(const std::string& id,
                         const std::u16string& text) const;

  // ui::ClipboardObserver
  void OnClipboardDataChanged() override;

  // views::WidgetObserver
  void OnWidgetClosing(views::Widget* widget) override;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

  // Vector of destinations approved by the user on warning for copy/paste. It
  // gets reset when the clipboard data changes.
  std::vector<ui::DataTransferEndpoint> approved_dsts_;

  // Vector of destinations rejected by the user on warning for copy/paste. It
  // gets reset when the clipboard data changes.
  std::vector<ui::DataTransferEndpoint> cancelled_dsts_;

  // Blink paste callback.
  base::OnceCallback<void(bool)> blink_paste_cb_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CLIPBOARD_NOTIFIER_H_
