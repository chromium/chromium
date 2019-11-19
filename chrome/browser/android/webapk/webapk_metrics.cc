// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"

namespace webapk {

const char kInstallDurationHistogram[] = "WebApk.Install.InstallDuration";
const char kInstallEventHistogram[] = "WebApk.Install.InstallEvent";

void TrackRequestTokenDuration(base::TimeDelta delta) {
  UMA_HISTOGRAM_TIMES("WebApk.Install.RequestTokenDuration", delta);
}

void TrackInstallDuration(base::TimeDelta delta) {
  UMA_HISTOGRAM_MEDIUM_TIMES(kInstallDurationHistogram, delta);
}

void TrackInstallEvent(InstallEvent event) {
  UMA_HISTOGRAM_ENUMERATION(kInstallEventHistogram, event, INSTALL_EVENT_MAX);
}

}  // namespace webapk
