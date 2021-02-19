// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/registration.h"

#include <memory>

#include "android_webview/nonembedded/component_updater/aw_component_installer_policy_delegate.h"
#include "android_webview/nonembedded/component_updater/installer_policies/aw_trust_token_key_commitments_component_installer_policy.h"
#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "components/component_updater/component_installer.h"

namespace android_webview {

void RegisterComponentsForUpdate(
    component_updater::ComponentUpdateService* component_update_service) {
  base::MakeRefCounted<component_updater::ComponentInstaller>(
      std::make_unique<AwTrustTokenKeyCommitmentsComponentInstallerPolicy>(
          std::make_unique<AwComponentInstallerPolicyDelegate>()))
      ->Register(component_update_service, base::OnceClosure());
}

}  // namespace android_webview
