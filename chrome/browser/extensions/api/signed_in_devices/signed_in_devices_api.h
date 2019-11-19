// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SIGNED_IN_DEVICES_SIGNED_IN_DEVICES_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_SIGNED_IN_DEVICES_SIGNED_IN_DEVICES_API_H__

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/extensions/chrome_extension_function.h"

class Profile;

namespace extensions {
class ExtensionPrefs;
}  // namespace extensions

namespace syncer {
class DeviceInfo;
class DeviceInfoTracker;
}  // namespace syncer

namespace extensions {

// Gets the list of signed in devices. The returned scoped vector will be
// filled with the list of devices associated with the account signed into this
// |profile|. This function needs the |extension_id| because the
// public device ids are set per extension.
std::vector<std::unique_ptr<syncer::DeviceInfo>> GetAllSignedInDevices(
    const std::string& extension_id,
    Profile* profile);

std::vector<std::unique_ptr<syncer::DeviceInfo>> GetAllSignedInDevices(
    const std::string& extension_id,
    syncer::DeviceInfoTracker* device_tracker,
    ExtensionPrefs* extension_prefs);

class SignedInDevicesGetFunction : public ExtensionFunction {
 protected:
  ~SignedInDevicesGetFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("signedInDevices.get", SIGNED_IN_DEVICES_GET)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SIGNED_IN_DEVICES_SIGNED_IN_DEVICES_API_H__
