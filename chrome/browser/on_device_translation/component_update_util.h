// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_COMPONENT_UPDATE_UTIL_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_COMPONENT_UPDATE_UTIL_H_

#include <optional>
#include <string>

#include "components/on_device_translation/installer.h"
#include "components/update_client/update_client_errors.h"

namespace component_updater {
class ComponentUpdateService;
}  // namespace component_updater

namespace on_device_translation {

// Returns true when downloads are blocked by the metered-network killswitch
// and the current connection is metered.
bool IsMeteredDownloadBlocked();

// Returns why an on-demand update of the component `component_id` left the
// component unusable, or nullopt if it did not. Call this when the update
// finished with `error`. `is_installed` says whether a version of the
// component is installed now.
//
// The update callback reports `update_client::Error::NONE` even when the
// download or the install of the component failed, so this also looks at the
// last state that the component updater recorded for the component.
std::optional<InstallError> GetOnDemandUpdateError(
    component_updater::ComponentUpdateService* cus,
    const std::string& component_id,
    update_client::Error error,
    bool is_installed);

}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_COMPONENT_UPDATE_UTIL_H_
