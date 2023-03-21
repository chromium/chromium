// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/trust_token_key_commitments_component_installer.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "components/component_updater/installer_policies/trust_token_key_commitments_component_installer_policy.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service.mojom.h"

using component_updater::ComponentUpdateService;

namespace component_updater {

void RegisterTrustTokenKeyCommitmentsComponentIfTrustTokensEnabled(
    ComponentUpdateService* cus) {
  if (!base::FeatureList::IsEnabled(network::features::kPrivateStateTokens) &&
      !base::FeatureList::IsEnabled(network::features::kFledgePst)) {
    return;
  }

  VLOG(1) << "Registering Trust Token Key Commitments component.";
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<TrustTokenKeyCommitmentsComponentInstallerPolicy>(
          /*on_commitments_ready=*/base::BindRepeating(
              [](const std::string& raw_commitments) {
                content::GetNetworkService()->SetTrustTokenKeyCommitments(
                    raw_commitments, /*callback=*/base::DoNothing());
              })));

  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
