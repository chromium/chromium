// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/registration.h"

#include <vector>

#include "android_webview/common/aw_switches.h"
#include "android_webview/nonembedded/component_updater/aw_component_installer_policy_shim.h"
#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/installer_policies/masked_domain_list_component_installer_policy.h"
#include "components/component_updater/installer_policies/origin_trials_component_installer.h"
#include "components/component_updater/installer_policies/trust_token_key_commitments_component_installer_policy.h"
#include "components/update_client/update_client.h"

namespace android_webview {

void RegisterComponentsForUpdate(
    base::RepeatingCallback<bool(
        const component_updater::ComponentRegistration&)> register_callback,
    base::OnceClosure on_finished) {
  // Set of non-AW components that are always downloaded on the default path
  // (not guarded by any flags). Update when changing the non-AW components
  // WebView registers. Note: 'non-AW' refers to classes that do not contain
  // AwComponentInstallerPolicy as a parent class
  std::vector<std::unique_ptr<component_updater::ComponentInstallerPolicy>>
      component_installer_list;

  component_installer_list.push_back(
      std::make_unique<
          component_updater::OriginTrialsComponentInstallerPolicy>());
  component_installer_list.push_back(
      std::make_unique<
          component_updater::MaskedDomainListComponentInstallerPolicy>(
          /*on_list_ready=*/base::BindRepeating(
              [](base::Version version,
                 const absl::optional<std::string>& raw_mdl) {
                if (raw_mdl.has_value()) {
                  VLOG(1) << "Received Masked Domain List version " << version;
                } else {
                  LOG(ERROR) << "Could not read Masked Domain List file";
                }
              })));

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebViewEnableTrustTokensComponent)) {
    // TODO(https://crbug.com/1170468): decide if this component is still
    // needed. Note: We're using a command-line switch because finch features
    // isn't supported in nonembedded WebView.
    component_installer_list.push_back(
        std::make_unique<component_updater::
                             TrustTokenKeyCommitmentsComponentInstallerPolicy>(
            /* on_commitments_ready= */ base::BindRepeating(
                [](const std::string& raw_commitments) { NOTREACHED(); })));
  }

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      component_installer_list.size(), std::move(on_finished));
  for (auto& component : component_installer_list) {
    base::MakeRefCounted<component_updater::ComponentInstaller>(
        std::make_unique<AwComponentInstallerPolicyShim>(std::move(component)))
        ->Register(base::OnceCallback<bool(
                       const component_updater::ComponentRegistration&)>(
                       register_callback),
                   base::OnceClosure(barrier_closure));
  }
}

}  // namespace android_webview
