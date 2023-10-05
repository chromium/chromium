// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_HELPERS_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_HELPERS_H_

#include "components/webapps/common/web_app_id.h"

namespace webapk {

// Generates the chrome-specific `AppId` from the spec-defined manifest id. See
// the `AppId` type for more information.
webapps::AppId GenerateAppIdFromManifestId(
    const webapps::ManifestId& manifest_id);

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_HELPERS_H_
