// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_DEFAULT_DEVICE_NAME_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_DEFAULT_DEVICE_NAME_H_

#include <string>

class Profile;

// Creates a default device name of the form "<given name>'s <device type>." For
// example, "Josh's Chromebook." If a given name cannot be found, returns just
// the device type.
std::string GetNearbyShareDefaultDeviceName(Profile* profile);

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_DEFAULT_DEVICE_NAME_H_
