// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/webapk/webapk_metrics.h"

#include "ash/components/arc/mojom/webapk.mojom.h"
#include "base/metrics/histogram_functions.h"

namespace apps {

const char kWebApkInstallResultHistogram[] = "ChromeOS.WebAPK.Install.Result";
const char kWebApkUpdateResultHistogram[] = "ChromeOS.WebAPK.Update.Result";
const char kWebApkArcInstallResultHistogram[] =
    "ChromeOS.WebAPK.Install.ArcInstallResult";
const char kWebApkArcUpdateResultHistogram[] =
    "ChromeOS.WebAPK.Update.ArcInstallResult";

void RecordWebApkInstallResult(bool is_update, WebApkInstallStatus result) {
  const char* histogram =
      is_update ? kWebApkUpdateResultHistogram : kWebApkInstallResultHistogram;
  base::UmaHistogramEnumeration(histogram, result);
}

void RecordWebApkArcResult(bool is_update,
                           arc::mojom::WebApkInstallResult error) {
  const char* histogram = is_update ? kWebApkArcUpdateResultHistogram
                                    : kWebApkArcInstallResultHistogram;
  base::UmaHistogramEnumeration(histogram, error);
}

}  // namespace apps
