// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/metrics/power/power_details_provider.h"

#import <Foundation/Foundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOGraphicsLib.h>

#include "base/mac/scoped_ioobject.h"

class PowerDetailsProviderMac : public PowerDetailsProvider {
 public:
  PowerDetailsProviderMac() : service_(CreateIOService()) {}
  ~PowerDetailsProviderMac() override = default;
  PowerDetailsProviderMac(const PowerDetailsProviderMac& rhs) = delete;
  PowerDetailsProviderMac& operator=(const PowerDetailsProviderMac& rhs) =
      delete;

  absl::optional<double> GetMainScreenBrightnessLevel() override {
    static const CFStringRef kDisplayBrightness =
        CFSTR(kIODisplayBrightnessKey);
    if (service_) {
      float brightness = 0;
      if (IODisplayGetFloatParameter(service_, kNilOptions, kDisplayBrightness,
                                     &brightness) == kIOReturnSuccess) {
        return static_cast<double>(brightness);
      }
    }
    return absl::nullopt;
  }

 private:
  static base::mac::ScopedIOObject<io_service_t> CreateIOService() {
    // NOTE: This is only available on Mac devices with an Intel processor. This
    // will return a null base::mac::ScopedIOObject for other architectures.
    return base::mac::ScopedIOObject<io_service_t>(IOServiceGetMatchingService(
        kIOMasterPortDefault, IOServiceMatching("IODisplayConnect")));
  }

  const base::mac::ScopedIOObject<io_service_t> service_;
};

std::unique_ptr<PowerDetailsProvider> PowerDetailsProvider::Create() {
  return std::make_unique<PowerDetailsProviderMac>();
}
