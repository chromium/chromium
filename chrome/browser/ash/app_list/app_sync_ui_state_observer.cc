// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_sync_ui_state_observer.h"

#include "base/check.h"

AppSyncUIStateObserver::~AppSyncUIStateObserver() {
  CHECK(!IsInObserverList());
}
