// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_ENTERPRISE_CLIPBOARD_DLP_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_ENTERPRISE_CLIPBOARD_DLP_CONTROLLER_H_

#include "base/strings/string16.h"
#include "ui/base/clipboard/clipboard_dlp_controller.h"

namespace ui {
class ClipboardDataEndpoint;
}

namespace policy {

// EnterpriseClipboardDlpController is responsible for preventing leaks of
// confidential clipboard data by controlling read operations according to the
// policy rules set by the admin.
class EnterpriseClipboardDlpController : public ui::ClipboardDlpController {
 public:
  // Creates an instance of the class.
  // Indicates that restricting clipboard content is required.
  static void Init();

  EnterpriseClipboardDlpController(const EnterpriseClipboardDlpController&) =
      delete;
  void operator=(const EnterpriseClipboardDlpController&) = delete;

  // nullptr can be passed instead of |data_src| or |data_dst|. If clipboard
  // data read is not allowed, this function will show a toast to the user.
  bool IsDataReadAllowed(
      const ui::ClipboardDataEndpoint* const data_src,
      const ui::ClipboardDataEndpoint* const data_dst) const override;

 private:
  EnterpriseClipboardDlpController();
  ~EnterpriseClipboardDlpController() override;

  // Shows toast in case the access to the clipboard data is blocked.
  // TODO(crbug.com/1131670): Move `ShowBlockToast` to a separate util/helper.
  void ShowBlockToast(const base::string16& text) const;

  // The text will be different if the clipboard data is shared with Crostini
  // or Parallels or ARC.
  base::string16 GetToastText(
      const ui::ClipboardDataEndpoint* const data_src,
      const ui::ClipboardDataEndpoint* const data_dst) const;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_ENTERPRISE_CLIPBOARD_DLP_CONTROLLER_H_
