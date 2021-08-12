// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DLP_WARN_DIALOG_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DLP_WARN_DIALOG_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace policy {

// PrintWarnDialog is a system dialog that is shown when printing restriction
// level is set to WARN.
class PrintWarnDialog : public views::DialogDelegateView {
 public:
  METADATA_HEADER(PrintWarnDialog);
  PrintWarnDialog(base::OnceClosure accept_callback,
                  base::OnceClosure cancel_callback);
  PrintWarnDialog(const PrintWarnDialog&) = delete;
  PrintWarnDialog& operator=(const PrintWarnDialog&) = delete;
  ~PrintWarnDialog() override;

 private:
  void InitializeView();
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DLP_WARN_DIALOG_H_
