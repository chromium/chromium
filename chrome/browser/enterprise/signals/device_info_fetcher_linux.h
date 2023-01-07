// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNALS_DEVICE_INFO_FETCHER_LINUX_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNALS_DEVICE_INFO_FETCHER_LINUX_H_

#include "chrome/browser/enterprise/signals/device_info_fetcher.h"

namespace enterprise_signals {

// Linux implementation of DeviceInfoFetcher.
class DeviceInfoFetcherLinux : public DeviceInfoFetcher {
 public:
  DeviceInfoFetcherLinux();
  DeviceInfoFetcherLinux(const DeviceInfoFetcherLinux&) = delete;
  DeviceInfoFetcherLinux& operator=(const DeviceInfoFetcherLinux&) = delete;
  ~DeviceInfoFetcherLinux() override;

  // Overrides DeviceInfoFetcher:
  DeviceInfo Fetch() override;
};

}  // namespace enterprise_signals

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNALS_DEVICE_INFO_FETCHER_LINUX_H_
