// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/music_manager_private/music_manager_private_api.h"

#include <memory>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/apps/platform_apps/api/music_manager_private/device_id.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

const char kDeviceIdNotSupported[] =
    "Device ID API is not supported on this platform.";
}

namespace chrome_apps {
namespace api {

MusicManagerPrivateGetDeviceIdFunction::
    MusicManagerPrivateGetDeviceIdFunction() {}

MusicManagerPrivateGetDeviceIdFunction::
    ~MusicManagerPrivateGetDeviceIdFunction() {}

ExtensionFunction::ResponseAction
MusicManagerPrivateGetDeviceIdFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DeviceId::GetDeviceId(
      extension_id(),
      base::Bind(&MusicManagerPrivateGetDeviceIdFunction::DeviceIdCallback,
                 this));
  // GetDeviceId will respond asynchronously.
  return RespondLater();
}

void MusicManagerPrivateGetDeviceIdFunction::DeviceIdCallback(
    const std::string& device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (device_id.empty()) {
    Respond(Error(kDeviceIdNotSupported));
  } else {
    Respond(OneArgument(std::make_unique<base::Value>(device_id)));
  }
}

}  // namespace api
}  // namespace chrome_apps
