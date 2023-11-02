// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_METRICS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_METRICS_H_

#include "ash/components/arc/mojom/webapk.mojom-forward.h"

namespace apps {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class WebApkInstallStatus {
  kSuccess = 0,
  // Installation failed because the app was in an invalid state (e.g., it had
  // no suitable icon).
  kAppInvalid = 1,
  // Installation failed because ARC was not available.
  kArcUnavailable = 2,
  // Updating was cancelled because the existing WebAPK was found to be up to
  // date.
  kUpdateCancelledWebApkUpToDate = 3,
  // Updating failed because fetching information about the existing WebAPK from
  // ARC failed.
  kUpdateGetWebApkInfoError = 4,
  // Network request to the WebAPK server failed.
  kNetworkError = 5,
  // Network request to the WebAPK server timed out.
  kNetworkTimeout = 6,
  // Installation through Google Play failed.
  kGooglePlayError = 7,
  kMaxValue = kGooglePlayError,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class WebApkUninstallSource {
  // The WebAPK was uninstalled on the Ash side (e.g. uninstalling the web app
  // through App Management).
  kAsh = 0,
  // The WebAPK was uninstalled on the ARC side (e.g. uninstalling the app
  // through Android settings).
  kArc = 1,
  kMaxValue = kArc,
};

extern const char kWebApkInstallResultHistogram[];
extern const char kWebApkUpdateResultHistogram[];

// Records the overall result of installing/updating a WebAPK to UMA.
void RecordWebApkInstallResult(bool is_update, WebApkInstallStatus result);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_METRICS_H_
