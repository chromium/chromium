// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/extension_apps_util.h"

#include <set>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/common/constants.h"

namespace apps {

extensions::UninstallReason GetExtensionUninstallReason(
    apps::mojom::UninstallSource uninstall_source) {
  switch (uninstall_source) {
    case apps::mojom::UninstallSource::kUnknown:
      // We assume if the reason is unknown that it's user inititated.
      return extensions::UNINSTALL_REASON_USER_INITIATED;
    case apps::mojom::UninstallSource::kAppList:
    case apps::mojom::UninstallSource::kAppManagement:
    case apps::mojom::UninstallSource::kShelf:
      return extensions::UNINSTALL_REASON_USER_INITIATED;
    case apps::mojom::UninstallSource::kMigration:
      return extensions::UNINSTALL_REASON_MIGRATED;
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool ExtensionAppRunsInAsh(const std::string& app_id) {
  static base::NoDestructor<std::set<std::string>> keep_list(
      {file_manager::kAudioPlayerAppId, extension_misc::kFeedbackExtensionId,
       extension_misc::kFilesManagerAppId, extension_misc::kGoogleKeepAppId,
       extension_misc::kCalculatorAppId, extension_misc::kTextEditorAppId,
       extension_misc::kInAppPaymentsSupportAppId,
       extension_misc::kWallpaperManagerId});
  return base::Contains(*keep_list, app_id);
}
#endif

}  // namespace apps
