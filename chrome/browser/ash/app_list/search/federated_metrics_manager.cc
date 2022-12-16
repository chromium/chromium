// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/federated_metrics_manager.h"

namespace app_list {

namespace {

bool IsLoggingEnabled() {
  // TODO(b/262611120): Check user metrics opt-in/out, federated feature flag,
  // etc.
  return false;
}

}  // namespace

FederatedMetricsManager::FederatedMetricsManager(
    ash::AppListNotifier* notifier) {
  // TODO(b/262611120): Implement.
}

FederatedMetricsManager::~FederatedMetricsManager() = default;

void FederatedMetricsManager::OnAbandon(Location location,
                                        const std::vector<Result>& results,
                                        const std::u16string& query) {
  // TODO(b/262611120): Implement.
  if (!IsLoggingEnabled()) {
    return;
  }
}

void FederatedMetricsManager::OnLaunch(Location location,
                                       const Result& launched,
                                       const std::vector<Result>& shown,
                                       const std::u16string& query) {
  // TODO(b/262611120): Implement.
  if (!IsLoggingEnabled()) {
    return;
  }
}

}  // namespace app_list
