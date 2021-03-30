// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/installer_policies/aw_origin_trials_component_installer.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/installer_policies/origin_trials_component_installer.h"

namespace android_webview {

AwOriginTrialsComponentInstallerPolicy::
    AwOriginTrialsComponentInstallerPolicy() {
  std::vector<uint8_t> hash;
  GetHash(&hash);
  delegate_ = std::make_unique<AwComponentInstallerPolicyDelegate>(hash);
}

AwOriginTrialsComponentInstallerPolicy::
    ~AwOriginTrialsComponentInstallerPolicy() = default;

void AwOriginTrialsComponentInstallerPolicy::OnCustomUninstall() {
  delegate_->OnCustomUninstall();
}

void AwOriginTrialsComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  delegate_->ComponentReady(version, install_dir, std::move(manifest));
}

void RegisterOriginTrialsComponent(
    base::OnceCallback<bool(const update_client::CrxComponent&)>
        register_callback,
    base::OnceClosure registration_finished) {
  base::MakeRefCounted<component_updater::ComponentInstaller>(
      std::make_unique<AwOriginTrialsComponentInstallerPolicy>())
      ->Register(std::move(register_callback),
                 std::move(registration_finished));
}

}  // namespace android_webview