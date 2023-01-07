// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_INSTALL_STATUS_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_INSTALL_STATUS_H_

#include "chrome/browser/ash/system_extensions/system_extension.h"
#include "chrome/browser/ash/system_extensions/system_extensions_status_or.h"

namespace ash {

enum class SystemExtensionsInstallStatus {
  // This is used for the default constructor of `StatusOrSystemExtension`.
  kUnknown,
  kFailedDirectoryMissing,
  kFailedManifestReadError,
  kFailedJsonErrorParsingManifest,
  kFailedIdMissing,
  kFailedIdInvalid,
  kFailedTypeMissing,
  kFailedTypeInvalid,
  kFailedServiceWorkerUrlMissing,
  kFailedServiceWorkerUrlInvalid,
  kFailedServiceWorkerUrlDifferentOrigin,
  kFailedNameMissing,
  kFailedNameEmpty,
  kFailedToCopyAssetsToProfileDir,
};

using InstallStatusOrSystemExtension =
    SystemExtensionsStatusOr<SystemExtensionsInstallStatus, SystemExtension>;

using InstallStatusOrSystemExtensionId =
    SystemExtensionsStatusOr<SystemExtensionsInstallStatus, SystemExtensionId>;

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_INSTALL_STATUS_H_
