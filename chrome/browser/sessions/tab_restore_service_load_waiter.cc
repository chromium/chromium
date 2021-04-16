// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/tab_restore_service_load_waiter.h"

TabRestoreServiceLoadWaiter::TabRestoreServiceLoadWaiter(
    sessions::TabRestoreService* service)
    : service_(service) {
  observer_.Add(service_);
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
