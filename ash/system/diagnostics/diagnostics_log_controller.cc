// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/diagnostics_log_controller.h"

#include "ash/system/diagnostics/diagnostics_browser_delegate.h"
#include "base/check_op.h"
#include "base/notreached.h"

namespace ash {
namespace diagnostics {

namespace {

DiagnosticsLogController* g_instance = nullptr;

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

}  // namespace diagnostics
}  // namespace ash
