// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/android_app_info_generator.h"

#include <map>
#include <sstream>

#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/arc/mojom/app_permissions.mojom.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace {

void ExtractPackagePermissions(
    const base::flat_map<arc::mojom::AppPermission,
                         arc::mojom::PermissionStatePtr>& permissions,
    em::AndroidAppInfo* app_info) {
  for (auto it = permissions.begin(); it != permissions.end(); it++) {
    std::stringstream stream;
    stream << it->first;

    em::AndroidAppPermission* app_permission = app_info->add_permissions();
    app_permission->set_name(stream.str());
    app_permission->set_granted(it->second->granted);
    app_permission->set_managed(it->second->managed);
  }
}

em::AndroidAppInfo::AndroidAppStatus ExtractAppStatus(
    const ArcAppListPrefs::AppInfo& app) {
  if (app.suspended)
    return em::AndroidAppInfo::STATUS_SUSPENDED;
  else if (app.ready)
    return em::AndroidAppInfo::STATUS_ENABLED;
  else
    return em::AndroidAppInfo::STATUS_DISABLED;
}

em::AndroidAppInfo::InstalledSource ExtractInstalledSource(
    const ArcAppListPrefs& prefs,
    const ArcAppListPrefs::AppInfo& app) {
  if (!app.ready)
    return em::AndroidAppInfo::SOURCE_NOT_INSTALLED;

  if (prefs.IsControlledByPolicy(app.package_name))
    return em::AndroidAppInfo::SOURCE_BY_ADMIN;
  else
    return em::AndroidAppInfo::SOURCE_BY_USER;
}

}  // namespace

namespace enterprise_reporting {

std::unique_ptr<em::AndroidAppInfo> AndroidAppInfoGenerator::Generate(
    ArcAppListPrefs* prefs,
    const std::string& app_id) const {
  // An AppInfo instance will always be generated.
  auto info = std::make_unique<em::AndroidAppInfo>();

  // Collect application information if possible.
  std::unique_ptr<ArcAppListPrefs::AppInfo> app = prefs->GetApp(app_id);

  if (app) {
    info->set_app_id(prefs->GetAppIdByPackageName(app->package_name));
    info->set_app_name(app->name);
    info->set_package_name(app->package_name);
    info->set_status(ExtractAppStatus(*app));
    info->set_installed_source(ExtractInstalledSource(*prefs, *app));

    // Collect package information if possible.
    std::unique_ptr<ArcAppListPrefs::PackageInfo> package =
        prefs->GetPackage(app->package_name);

    if (package) {
      info->set_version(package->package_version);
      ExtractPackagePermissions(package->permissions, info.get());
    } else {
      LOG(ERROR) << base::StringPrintf(
          "Package (package name: %s) can not be found.",
          app->package_name.c_str());
    }
  } else {
    LOG(ERROR) << base::StringPrintf("App (app id: %s) can not be found.",
                                     app_id.c_str());
  }

  return info;
}

}  // namespace enterprise_reporting
