// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_COMPONENT_UPDATER_REGISTRATION_H_
#define ANDROID_WEBVIEW_BROWSER_COMPONENT_UPDATER_REGISTRATION_H_

#include "components/component_updater/android/component_loader_policy.h"

namespace android_webview {

// ComponentLoaderPolicies for component to load in an embedded WebView during
// startup.
component_updater::ComponentLoaderPolicyVector GetComponentLoaderPolicies();

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_COMPONENT_UPDATER_REGISTRATION_H_