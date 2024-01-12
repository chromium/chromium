// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/shortcut_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace apps {

const char kShortcutLaunchSourceHistogram[] = "Apps.Shortcut.LaunchSource";
const char kShortcutRemovalSourceHistogram[] = "Apps.Shortcut.RemovalSource";
const char kShortcutPinActionHistogram[] = "Apps.Shortcut.PinAction";

void RecordShortcutLaunchSource(const ShortcutActionSource action_source) {
  base::UmaHistogramEnumeration(kShortcutLaunchSourceHistogram, action_source);
}

void RecordShortcutRemovalSource(const ShortcutActionSource action_source) {
  base::UmaHistogramEnumeration(kShortcutRemovalSourceHistogram, action_source);
}

void RecordShortcutPinAction(const ShortcutPinAction pin_action) {
  base::UmaHistogramEnumeration(kShortcutPinActionHistogram, pin_action);
}

}  // namespace apps
