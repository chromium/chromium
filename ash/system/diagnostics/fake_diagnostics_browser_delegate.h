// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DIAGNOSTICS_FAKE_DIAGNOSTICS_BROWSER_DELEGATE_H_
#define ASH_SYSTEM_DIAGNOSTICS_FAKE_DIAGNOSTICS_BROWSER_DELEGATE_H_

#include <optional>

#include "ash/system/diagnostics/diagnostics_browser_delegate.h"
#include "base/files/file_path.h"

namespace ash::diagnostics {

namespace {

constexpr char kDefaultUserDir[] = "/fake/user-dir";

}  // namespace

// Fake delegate used to set the expected user directory path.
class FakeDiagnosticsBrowserDelegate : public DiagnosticsBrowserDelegate {
 public:
  explicit FakeDiagnosticsBrowserDelegate(
      std::optional<base::FilePath> path = base::FilePath(kDefaultUserDir));

  FakeDiagnosticsBrowserDelegate(const FakeDiagnosticsBrowserDelegate&) =
      delete;
  FakeDiagnosticsBrowserDelegate& operator=(
      const FakeDiagnosticsBrowserDelegate&) = delete;

  ~FakeDiagnosticsBrowserDelegate() override;

  std::optional<base::FilePath> GetActiveUserProfileDir() override;

 private:
  std::optional<base::FilePath> active_user_dir_;
};

}  // namespace ash::diagnostics

#endif  // ASH_SYSTEM_DIAGNOSTICS_FAKE_DIAGNOSTICS_BROWSER_DELEGATE_H_
