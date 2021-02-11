// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CLIPBOARD_NOTIFIER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CLIPBOARD_NOTIFIER_H_

#include "chrome/browser/chromeos/policy/dlp/dlp_data_transfer_notifier.h"
#include "ui/base/clipboard/clipboard_observer.h"

namespace policy {

class DlpClipboardNotifier : public DlpDataTransferNotifier,
                             public ui::ClipboardObserver {
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
  // If the type of `data_dst` is kCrostini, kPluginVm or kArc, it will show a
  // toast instead of a bubble.
  void WarnOnAction(const ui::DataTransferEndpoint* const data_src,
                    const ui::DataTransferEndpoint* const data_dst) override;

  // Returns true if the user approved to paste the clipboard data to this
  // |data_dst| before.
  bool DidUserProceedOnWarn(const ui::DataTransferEndpoint* const data_dst);

 protected:
  void ProceedOnWarn(const ui::DataTransferEndpoint& data_dst,
                     views::Widget* widget);

  void ResetUserWarnSelection();

 private:
  virtual void ShowToast(const std::string& id,
                         const base::string16& text) const;

  // ui::ClipboardObserver
  void OnClipboardDataChanged() override;

  // Vector of destinations approved by the user on warning for copy/paste. It
  // gets reset when the clipboard data changes.
  std::vector<ui::DataTransferEndpoint> approved_dsts_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CLIPBOARD_NOTIFIER_H_
