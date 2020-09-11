// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_DEFAULT_DEVICE_NAME_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_DEFAULT_DEVICE_NAME_H_

#include <string>

#include "base/callback.h"
#include "base/optional.h"

class Profile;

// Creates a default device name of the form <profile name>'s <device model>.
void GetNearbyShareDefaultDeviceName(
    Profile* profile,
    base::OnceCallback<void(const base::Optional<std::string>&)> callback);

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_DEFAULT_DEVICE_NAME_H_
