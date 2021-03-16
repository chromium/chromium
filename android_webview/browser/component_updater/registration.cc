// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/component_updater/registration.h"

#include "android_webview/browser/component_updater/origin_trials_component_loader.h"
#include "android_webview/browser/component_updater/trust_token_key_commitments_component_loader.h"

namespace android_webview {

component_updater::ComponentLoaderPolicyVector GetComponentLoaderPolicies() {
  component_updater::ComponentLoaderPolicyVector policies;
  LoadTrustTokenKeyCommitmentsComponent(policies);
  LoadOriginTrialsComponent(policies);
  return policies;
}

}  // namespace android_webview
