// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_LOADER_DEVICE_LOCAL_ACCOUNT_EXTERNAL_POLICY_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_LOADER_DEVICE_LOCAL_ACCOUNT_EXTERNAL_POLICY_LOADER_H_

#include <optional>

#include "base/values.h"
#include "chrome/browser/extensions/external_loader.h"

namespace chromeos {

// A specialization of the ExternalLoader that serves external extensions from
// the enterprise policy force-install list. This class is used for device-local
// accounts in place of the ExternalPolicyLoader. The difference is that while
// the ExternalPolicyLoader requires extensions to be downloaded on-the-fly,
// this loader installs them from a cached location as provided via
// OnExtensionListsUpdated.
class DeviceLocalAccountExternalPolicyLoader
    : public extensions::ExternalLoader {
 public:
  DeviceLocalAccountExternalPolicyLoader();
  DeviceLocalAccountExternalPolicyLoader(
      const DeviceLocalAccountExternalPolicyLoader&) = delete;
  DeviceLocalAccountExternalPolicyLoader& operator=(
      const DeviceLocalAccountExternalPolicyLoader&) = delete;

  // extensions::ExternalLoader
  void StartLoading() override;

  // Update the list of extensions to be installed.
  // The dictionary should adhere to the interface of
  // ExternalLoader::LoadFinished.
  void OnExtensionListsUpdated(const base::Value::Dict& prefs);

 private:
  // If the cache was started, it must be stopped before |this| is destroyed.
  ~DeviceLocalAccountExternalPolicyLoader() override;

  std::optional<base::Value::Dict> prefs_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_LOADER_DEVICE_LOCAL_ACCOUNT_EXTERNAL_POLICY_LOADER_H_
