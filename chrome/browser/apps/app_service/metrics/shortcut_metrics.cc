// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/shortcut_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace apps {

const char kShortcutLaunchSourceHistogram[] = "Apps.Shortcut.LaunchSource";

void RecordShortcutLaunchSource(const ShortcutLaunchSource launch_source) {
  base::UmaHistogramEnumeration(kShortcutLaunchSourceHistogram, launch_source);
}

}  // namespace apps
