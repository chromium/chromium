// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PACKAGE_ID_UTIL_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PACKAGE_ID_UTIL_H_

#include "build/chromeos_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace apps {

class AppUpdate;
class PackageId;

}  // namespace apps

namespace apps_util {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Generate the package ID for an app using its metadata. When an app has
// incomplete metadata or a type that package IDs cannot support, we return
// absl::nullopt since a package ID cannot be generated.
absl::optional<apps::PackageId> GetPackageIdForApp(
    Profile* profile,
    const apps::AppUpdate& update);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace apps_util

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PACKAGE_ID_UTIL_H_
