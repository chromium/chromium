// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DATA_TRANSFER_DLP_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DATA_TRANSFER_DLP_CONTROLLER_H_

#include "base/strings/string16.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"

namespace ui {
class DataTransferEndpoint;
}

namespace policy {

// DataTransferDlpController is responsible for preventing leaks of confidential
// data through clipboard data read or drag-and-drop by controlling read
// operations according to the rules of the Data leak prevention policy set by
// the admin.
class DataTransferDlpController : public ui::DataTransferPolicyController {
 public:
  // Creates an instance of the class.
  // Indicates that restricting clipboard content and drag-n-drop is required.
  static void Init();

  DataTransferDlpController(const DataTransferDlpController&) = delete;
  void operator=(const DataTransferDlpController&) = delete;

  // nullptr can be passed instead of `data_src` or `data_dst`. If data read is
  // not allowed, this function will show a toast to the user.
  bool IsDataReadAllowed(
      const ui::DataTransferEndpoint* const data_src,
      const ui::DataTransferEndpoint* const data_dst) const override;

 private:
  DataTransferDlpController();
  ~DataTransferDlpController() override;

  // Shows toast in case the data read is blocked.
  // TODO(crbug.com/1131670): Move `ShowBlockToast` to a separate util/helper.
  void ShowBlockToast(const base::string16& text) const;

  // The text will be different if the data transferred is being shared with
  // Crostini or Parallels or ARC.
  base::string16 GetToastText(
      const ui::DataTransferEndpoint* const data_src,
      const ui::DataTransferEndpoint* const data_dst) const;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DATA_TRANSFER_DLP_CONTROLLER_H_
