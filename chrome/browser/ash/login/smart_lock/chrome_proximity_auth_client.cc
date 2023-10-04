// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/smart_lock/chrome_proximity_auth_client.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/ash/device_sync/device_sync_client_factory.h"
#include "chrome/browser/ash/login/smart_lock/smart_lock_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

namespace ash {

ChromeProximityAuthClient::ChromeProximityAuthClient(Profile* profile)
    : profile_(profile) {}

ChromeProximityAuthClient::~ChromeProximityAuthClient() {}

void ChromeProximityAuthClient::UpdateSmartLockState(SmartLockState state) {
  SmartLockService* service = SmartLockService::Get(profile_);
  if (service) {
    service->UpdateSmartLockState(state);
  }
}

void ChromeProximityAuthClient::FinalizeUnlock(bool success) {
  SmartLockService* service = SmartLockService::Get(profile_);
  if (service) {
    service->FinalizeUnlock(success);
  }
}

}  // namespace ash
