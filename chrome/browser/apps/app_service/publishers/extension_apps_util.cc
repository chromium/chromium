// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/extension_apps_util.h"

namespace apps {

extensions::UninstallReason GetExtensionUninstallReason(
    UninstallSource uninstall_source) {
  switch (uninstall_source) {
    case UninstallSource::kUnknown:
      // We assume if the reason is unknown that it's user inititated.
      return extensions::UNINSTALL_REASON_USER_INITIATED;
    case UninstallSource::kAppList:
    case UninstallSource::kAppManagement:
    case UninstallSource::kShelf:
      return extensions::UNINSTALL_REASON_USER_INITIATED;
    case UninstallSource::kMigration:
      return extensions::UNINSTALL_REASON_MIGRATED;
  }
}

}  // namespace apps
