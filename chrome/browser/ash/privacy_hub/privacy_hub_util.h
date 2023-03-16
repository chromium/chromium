// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRIVACY_HUB_PRIVACY_HUB_UTIL_H_
#define CHROME_BROWSER_ASH_PRIVACY_HUB_PRIVACY_HUB_UTIL_H_

#include "ash/public/cpp/privacy_hub_delegate.h"

namespace ash {

class PrivacyHubDelegate;

namespace privacy_hub_util {

// Sets a given frontend handler withing the controller.
void SetFrontend(PrivacyHubDelegate* ptr);

// Returns the current switch state of the microphone.
bool MicrophoneSwitchState();

// Needs to be called for the Privacy Hub to be aware of the camera count.
void SetUpCameraCountObserver();

}  // namespace privacy_hub_util

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRIVACY_HUB_PRIVACY_HUB_CONTROLLER_PROXY_H_
