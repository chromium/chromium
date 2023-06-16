// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/metrics/managed_screensaver_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace ash {

namespace {

constexpr base::StringPiece kManagedScreensaverHistogramPrefix =
    "Enterprise.ManagedScreensaver.";

}  // namespace

std::string GetManagedScreensaverHistogram(
    const base::StringPiece& histogram_suffix) {
  return base::StrCat({kManagedScreensaverHistogramPrefix, histogram_suffix});
}

void RecordManagedScreensaverEnabled(bool enabled) {
  base::UmaHistogramBoolean(
      GetManagedScreensaverHistogram(kManagedScreensaverEnabledUMA), enabled);
}

}  // namespace ash
