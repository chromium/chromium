// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APPS_WEBAPK_WEBAPK_UTILS_H_
#define CHROME_BROWSER_ASH_APPS_WEBAPK_WEBAPK_UTILS_H_

#include <stdint.h>

#include <string>

#include "components/webapk/webapk.pb.h"

class Profile;

namespace apps {

// Populates |web_app_manifest|, apart from icons, using WebAppRegistrar
// information.
void PopulateWebApkManifest(Profile* profile,
                            const std::string& app_id,
                            webapk::WebAppManifest* web_app_manifest);

}  // namespace apps

#endif  // CHROME_BROWSER_ASH_APPS_WEBAPK_WEBAPK_UTILS_H_
