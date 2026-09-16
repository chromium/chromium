// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_OPTIMIZATION_GUIDE_ON_DEVICE_MODEL_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_OPTIMIZATION_GUIDE_ON_DEVICE_MODEL_INSTALLER_H_

#include <memory>

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager.h"

namespace component_updater {

// Creates a generic delegate for Manifest Component.
std::unique_ptr<optimization_guide::ManifestAssetManager::Delegate>
CreateManifestAssetManagerDelegate();

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_OPTIMIZATION_GUIDE_ON_DEVICE_MODEL_INSTALLER_H_
