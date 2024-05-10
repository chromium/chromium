// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"

namespace webapk {

const char kInstallDurationHistogram[] = "WebApk.Install.InstallDuration";
const char kInstallEventHistogram[] = "WebApk.Install.InstallEvent";
const char kInstallResultHistogram[] = "WebApk.Install.InstallResult";

void TrackRequestTokenDuration(base::TimeDelta delta,
                               const std::string& webapk_package) {
  UMA_HISTOGRAM_MEDIUM_TIMES("WebApk.Install.RequestTokenDurationV2", delta);
}

void TrackInstallDuration(base::TimeDelta delta) {
  UMA_HISTOGRAM_MEDIUM_TIMES(kInstallDurationHistogram, delta);
}

void TrackInstallEvent(InstallEvent event) {
  UMA_HISTOGRAM_ENUMERATION(kInstallEventHistogram, event, INSTALL_EVENT_MAX);
}

void TrackInstallResult(webapps::WebApkInstallResult result) {
  UMA_HISTOGRAM_ENUMERATION(kInstallResultHistogram, result);
}

}  // namespace webapk
