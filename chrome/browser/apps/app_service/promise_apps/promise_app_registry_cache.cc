// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_apps.h"

namespace apps {

PromiseAppRegistryCache::PromiseAppRegistryCache() = default;

PromiseAppRegistryCache::~PromiseAppRegistryCache() = default;

void PromiseAppRegistryCache::AddPromiseApp(PromiseAppPtr promise_app) {
  const PackageId& package_id = promise_app->package_id;
  promise_app_map_[package_id] = std::move(promise_app);
}

}  // namespace apps
