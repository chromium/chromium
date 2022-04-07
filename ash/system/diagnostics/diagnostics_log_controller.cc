// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/diagnostics_log_controller.h"

#include "ash/system/diagnostics/diagnostics_browser_delegate.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"

namespace ash {
namespace diagnostics {

namespace {

DiagnosticsLogController* g_instance = nullptr;

// Placeholder session log contents.
const char kLogFileContents[] = "Diagnostics Log";

}  // namespace

DiagnosticsLogController::DiagnosticsLogController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

DiagnosticsLogController::~DiagnosticsLogController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
DiagnosticsLogController* DiagnosticsLogController::Get() {
  return g_instance;
}

// static
bool DiagnosticsLogController::IsInitialized() {
  return g_instance && g_instance->delegate_;
}

// static
void DiagnosticsLogController::Initialize(
    std::unique_ptr<DiagnosticsBrowserDelegate> delegate) {
  DCHECK(g_instance);
  g_instance->delegate_ = std::move(delegate);
}

bool DiagnosticsLogController::GenerateSessionLogOnBlockingPool(
    const base::FilePath& save_file_path) {
  DCHECK(!save_file_path.empty());

  // TODO(ashleydp): Replace |kLogFileContents| when actual log contents
  // available to write to file.
  return base::WriteFile(save_file_path, kLogFileContents);
}

}  // namespace diagnostics
}  // namespace ash
