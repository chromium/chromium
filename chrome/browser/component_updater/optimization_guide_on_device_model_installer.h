// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_OPTIMIZATION_GUIDE_ON_DEVICE_MODEL_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_OPTIMIZATION_GUIDE_ON_DEVICE_MODEL_INSTALLER_H_

#include <memory>

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager_delegate.h"

namespace base {
class FilePath;
}  // namespace base

namespace optimization_guide {
class ManifestProvider;
}  // namespace optimization_guide

namespace component_updater {

// Creates a generic delegate for Manifest Component.
std::unique_ptr<optimization_guide::ManifestAssetManagerDelegate>
CreateManifestAssetManagerDelegate();

// Creates a provider for the manifest component. Installations must contain
// `required_file_name` to pass verification. The metered connection parameter
// controls whether the component can update on metered networks.
std::unique_ptr<optimization_guide::ManifestProvider> CreateManifestProvider(
    base::FilePath required_file_name,
    bool allow_updates_on_metered_connections);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_OPTIMIZATION_GUIDE_ON_DEVICE_MODEL_INSTALLER_H_
