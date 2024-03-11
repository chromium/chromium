// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_MANAGER_FEATURE_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_MANAGER_FEATURE_H_

namespace crosapi {

// These ash features are allowed to request that Lacros stays running in the
// background.
enum class BrowserManagerFeature {
  kTestOnly,
  kAppService,
  kApkWebAppService,
  kChromeApps,
  kExtensions,
  kPersistentForcedExtension,
  kSmartCardSessionController,
  kDriveFsNativeMessaging,
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_MANAGER_FEATURE_H_
