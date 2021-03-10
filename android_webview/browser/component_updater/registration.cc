// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/component_updater/registration.h"

namespace android_webview {

component_updater::ComponentLoaderPolicyVector GetComponentLoaderPolicies() {
  // TODO(crbug.com/1171762) register trust tokens component.
  return component_updater::ComponentLoaderPolicyVector();
}

}  // namespace android_webview
