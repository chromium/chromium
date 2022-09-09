// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/tab_restore_service_load_waiter.h"

TabRestoreServiceLoadWaiter::TabRestoreServiceLoadWaiter(
    sessions::TabRestoreService* service)
    : service_(service) {
  observation_.Observe(service_.get());
}

TabRestoreServiceLoadWaiter::~TabRestoreServiceLoadWaiter() = default;

void TabRestoreServiceLoadWaiter::Wait() {
  if (!service_->IsLoaded())
    run_loop_.Run();
}

void TabRestoreServiceLoadWaiter::TabRestoreServiceLoaded(
    sessions::TabRestoreService* service) {
  run_loop_.Quit();
}
