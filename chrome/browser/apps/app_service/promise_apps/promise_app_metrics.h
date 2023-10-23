// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_METRICS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_METRICS_H_

namespace apps {

extern const char kPromiseAppLifecycleEventHistogram[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PromiseAppLifecycleEvent {
  kCreatedInCache = 0,
  kInstallationSucceeded = 1,
  kInstallationCancelled = 2,
  kMaxValue = kInstallationCancelled
};

void RecordPromiseAppLifecycleEvent(const PromiseAppLifecycleEvent event);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_METRICS_H_
