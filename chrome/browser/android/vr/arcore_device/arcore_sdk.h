// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_SDK_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_SDK_H_

#include "third_party/arcore-android-sdk/src/libraries/include/arcore_c_api.h"

/// Sets ARCore to comply with incognito mode in Google Chrome and Chromium.
/// When called before calling Resume(), it will minimize the amount of logging
/// done by ARCore for this ArSession.
///
/// @param[in] session - the ARCore session.
void ArSession_enableIncognitoMode_private(ArSession* session);

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_SDK_H_
