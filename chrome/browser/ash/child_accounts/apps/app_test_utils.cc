// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/apps/app_test_utils.h"

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"

namespace ash {

arc::mojom::ArcPackageInfoPtr CreateArcAppPackage(
    const std::string& package_name) {
  auto package = arc::mojom::ArcPackageInfo::New();
  package->package_name = package_name;
  package->package_version = 1;
  package->last_backup_android_id = 1;
  package->last_backup_time = 1;
  package->sync = false;
  return package;
}

arc::mojom::AppInfoPtr CreateArcAppInfo(const std::string& package_name,
                                        const std::string& name) {
  auto app = arc::mojom::AppInfo::New();
  app->package_name = package_name;
  app->name = name;
  app->activity = base::StrCat({name, "Activity"});
  app->sticky = true;
  return app;
}

scoped_refptr<extensions::Extension> CreateExtension(
    const std::string& extension_id,
    const std::string& name,
    const std::string& url) {
  base::Value::Dict manifest;
  manifest.Set(extensions::manifest_keys::kName, name);
  manifest.Set(extensions::manifest_keys::kVersion, "1");
  manifest.Set(extensions::manifest_keys::kManifestVersion, 2);
  manifest.SetByDottedPath(extensions::manifest_keys::kLaunchWebURL, url);

  std::string error;
  extensions::Extension::InitFromValueFlags flags =
      extensions::Extension::NO_FLAGS;
  scoped_refptr<extensions::Extension> extension =
      extensions::Extension::Create(
          base::FilePath(), extensions::mojom::ManifestLocation::kUnpacked,
          manifest, flags, extension_id, &error);
  return extension;
}

}  // namespace ash
