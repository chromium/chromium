// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace apps {

const char kPromiseAppLifecycleEventHistogram[] =
    "Apps.PromiseApp.LifecycleEvent";

void RecordPromiseAppLifecycleEvent(const PromiseAppLifecycleEvent event) {
  base::UmaHistogramEnumeration(kPromiseAppLifecycleEventHistogram, event);
}

}  // namespace apps
