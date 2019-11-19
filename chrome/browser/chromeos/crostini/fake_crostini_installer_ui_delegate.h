// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_FAKE_CROSTINI_INSTALLER_UI_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_FAKE_CROSTINI_INSTALLER_UI_DELEGATE_H_

#include "base/callback.h"
#include "chrome/browser/chromeos/crostini/crostini_installer_ui_delegate.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"

namespace crostini {

class FakeCrostiniInstallerUIDelegate : public CrostiniInstallerUIDelegate {
 public:
  FakeCrostiniInstallerUIDelegate();
  ~FakeCrostiniInstallerUIDelegate();

  void Install(CrostiniManager::RestartOptions options,
               ProgressCallback progress_callback,
               ResultCallback result_callback) override;
  void Cancel(base::OnceClosure callback) override;
  void CancelBeforeStart() override;

  CrostiniManager::RestartOptions restart_options_;
  ProgressCallback progress_callback_;
  ResultCallback result_callback_;
  base::OnceClosure cancel_callback_;
  bool cancel_before_start_called_ = false;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_FAKE_CROSTINI_INSTALLER_UI_DELEGATE_H_
