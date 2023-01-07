// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNALS_DEVICE_INFO_FETCHER_MAC_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNALS_DEVICE_INFO_FETCHER_MAC_H_

#include "chrome/browser/enterprise/signals/device_info_fetcher.h"

namespace enterprise_signals {

// MacOS implementation of DeviceInfoFetcher.
class DeviceInfoFetcherMac : public DeviceInfoFetcher {
 public:
  DeviceInfoFetcherMac();
  ~DeviceInfoFetcherMac() override;

  DeviceInfoFetcherMac(const DeviceInfoFetcherMac&) = delete;
  DeviceInfoFetcherMac& operator=(const DeviceInfoFetcherMac&) = delete;

  // Fetches the device info for the current platform.
  DeviceInfo Fetch() override;
};

}  // namespace enterprise_signals

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNALS_DEVICE_INFO_FETCHER_MAC_H_
