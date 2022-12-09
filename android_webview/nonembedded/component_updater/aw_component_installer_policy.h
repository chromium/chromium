// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_INSTALLER_POLICY_H_
#define ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_INSTALLER_POLICY_H_

#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace android_webview {

// Provides an implementation for the subset of methods that need custom
// implementation for WebView. Components authors should always extend this
// class instead of `ComponentInstallerPolicy` for components registered in
// WebView.
class AwComponentInstallerPolicy
    : public component_updater::ComponentInstallerPolicy {
 public:
  AwComponentInstallerPolicy();

  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value::Dict manifest) final;
  void OnCustomUninstall() final;

 private:
  base::FilePath GetComponentsProviderServiceDirectory();

  // Virtual for testing.
  virtual void IncrementComponentsUpdatedCount();
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_INSTALLER_POLICY_H_
