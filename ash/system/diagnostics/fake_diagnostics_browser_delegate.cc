// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/fake_diagnostics_browser_delegate.h"

#include <optional>

#include "base/files/file_path.h"

namespace ash::diagnostics {

FakeDiagnosticsBrowserDelegate::FakeDiagnosticsBrowserDelegate(
    std::optional<base::FilePath> path)
    : active_user_dir_(std::move(path)) {}

FakeDiagnosticsBrowserDelegate::~FakeDiagnosticsBrowserDelegate() = default;

std::optional<base::FilePath>
FakeDiagnosticsBrowserDelegate::GetActiveUserProfileDir() {
  return active_user_dir_;
}

}  // namespace ash::diagnostics
