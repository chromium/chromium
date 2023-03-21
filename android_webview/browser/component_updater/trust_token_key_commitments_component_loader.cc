// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/component_updater/trust_token_key_commitments_component_loader.h"

#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "components/component_updater/android/loader_policies/trust_token_key_commitments_component_loader_policy.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace android_webview {

// Add trust tokens ComponentLoaderPolicy to the given policies vector, if Trust
// Tokens is enabled.
void LoadTrustTokenKeyCommitmentsComponent(
    ComponentLoaderPolicyVector& policies) {
  if (!base::FeatureList::IsEnabled(network::features::kPrivateStateTokens) &&
      !base::FeatureList::IsEnabled(network::features::kFledgePst)) {
    return;
  }

  DVLOG(1)
      << "Registering Trust Token Key Commitments component for loading in "
         "embedded WebView.";

  policies.push_back(
      std::make_unique<
          component_updater::TrustTokenKeyCommitmentsComponentLoaderPolicy>(
          /* on_commitments_ready = */ base::BindRepeating(
              [](const std::string& raw_commitments) {
                content::GetNetworkService()->SetTrustTokenKeyCommitments(
                    raw_commitments, /* done_callback = */ base::DoNothing());
              })));
}

}  // namespace android_webview
