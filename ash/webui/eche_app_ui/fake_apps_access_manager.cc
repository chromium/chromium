// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/fake_apps_access_manager.h"

namespace ash {
namespace eche_app {

FakeAppsAccessManager::FakeAppsAccessManager(AccessStatus access_status)
    : access_status_(access_status) {}

FakeAppsAccessManager::~FakeAppsAccessManager() = default;

AppsAccessManager::AccessStatus FakeAppsAccessManager::GetAccessStatus() const {
  return access_status_;
}

void FakeAppsAccessManager::SetAccessStatusInternal(
    AccessStatus access_status) {
  access_status_ = access_status;
  NotifyAppsAccessChanged();
}

void FakeAppsAccessManager::OnSetupRequested() {}

}  // namespace eche_app
}  // namespace ash
