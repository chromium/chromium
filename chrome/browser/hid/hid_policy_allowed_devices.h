// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_POLICY_ALLOWED_DEVICES_H_
#define CHROME_BROWSER_HID_HID_POLICY_ALLOWED_DEVICES_H_

class PrefRegistrySimple;

// TODO(crbug.com/1049825): Implement HidPolicyAllowedDevices
class HidPolicyAllowedDevices {
 public:
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
};

#endif  // CHROME_BROWSER_HID_HID_POLICY_ALLOWED_DEVICES_H_
