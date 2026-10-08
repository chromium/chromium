// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_OPTIMIZATION_GUIDE_ON_DEVICE_MODEL_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_OPTIMIZATION_GUIDE_ON_DEVICE_MODEL_INSTALLER_H_

#include <memory>

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager_delegate.h"

namespace optimization_guide {

// Overrides the task priority for getting free disk space in the context of the
// on-device model eligibility check to base::TaskPriority::USER_VISIBLE.
inline constexpr char kGetFreeDiskSpaceWithUserVisiblePriorityTaskSwitch[] =
    "optimization-guide-get-free-disk-space-with-user-visible-priority-task";

}  // namespace optimization_guide

namespace component_updater {

// Creates a generic delegate for Manifest Component.
std::unique_ptr<optimization_guide::ManifestAssetManagerDelegate>
CreateManifestAssetManagerDelegate();

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_OPTIMIZATION_GUIDE_ON_DEVICE_MODEL_INSTALLER_H_
