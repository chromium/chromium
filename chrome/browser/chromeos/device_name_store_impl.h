// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DEVICE_NAME_STORE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_DEVICE_NAME_STORE_IMPL_H_

#include "chrome/browser/chromeos/device_name_store.h"

class PrefService;

namespace chromeos {

// DeviceNameStore implementation which uses a PrefService to store the device
// name.
class DeviceNameStoreImpl : public DeviceNameStore {
 public:
  explicit DeviceNameStoreImpl(PrefService* prefs);
  ~DeviceNameStoreImpl() override;

 private:
  // DeviceNameStore:
  std::string GetDeviceName() const override;

  // Provides access and persistence for the device name value.
  PrefService* prefs_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DEVICE_NAME_STORE_IMPL_H_
