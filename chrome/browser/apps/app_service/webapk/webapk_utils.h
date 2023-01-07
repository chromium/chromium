// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_UTILS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_UTILS_H_

#include <stdint.h>

#include <string>

#include "base/functional/callback.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#include "components/webapk/webapk.pb.h"

class Profile;

namespace apps {

// Populates |web_app_manifest|, apart from icons, using WebAppRegistrar
// information.
void PopulateWebApkManifest(Profile* profile,
                            const std::string& app_id,
                            webapk::WebAppManifest* web_app_manifest);

#if BUILDFLAG(IS_CHROMEOS_LACROS)

using GetWebApkCreationParamsCallback = base::OnceCallback<void(
    crosapi::mojom::WebApkCreationParamsPtr webapk_creation_params)>;

// Called when a web app defining a share target has been installed in Lacros.
// Returns the manifest URL and a serialized webapk::WebAppManifest proto
// containing the information required for an Android WebApk to be minted.
void GetWebApkCreationParams(Profile* profile,
                             const std::string& app_id,
                             GetWebApkCreationParamsCallback callback);

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_UTILS_H_
