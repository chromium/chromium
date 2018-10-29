// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_SHIM_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_SHIM_H_

namespace vr {

// TODO(vollick): add support for unloading the SDK.
bool LoadArCoreSdk(const std::string& libraryPath);

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_SHIM_H_
