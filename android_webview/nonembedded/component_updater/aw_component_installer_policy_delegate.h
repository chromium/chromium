// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_INSTALLER_POLICY_DELEGATE_H_
#define ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_INSTALLER_POLICY_DELEGATE_H_

#include <stdint.h>

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
  // `hash` is the raw byte SHA256 public key hash of the component.
  explicit AwComponentInstallerPolicyDelegate(const std::vector<uint8_t>& hash);

  // Virtual for testing.
  virtual ~AwComponentInstallerPolicyDelegate();

  AwComponentInstallerPolicyDelegate(
      const AwComponentInstallerPolicyDelegate&) = delete;
  AwComponentInstallerPolicyDelegate& operator=(
      const AwComponentInstallerPolicyDelegate&) = delete;

  // These methods should match the methods in ComponentInstallerPolicy
  void OnCustomUninstall();
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      std::unique_ptr<base::DictionaryValue> manifest);

 private:
  base::FilePath GetComponentsProviderServiceDirectory();

  // Virtual for testing.
  virtual void IncrementComponentsUpdatedCount();

  const std::string component_id_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_INSTALLER_POLICY_DELEGATE_H_
