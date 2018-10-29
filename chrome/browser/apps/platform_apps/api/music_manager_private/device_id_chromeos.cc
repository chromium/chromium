// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/music_manager_private/device_id.h"

#include "chromeos/cryptohome/system_salt_getter.h"

namespace chrome_apps {
namespace api {

// static
void DeviceId::GetRawDeviceId(const IdCallback& callback) {
  chromeos::SystemSaltGetter::Get()->GetSystemSalt(callback);
}

}  // namespace api
}  // namespace chrome_apps
