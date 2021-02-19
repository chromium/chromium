// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_INSTALLER_POLICY_DELEGATE_H_
#define ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_INSTALLER_POLICY_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "components/update_client/update_client.h"

namespace base {
class DictionaryValue;
class FilePath;
class Version;
}  // namespace base

namespace android_webview {

// Provides an implementation for the subset of methods that need custom
// implementation for WebView. Components authors should always call methods
// from this delegate in their custom WebView ComponentInstallerPolicy,
// otherwise their components won't be installed as expected in WebView.
class AwComponentInstallerPolicyDelegate {
 public:
  AwComponentInstallerPolicyDelegate();
  ~AwComponentInstallerPolicyDelegate();

  AwComponentInstallerPolicyDelegate(
      const AwComponentInstallerPolicyDelegate&) = delete;
  AwComponentInstallerPolicyDelegate& operator=(
      const AwComponentInstallerPolicyDelegate&) = delete;

  // These methods should match the methods in ComponentInstallerPolicy
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir,
      const std::vector<uint8_t>& hash);
  void OnCustomUninstall();
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      std::unique_ptr<base::DictionaryValue> manifest);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_INSTALLER_POLICY_DELEGATE_H_
