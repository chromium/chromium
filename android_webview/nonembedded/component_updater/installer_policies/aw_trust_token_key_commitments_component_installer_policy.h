// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_INSTALLER_POLICIES_AW_TRUST_TOKEN_KEY_COMMITMENTS_COMPONENT_INSTALLER_POLICY_H_
#define ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_INSTALLER_POLICIES_AW_TRUST_TOKEN_KEY_COMMITMENTS_COMPONENT_INSTALLER_POLICY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "components/component_updater/installer_policies/trust_token_key_commitments_component_installer_policy.h"

namespace base {
class DictionaryValue;
class FilePath;
class Version;
}  // namespace base

namespace android_webview {

class AwComponentInstallerPolicyDelegate;

// Provides an implementation for the policy methods that need custom
// implementation for WebView. These methods should always be delegated to the
// custom delegate object, inherited methods shouldn't be called if they need
// a browser context for execution.
class AwTrustTokenKeyCommitmentsComponentInstallerPolicy
    : public component_updater::
          TrustTokenKeyCommitmentsComponentInstallerPolicy {
 public:
  AwTrustTokenKeyCommitmentsComponentInstallerPolicy();
  ~AwTrustTokenKeyCommitmentsComponentInstallerPolicy() override;

  AwTrustTokenKeyCommitmentsComponentInstallerPolicy(
      const AwTrustTokenKeyCommitmentsComponentInstallerPolicy&) = delete;
  AwTrustTokenKeyCommitmentsComponentInstallerPolicy& operator=(
      const AwTrustTokenKeyCommitmentsComponentInstallerPolicy&) = delete;

  void OnCustomUninstall() override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      std::unique_ptr<base::DictionaryValue> manifest) override;

 private:
  std::unique_ptr<AwComponentInstallerPolicyDelegate> delegate_;
};

// Call once during startup to make the component update service aware of
// the trust tokens update component.
void RegisterTrustTokensComponent(
    base::OnceCallback<bool(const update_client::CrxComponent&)>
        register_callback,
    base::OnceClosure registration_finished);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_INSTALLER_POLICIES_AW_TRUST_TOKEN_KEY_COMMITMENTS_COMPONENT_INSTALLER_POLICY_H_
