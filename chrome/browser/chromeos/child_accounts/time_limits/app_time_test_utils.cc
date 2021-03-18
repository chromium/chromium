// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_test_utils.h"

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"

namespace chromeos {
namespace app_time {

arc::mojom::ArcPackageInfoPtr CreateArcAppPackage(
    const std::string& package_name) {
  auto package = arc::mojom::ArcPackageInfo::New();
  package->package_name = package_name;
  package->package_version = 1;
  package->last_backup_android_id = 1;
  package->last_backup_time = 1;
  package->sync = false;
  package->system = false;
  package->permissions = base::flat_map<::arc::mojom::AppPermission, bool>();
  return package;
}

arc::mojom::AppInfo CreateArcAppInfo(const std::string& package_name,
                                     const std::string& name) {
  arc::mojom::AppInfo app;
  app.package_name = package_name;
  app.name = name;
  app.activity = base::StrCat({name, "Activity"});
  app.sticky = true;
  return app;
}

scoped_refptr<extensions::Extension> CreateExtension(
    const std::string& extension_id,
    const std::string& name,
    const std::string& url,
    bool is_bookmark_app) {
  base::Value manifest(base::Value::Type::DICTIONARY);
  manifest.SetStringPath(extensions::manifest_keys::kName, name);
  manifest.SetStringPath(extensions::manifest_keys::kVersion, "1");
  manifest.SetIntPath(extensions::manifest_keys::kManifestVersion, 2);
  manifest.SetStringPath(extensions::manifest_keys::kLaunchWebURL, url);

  std::string error;
  extensions::Extension::InitFromValueFlags flags =
      is_bookmark_app ? extensions::Extension::FROM_BOOKMARK
                      : extensions::Extension::NO_FLAGS;
  scoped_refptr<extensions::Extension> extension =
      extensions::Extension::Create(
          base::FilePath(), extensions::mojom::ManifestLocation::kUnpacked,
          static_cast<base::DictionaryValue&>(manifest), flags, extension_id,
          &error);
  return extension;
}

}  // namespace app_time
}  // namespace chromeos
