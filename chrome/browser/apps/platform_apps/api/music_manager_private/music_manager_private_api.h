// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_API_MUSIC_MANAGER_PRIVATE_MUSIC_MANAGER_PRIVATE_API_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_API_MUSIC_MANAGER_PRIVATE_MUSIC_MANAGER_PRIVATE_API_H_

#include "extensions/browser/extension_function.h"

namespace chrome_apps {
namespace api {

class MusicManagerPrivateGetDeviceIdFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("musicManagerPrivate.getDeviceId",
                             MUSICMANAGERPRIVATE_GETDEVICEID)

  MusicManagerPrivateGetDeviceIdFunction();

 protected:
  ~MusicManagerPrivateGetDeviceIdFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void DeviceIdCallback(const std::string& device_id);
};

}  // namespace api
}  // namespace chrome_apps

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_API_MUSIC_MANAGER_PRIVATE_MUSIC_MANAGER_PRIVATE_API_H_
