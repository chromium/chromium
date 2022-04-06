// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/diagnostics/diagnostics_browser_delegate_impl.h"

#include "base/files/file_path.h"

namespace ash {
namespace diagnostics {

base::FilePath DiagnosticsBrowserDelegateImpl::GetActiveUserProfileDir() {
  return base::FilePath();
}

}  // namespace diagnostics
}  // namespace ash
