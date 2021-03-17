// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/installer_policies/aw_trust_token_key_commitments_component_installer_policy.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "android_webview/nonembedded/component_updater/aw_component_installer_policy_delegate.h"
#include "base/callback.h"
#include "base/notreached.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/installer_policies/trust_token_key_commitments_component_installer_policy.h"

namespace android_webview {

AwTrustTokenKeyCommitmentsComponentInstallerPolicy::
    AwTrustTokenKeyCommitmentsComponentInstallerPolicy()
    : component_updater::TrustTokenKeyCommitmentsComponentInstallerPolicy(
          /* on_commitments_ready= */ base::BindRepeating(
              [](const std::string& raw_commitments) {
                // The inherited ComponentReady shouldn't be called because it
                // assumes it runs in a browser context.
                NOTREACHED();
              })) {
  std::vector<uint8_t> hash;
  GetHash(&hash);
  delegate_ = std::make_unique<AwComponentInstallerPolicyDelegate>(hash);
}

AwTrustTokenKeyCommitmentsComponentInstallerPolicy::
    ~AwTrustTokenKeyCommitmentsComponentInstallerPolicy() = default;

void AwTrustTokenKeyCommitmentsComponentInstallerPolicy::OnCustomUninstall() {
  delegate_->OnCustomUninstall();
}

void AwTrustTokenKeyCommitmentsComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  delegate_->ComponentReady(version, install_dir, std::move(manifest));
}

void RegisterTrustTokensComponent(
    base::OnceCallback<bool(const update_client::CrxComponent&)>
        register_callback,
    base::OnceClosure registration_finished) {
  base::MakeRefCounted<component_updater::ComponentInstaller>(
      std::make_unique<AwTrustTokenKeyCommitmentsComponentInstallerPolicy>())
      ->Register(std::move(register_callback),
                 std::move(registration_finished));
}

}  // namespace android_webview
