// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DATA_TRANSFER_DLP_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DATA_TRANSFER_DLP_CONTROLLER_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_clipboard_notifier.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_drag_drop_notifier.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"

namespace ui {
class DataTransferEndpoint;
}

namespace policy {

class DlpRulesManager;

// DataTransferDlpController is responsible for preventing leaks of confidential
// data through clipboard data read or drag-and-drop by controlling read
// operations according to the rules of the Data leak prevention policy set by
// the admin.
class DataTransferDlpController : public ui::DataTransferPolicyController {
 public:
  // Creates an instance of the class.
  // Indicates that restricting clipboard content and drag-n-drop is required.
  // It's guaranteed that `dlp_rules_manager` controls the lifetime of
  // DataTransferDlpController and outlives it.
  static void Init(const DlpRulesManager& dlp_rules_manager);

  DataTransferDlpController(const DataTransferDlpController&) = delete;
  void operator=(const DataTransferDlpController&) = delete;

  // ui::DataTransferPolicyController:
  bool IsClipboardReadAllowed(
      const ui::DataTransferEndpoint* const data_src,
      const ui::DataTransferEndpoint* const data_dst) override;
  void PasteIfAllowed(const ui::DataTransferEndpoint* const data_src,
                      const ui::DataTransferEndpoint* const data_dst,
                      content::WebContents* web_contents,
                      base::OnceCallback<void(bool)> callback) override;
  bool IsDragDropAllowed(const ui::DataTransferEndpoint* const data_src,
                         const ui::DataTransferEndpoint* const data_dst,
                         const bool is_drop) override;

 protected:
  explicit DataTransferDlpController(const DlpRulesManager& dlp_rules_manager);
  ~DataTransferDlpController() override;

 private:
  virtual void NotifyBlockedPaste(
      const ui::DataTransferEndpoint* const data_src,
      const ui::DataTransferEndpoint* const data_dst);

  virtual void WarnOnPaste(const ui::DataTransferEndpoint* const data_src,
                           const ui::DataTransferEndpoint* const data_dst);

  virtual void WarnOnBlinkPaste(const ui::DataTransferEndpoint* const data_src,
                                const ui::DataTransferEndpoint* const data_dst,
                                content::WebContents* web_contents,
                                base::OnceCallback<void(bool)> paste_cb);

  virtual bool ShouldPasteOnWarn(
      const ui::DataTransferEndpoint* const data_dst);

  virtual bool ShouldCancelOnWarn(
      const ui::DataTransferEndpoint* const data_dst);

  virtual void NotifyBlockedDrop(
      const ui::DataTransferEndpoint* const data_src,
      const ui::DataTransferEndpoint* const data_dst);

  const DlpRulesManager& dlp_rules_manager_;
  DlpClipboardNotifier clipboard_notifier_;
  DlpDragDropNotifier drag_drop_notifier_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DATA_TRANSFER_DLP_CONTROLLER_H_
