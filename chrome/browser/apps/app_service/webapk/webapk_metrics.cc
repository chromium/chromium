// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/webapk/webapk_metrics.h"

#include "ash/components/arc/mojom/webapk.mojom.h"
#include "base/metrics/histogram_functions.h"

namespace apps {

const char kWebApkInstallResultHistogram[] = "ChromeOS.WebAPK.Install.Result";
const char kWebApkUpdateResultHistogram[] = "ChromeOS.WebAPK.Update.Result";

void RecordWebApkInstallResult(bool is_update, WebApkInstallStatus result) {
  const char* histogram =
      is_update ? kWebApkUpdateResultHistogram : kWebApkInstallResultHistogram;
  base::UmaHistogramEnumeration(histogram, result);
}
}  // namespace apps
