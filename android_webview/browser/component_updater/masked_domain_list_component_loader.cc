// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/component_updater/masked_domain_list_component_loader.h"

#include <memory>
#include <string>
#include <vector>

#include "android_webview/browser/aw_ip_protection_proxy_bypass_info.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "components/component_updater/android/loader_policies/masked_domain_list_component_loader_policy.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace {
void UpdateMaskedDomainList(const std::string& raw_mdl,
                            const std::vector<std::string>& exclusion_list) {
  content::GetNetworkService()->UpdateMaskedDomainList(raw_mdl, exclusion_list);
}
}  // namespace

namespace android_webview {

// Add MaskedDomainListComponentLoaderPolicy to the given policies vector, if
// the component is enabled.
void LoadMaskedDomainListComponent(ComponentLoaderPolicyVector& policies) {
  if (!base::FeatureList::IsEnabled(network::features::kMaskedDomainList)) {
    return;
  }

  DVLOG(1) << "Registering Masked Domain List component for loading in "
              "embedded WebView.";

  policies.push_back(std::make_unique<
                     component_updater::MaskedDomainListComponentLoaderPolicy>(
      /* on_list_ready=*/base::BindRepeating(
          [](base::Version version,
             const absl::optional<std::string>& raw_mdl) {
            if (raw_mdl.has_value()) {
              base::ThreadPool::PostTaskAndReplyWithResult(
                  FROM_HERE,
                  {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
                  base::BindOnce(&android_webview::LoadExclusionList),
                  base::BindOnce(&UpdateMaskedDomainList, raw_mdl.value()));
            } else {
              LOG(ERROR) << "Could not read Masked Domain List file";
            }
          })));
}

}  // namespace android_webview
