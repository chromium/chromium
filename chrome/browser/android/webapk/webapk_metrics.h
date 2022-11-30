// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_METRICS_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_METRICS_H_

#include <string>

#include "base/time/time.h"
#include "components/webapps/browser/android/webapk/webapk_types.h"

namespace webapk {

// Keep these enums up to date with tools/metrics/histograms/histograms.xml.
// Events for WebAPKs installation flow. The sum of InstallEvent histogram
// is the total number of times that a WebAPK infobar was triggered.
enum InstallEvent {
  // Deprecated: INFOBAR_IGNORED = 0,
  // The add-to-homescreen dialog is dismissed without the user initiating a
  // WebAPK install.
  ADD_TO_HOMESCREEN_DIALOG_DISMISSED_BEFORE_INSTALLATION = 1,
  // Deprecated: INFOBAR_DISMISSED_DURING_INSTALLATION = 2,
  INSTALL_COMPLETED = 3,
  INSTALL_FAILED = 4,
  INSTALL_EVENT_MAX = 5,
};

void TrackRequestTokenDuration(base::TimeDelta delta,
                               const std::string& webapk_package);
void TrackInstallDuration(base::TimeDelta delta);
void TrackInstallEvent(InstallEvent event);
void TrackInstallResult(webapps::WebApkInstallResult result);

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_METRICS_H_
