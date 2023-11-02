// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/registration.h"

#include <memory>

#include "android_webview/common/aw_switches.h"
#include "android_webview/nonembedded/component_updater/aw_component_installer_policy_shim.h"
#include "android_webview/nonembedded/component_updater/installer_policies/aw_package_names_allowlist_component_installer_policy.h"
#include "base/barrier_closure.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/installer_policies/client_side_phishing_component_installer_policy.h"
#include "components/component_updater/installer_policies/origin_trials_component_installer.h"
#include "components/component_updater/installer_policies/trust_token_key_commitments_component_installer_policy.h"
#include "components/update_client/update_client.h"

namespace android_webview {

namespace {
// Update when changing the components WebView registers.
constexpr int kNumWebViewComponents = 4;

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
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(kNumWebViewComponents, std::move(on_finished));

  RegisterComponentInstallerPolicyShim(
      std::make_unique<
          component_updater::OriginTrialsComponentInstallerPolicy>(),
      register_callback, barrier_closure);

  RegisterComponentInstallerPolicyShim(
      std::make_unique<
          component_updater::TrustTokenKeyCommitmentsComponentInstallerPolicy>(
          /* on_commitments_ready= */ base::BindRepeating(
              [](const std::string& raw_commitments) { NOTREACHED(); })),
      register_callback, barrier_closure);

  RegisterComponentInstallerPolicyShim(
      std::make_unique<
          component_updater::ClientSidePhishingComponentInstallerPolicy>(
          // Files shouldn't be parsed or loaded in this process, thus
          // ClientSidePhishingComponentInstallerPolicy::ComponentReady will
          // never be called in this process and the `ReadFilesCallback`
          // shouldn't be called either.
          base::BindRepeating(
              [](const base::FilePath& /* install_path */) { NOTREACHED(); }),
          base::BindRepeating([]() {
            // Always download the "default" binary, because variations aren't
            // initialized in this process and values can't be dynamically
            // changed using finch. See https://crbug.com/1115700#c36.
            return update_client::InstallerAttributes{{"tag", "default"}};
          })),
      register_callback, barrier_closure);

  RegisterWebViewAppsPackageNamesAllowlistComponent(register_callback,
                                                    barrier_closure);
}

}  // namespace android_webview
