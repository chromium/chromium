// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/component_updater/origin_trials_component_loader.h"

#include <memory>

#include "android_webview/browser/component_updater/loader_policies/origin_trials_component_loader_policy.h"
#include "components/component_updater/android/component_loader_policy.h"
#include "components/component_updater/android/component_loader_policy_forward.h"

namespace android_webview {

void LoadOriginTrialsComponent(
    component_updater::ComponentLoaderPolicyVector& policies) {
  policies.push_back(std::make_unique<OriginTrialsComponentLoaderPolicy>());
}

}  // namespace android_webview
