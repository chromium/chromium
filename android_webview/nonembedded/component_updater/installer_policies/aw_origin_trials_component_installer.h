// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_INSTALLER_POLICIES_AW_ORIGIN_TRIALS_COMPONENT_INSTALLER_H_
#define ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_INSTALLER_POLICIES_AW_ORIGIN_TRIALS_COMPONENT_INSTALLER_H_

#include <cstdint>
#include <memory>

#include "android_webview/nonembedded/component_updater/aw_component_installer_policy_delegate.h"
#include "base/callback_forward.h"
#include "components/component_updater/installer_policies/origin_trials_component_installer.h"

namespace base {
class DictionaryValue;
class FilePath;
class Version;
}  // namespace base

namespace android_webview {

// Overrides OriginTrialsComponentInstallerPolicy to provide WebView-specific
// implementations of some methods.
class AwOriginTrialsComponentInstallerPolicy
    : public component_updater::OriginTrialsComponentInstallerPolicy {
 public:
  AwOriginTrialsComponentInstallerPolicy();
  ~AwOriginTrialsComponentInstallerPolicy() override;
  AwOriginTrialsComponentInstallerPolicy(
      const AwOriginTrialsComponentInstallerPolicy&) = delete;
  AwOriginTrialsComponentInstallerPolicy& operator=(
      const AwOriginTrialsComponentInstallerPolicy&) = delete;

  void OnCustomUninstall() override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      std::unique_ptr<base::DictionaryValue> manifest) override;

 private:
  std::unique_ptr<AwComponentInstallerPolicyDelegate> delegate_;
};

// Call once during startup to register the origin trials update component.
void RegisterOriginTrialsComponent(
    base::OnceCallback<bool(const update_client::CrxComponent&)>
        register_callback,
    base::OnceClosure registration_finished);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_INSTALLER_POLICIES_AW_ORIGIN_TRIALS_COMPONENT_INSTALLER_H_
