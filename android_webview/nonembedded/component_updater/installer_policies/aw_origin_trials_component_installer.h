// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_INSTALLER_POLICIES_AW_ORIGIN_TRIALS_COMPONENT_INSTALLER_H_
#define ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_INSTALLER_POLICIES_AW_ORIGIN_TRIALS_COMPONENT_INSTALLER_H_

#include <cstdint>
#include <memory>

#include "android_webview/nonembedded/component_updater/aw_component_installer_policy_delegate.h"
#include "components/component_updater/installer_policies/origin_trials_component_installer.h"

namespace base {
class DictionaryValue;
class FilePath;
class Version;
}  // namespace base

namespace component_updater {
class ComponentUpdateService;
}  // namespace component_updater

namespace android_webview {

// Overrides OriginTrialsComponentInstallerPolicy to provide WebView-specific
// implementations of some methods.
class AwOriginTrialsComponentInstallerPolicy
    : public component_updater::OriginTrialsComponentInstallerPolicy {
 public:
  explicit AwOriginTrialsComponentInstallerPolicy(
      std::unique_ptr<AwComponentInstallerPolicyDelegate> delegate);
  ~AwOriginTrialsComponentInstallerPolicy() override;
  AwOriginTrialsComponentInstallerPolicy(
      const AwOriginTrialsComponentInstallerPolicy&) = delete;
  AwOriginTrialsComponentInstallerPolicy& operator=(
      const AwOriginTrialsComponentInstallerPolicy&) = delete;

  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      std::unique_ptr<base::DictionaryValue> manifest) override;

 private:
  std::unique_ptr<AwComponentInstallerPolicyDelegate> delegate_;
};

// Call once during startup to make the component update service aware of
// the origin trials update component.
void RegisterOriginTrialsComponent(
    component_updater::ComponentUpdateService* update_service);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_INSTALLER_POLICIES_AW_ORIGIN_TRIALS_COMPONENT_INSTALLER_H_
