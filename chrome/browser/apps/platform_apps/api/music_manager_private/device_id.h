// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_API_MUSIC_MANAGER_PRIVATE_DEVICE_ID_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_API_MUSIC_MANAGER_PRIVATE_DEVICE_ID_H_

#include <stddef.h>

#include <string>

#include "base/bind.h"
#include "base/task/task_traits.h"

namespace chrome_apps {
namespace api {

class DeviceId {
 public:
  typedef base::Callback<void(const std::string&)> IdCallback;

  // Calls |callback| with a unique device identifier as argument. The device
  // identifier has the following characteristics:
  // 1. It is shared across users of a device.
  // 2. It is resilient to device reboots.
  // 3. It can be reset in *some* way by the user. In Particular, it can *not*
  //    be based only on a MAC address of a physical device.
  // The specific implementation varies across platforms, some of them requiring
  // a round trip to the IO or FILE thread. "callback" will always be called on
  // the UI thread though (sometimes directly if the implementation allows
  // running on the UI thread).
  // The returned value is HMAC_SHA256(|raw_device_id|, |extension_id|), so that
  // the actual device identifier value is not exposed directly to the caller.
  static void GetDeviceId(const std::string& extension_id,
                          const IdCallback& callback);

 private:
  // Platform specific implementation of "raw" machine ID retrieval.
  static void GetRawDeviceId(const IdCallback& callback);

  // On some platforms, part of the machine ID is the MAC address. This function
  // is shared across platforms to filter out MAC addresses that have been
  // identified as invalid, i.e. not unique. For example, some VM hosts assign a
  // new MAC addresses at each reboot.
  static bool IsValidMacAddress(const void* bytes, size_t size);

  // The traits of the task that retrieves the device id.
  //
  // ThreadPool(): This should run on a background thread.
  // MayBlock(): Since this requires fetching disk.
  // TaskPriority: USER_VISIBLE. Though this might be conservative, depending
  //   on how GetDeviceId() is used.
  static constexpr base::TaskTraits traits() {
    return {base::ThreadPool(), base::MayBlock(),
            base::TaskPriority::USER_VISIBLE};
  }
};

}  // namespace api
}  // namespace chrome_apps

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_API_MUSIC_MANAGER_PRIVATE_DEVICE_ID_H_
