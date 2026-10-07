// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/install_product_component.h"

#include "build/build_config.h"

namespace component_updater {

#if !BUILDFLAG(IS_WIN)
ProductComponentInstallResult InstallProductComponent(
    const base::FilePath& inner_crx_path,
    const base::FilePath& user_data_dir) {
  return ProductComponentInstallResult::kLaunchFailed;
}
#endif

}  // namespace component_updater
