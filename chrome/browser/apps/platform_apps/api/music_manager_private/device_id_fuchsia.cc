// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/music_manager_private/device_id.h"

#include <string>

#include "base/callback.h"
#include "base/notreached.h"

namespace chrome_apps {
namespace api {

// static
void DeviceId::GetRawDeviceId(IdCallback callback) {
  // TODO(crbug.com/1234731)
  NOTIMPLEMENTED_LOG_ONCE();
  std::move(callback).Run(std::string());
}

}  // namespace api
}  // namespace chrome_apps
