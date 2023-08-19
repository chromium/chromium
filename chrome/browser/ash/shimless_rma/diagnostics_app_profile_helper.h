// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SHIMLESS_RMA_DIAGNOSTICS_APP_PROFILE_HELPER_H_
#define CHROME_BROWSER_ASH_SHIMLESS_RMA_DIAGNOSTICS_APP_PROFILE_HELPER_H_

#include "ash/webui/shimless_rma/backend/shimless_rma_delegate.h"
#include "base/files/file_path.h"

namespace ash::shimless_rma {

// Implements ShimlessRmaDelegate::PrepareDiagnosticsAppBrowserContext.
void PrepareDiagnosticsAppProfile(
    const base::FilePath& crx_path,
    const base::FilePath& swbn_path,
    ShimlessRmaDelegate::PrepareDiagnosticsAppBrowserContextCallback callback);

}  // namespace ash::shimless_rma

#endif  // CHROME_BROWSER_ASH_SHIMLESS_RMA_DIAGNOSTICS_APP_PROFILE_HELPER_H_
