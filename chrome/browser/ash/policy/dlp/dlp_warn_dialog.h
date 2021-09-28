// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DLP_WARN_DIALOG_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DLP_WARN_DIALOG_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace policy {

// DlpWarnDialog is a base class that represents a system dialog shown when Data
// Leak Protection restriction level is set to WARN.
class DlpWarnDialog : public views::DialogDelegateView {
 public:
  METADATA_HEADER(DlpWarnDialog);

  enum class Restriction { kScreenCapture, kVideoCapture, kPrinting };

  DlpWarnDialog(base::OnceClosure accept_callback,
                base::OnceClosure cancel_callback,
                Restriction restriction);
  DlpWarnDialog(const DlpWarnDialog&) = delete;
  DlpWarnDialog& operator=(const DlpWarnDialog&) = delete;
  ~DlpWarnDialog() override = default;

 private:
  virtual void InitializeView(Restriction restriction);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DLP_WARN_DIALOG_H_
