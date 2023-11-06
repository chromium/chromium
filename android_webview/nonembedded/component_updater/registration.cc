// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/registration.h"

#include <memory>

#include "android_webview/common/aw_switches.h"
#include "android_webview/nonembedded/component_updater/aw_component_installer_policy_shim.h"
#include "android_webview/nonembedded/component_updater/installer_policies/aw_package_names_allowlist_component_installer_policy.h"
#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/installer_policies/origin_trials_component_installer.h"
#include "components/component_updater/installer_policies/trust_token_key_commitments_component_installer_policy.h"
#include "components/update_client/update_client.h"

namespace android_webview {

namespace {
// Number of components that are always downloaded on the default path (not
// guarded by any flags). Update when changing the components WebView registers.
constexpr int kNumWebViewComponents = 2;

void RegisterComponentInstallerPolicyShim(
    std::unique_ptr<component_updater::ComponentInstallerPolicy> policy,
    base::OnceCallback<bool(const component_updater::ComponentRegistration&)>
        register_callback,
    base::OnceClosure registration_finished) {
  base::MakeRefCounted<component_updater::ComponentInstaller>(
      std::make_unique<AwComponentInstallerPolicyShim>(std::move(policy)))
      ->Register(std::move(register_callback),
                 std::move(registration_finished));
}

}  // namespace

void RegisterComponentsForUpdate(
    base::RepeatingCallback<bool(
        const component_updater::ComponentRegistration&)> register_callback,
    base::OnceClosure on_finished) {
  int num_webview_components = kNumWebViewComponents;

  bool trust_tokens_component_enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebViewEnableTrustTokensComponent);
  if (trust_tokens_component_enabled) {
    num_webview_components++;
  }

  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(num_webview_components, std::move(on_finished));

  RegisterComponentInstallerPolicyShim(
      std::make_unique<
          component_updater::OriginTrialsComponentInstallerPolicy>(),
      register_callback, barrier_closure);

  // TODO(https://crbug.com/1170468): decide if this component is still needed.
  // Note: We're using a command-line switch because finch features isn't
  // supported in nonembedded WebView.
  if (trust_tokens_component_enabled) {
    RegisterComponentInstallerPolicyShim(
        std::make_unique<component_updater::
                             TrustTokenKeyCommitmentsComponentInstallerPolicy>(
            /* on_commitments_ready= */ base::BindRepeating(
                [](const std::string& raw_commitments) { NOTREACHED(); })),
        register_callback, barrier_closure);
  }

  RegisterWebViewAppsPackageNamesAllowlistComponent(register_callback,
                                                    barrier_closure);
}

}  // namespace android_webview
