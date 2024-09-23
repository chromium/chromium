// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_manager_scoped_keep_alive.h"

// TODO: remove this circular dependency.
#include "chrome/browser/ash/crosapi/browser_manager.h"

namespace crosapi {

BrowserManagerScopedKeepAlive::~BrowserManagerScopedKeepAlive() {
  manager_->StopKeepAlive(feature_);
}

BrowserManagerScopedKeepAlive::BrowserManagerScopedKeepAlive(
    BrowserManager* manager,
    BrowserManagerFeature feature)
    : manager_(manager), feature_(feature) {
  manager_->StartKeepAlive(feature_);
}

}  // namespace crosapi
