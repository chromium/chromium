// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_FAKE_HARDWARE_INFO_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_FAKE_HARDWARE_INFO_DELEGATE_H_

#include <memory>
#include <string>

#include "chrome/browser/chromeos/extensions/telemetry/api/hardware_info_delegate.h"

namespace chromeos {

class FakeHardwareInfoDelegate : public HardwareInfoDelegate {
 public:
  class Factory : public HardwareInfoDelegate::Factory {
   public:
    explicit Factory(std::string manufacturer);
    ~Factory() override;

   protected:
    // HardwareInfoDelegate::Factory:
    std::unique_ptr<HardwareInfoDelegate> CreateInstance() override;

   private:
    const std::string manufacturer_;
  };

  explicit FakeHardwareInfoDelegate(std::string manufacturer);
  ~FakeHardwareInfoDelegate() override;

  // HardwareInfoDelegate:
  void GetManufacturer(ManufacturerCallback callback) override;

 private:
  const std::string manufacturer_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_FAKE_HARDWARE_INFO_DELEGATE_H_
