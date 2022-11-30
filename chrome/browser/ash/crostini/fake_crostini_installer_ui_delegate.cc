// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/fake_crostini_installer_ui_delegate.h"

namespace crostini {

FakeCrostiniInstallerUIDelegate::FakeCrostiniInstallerUIDelegate() = default;
FakeCrostiniInstallerUIDelegate::~FakeCrostiniInstallerUIDelegate() = default;

void FakeCrostiniInstallerUIDelegate::Install(
    CrostiniManager::RestartOptions options,
    ProgressCallback progress_callback,
    ResultCallback result_callback) {
  restart_options_ = std::move(options);
  progress_callback_ = std::move(progress_callback);
  result_callback_ = std::move(result_callback);
}

void FakeCrostiniInstallerUIDelegate::Cancel(base::OnceClosure callback) {
  cancel_callback_ = std::move(callback);
}

void FakeCrostiniInstallerUIDelegate::CancelBeforeStart() {
  cancel_before_start_called_ = true;
}

}  // namespace crostini
