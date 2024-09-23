// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_HARDWARE_INFO_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_HARDWARE_INFO_DELEGATE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/remote_probe_service_strategy.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"

namespace chromeos {

// HardwareInfoDelegate is a helper class to get hardware info such as device
// manufacturer.
class HardwareInfoDelegate {
 public:
  static HardwareInfoDelegate& Get();

  using ManufacturerCallback = base::OnceCallback<void(const std::string&)>;

  HardwareInfoDelegate(const HardwareInfoDelegate&) = delete;
  HardwareInfoDelegate& operator=(const HardwareInfoDelegate&) = delete;
  virtual ~HardwareInfoDelegate();

  virtual void GetManufacturer(ManufacturerCallback done_cb);

  // Clears the manufacturer cache. Should be called in test to ensure the
  // correct manufacturer is returned.
  void ClearCacheForTesting();

 protected:
  HardwareInfoDelegate();

 private:
  void SetCacheAndReturnResult(ManufacturerCallback done_cb,
                               const std::string& manufacturer);

  // Each call to getManufacturer will access healthd and spawn a delegate
  // process. We can store the result in cache to optimize performance.
  std::optional<std::string> manufacturer_cache_ = std::nullopt;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_HARDWARE_INFO_DELEGATE_H_
