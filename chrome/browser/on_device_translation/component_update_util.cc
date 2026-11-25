// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/component_update_util.h"

#include "base/feature_list.h"
#include "components/component_updater/component_updater_service.h"
#include "components/on_device_translation/features.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "net/base/network_change_notifier.h"

namespace on_device_translation {

bool IsMeteredDownloadBlocked() {
  return base::FeatureList::IsEnabled(
             kTranslationAPIBlockDownloadsOnMeteredNetwork) &&
         net::NetworkChangeNotifier::GetConnectionCost() ==
             net::NetworkChangeNotifier::CONNECTION_COST_METERED;
}

std::optional<InstallError> GetOnDemandUpdateError(
    component_updater::ComponentUpdateService* cus,
    const std::string& component_id,
    update_client::Error error,
    bool is_installed) {
  if (is_installed) {
    // An installed version is usable even if the update failed.
    return std::nullopt;
  }
  switch (error) {
    case update_client::Error::NONE:
      break;
    case update_client::Error::UPDATE_IN_PROGRESS:
      // Another update of the component reports the outcome. If that is the
      // periodic update check, nothing reports its failure to the installer,
      // and pending requests wait until a later request asks again.
      // TODO(crbug.com/364795294): Watch the component updater events for the
      // component, so that this case reports a failure on every platform.
      return std::nullopt;
    default:
      return InstallError::kFailed;
  }

  component_updater::CrxUpdateItem item;
  if (!cus->GetComponentDetails(component_id, &item)) {
    return std::nullopt;
  }
  switch (item.state) {
    case update_client::ComponentState::kUpdateError:
      if (item.error_category == update_client::ErrorCategory::kDownload &&
          item.error_code ==
              static_cast<int>(update_client::CrxDownloaderError::DISK_FULL)) {
        return InstallError::kNotEnoughDiskSpace;
      }
      if (IsMeteredDownloadBlocked() &&
          item.error_category == update_client::ErrorCategory::kService &&
          (item.error_code ==
               static_cast<int>(update_client::ServiceError::CANCELLED) ||
           item.error_code ==
               static_cast<int>(
                   update_client::ServiceError::UPDATE_DISABLED))) {
        return InstallError::kMeteredConnectionBlocked;
      }
      return InstallError::kFailed;
    case update_client::ComponentState::kUpToDate:
      // The server has no version of the component for this client.
      return InstallError::kFailed;
    default:
      return std::nullopt;
  }
}

}  // namespace on_device_translation
