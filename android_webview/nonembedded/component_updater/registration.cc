// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/registration.h"

#include <memory>

#include "android_webview/common/aw_switches.h"
#include "android_webview/nonembedded/component_updater/installer_policies/aw_origin_trials_component_installer.h"
#include "android_webview/nonembedded/component_updater/installer_policies/aw_package_names_allowlist_component_installer_policy.h"
#include "android_webview/nonembedded/component_updater/installer_policies/aw_trust_token_key_commitments_component_installer_policy.h"
#include "base/barrier_closure.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"

namespace android_webview {

// Update when changing the components WebView registers.
constexpr int kNumWebViewComponents = 3;

void RegisterComponentsForUpdate(
    base::RepeatingCallback<bool(const update_client::CrxComponent&)>
        register_callback,
    base::OnceClosure on_finished) {
  // TODO(crbug.com/1174022): remove command line flag once launched.
  bool package_names_allowlist_enabled =
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebViewDisableAppsPackageNamesAllowlistComponent);
  int num_webview_components =
      package_names_allowlist_enabled ? kNumWebViewComponents : 2;

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      num_webview_components, base::BindOnce(std::move(on_finished)));

  RegisterOriginTrialsComponent(register_callback, barrier_closure);
  RegisterTrustTokensComponent(register_callback, barrier_closure);
  if (package_names_allowlist_enabled) {
    RegisterWebViewAppsPackageNamesAllowlistComponent(register_callback,
                                                      barrier_closure);
  }
}

}  // namespace android_webview
