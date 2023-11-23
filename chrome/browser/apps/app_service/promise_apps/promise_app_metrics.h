// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_METRICS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_METRICS_H_

namespace apps {

extern const char kPromiseAppLifecycleEventHistogram[];
extern const char kPromiseAppIconTypeHistogram[];
extern const char kPromiseAppTypeHistogram[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PromiseAppLifecycleEvent {
  // Tracks events in the PromiseAppRegistryCache.
  kCreatedInCache = 0,
  kInstallationSucceeded = 1,
  kInstallationCancelled = 2,

  // Tracks events in the client.
  kCreatedInLauncher = 3,
  kCreatedInShelf = 4,
  kMaxValue = kCreatedInShelf
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PromiseAppIconType {
  kPlaceholderIcon = 0,
  kRealIcon = 1,
  kMaxValue = kRealIcon
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PromiseAppType {
  kUnknown = 0,
  kArc = 1,
  kTwa = 2,
  kMaxValue = kTwa
};

void RecordPromiseAppLifecycleEvent(const PromiseAppLifecycleEvent event);

void RecordPromiseAppIconType(const PromiseAppIconType icon_type);

void RecordPromiseAppType(const PromiseAppType app_type);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_METRICS_H_
