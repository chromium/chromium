// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DIAGNOSTICS_DIAGNOSTICS_BROWSER_DELEGATE_H_
#define ASH_SYSTEM_DIAGNOSTICS_DIAGNOSTICS_BROWSER_DELEGATE_H_

#include "ash/ash_export.h"
#include "base/files/file_path.h"

namespace ash {
namespace diagnostics {

// Interface for retrieving state information from browser to be used by
// DiagnosticsLogController.
class ASH_EXPORT DiagnosticsBrowserDelegate {
 public:
  DiagnosticsBrowserDelegate();
  DiagnosticsBrowserDelegate(const DiagnosticsBrowserDelegate&) = delete;
  DiagnosticsBrowserDelegate& operator=(const DiagnosticsBrowserDelegate&) =
      delete;
  virtual ~DiagnosticsBrowserDelegate();

  // Override to retrieve full path to active user's profile directory or an
  // empty path if there is no active user or the user's profile has not been
  // loaded yet.
  virtual base::FilePath GetActiveUserProfileDir() = 0;
};

}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_SYSTEM_DIAGNOSTICS_DIAGNOSTICS_BROWSER_DELEGATE_H_
