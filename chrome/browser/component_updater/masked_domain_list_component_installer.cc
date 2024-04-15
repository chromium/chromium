// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/masked_domain_list_component_installer.h"

#include <utility>

#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/version.h"
#include "components/component_updater/installer_policies/masked_domain_list_component_installer_policy.h"
#include "content/public/browser/network_service_instance.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace component_updater {

void RegisterMaskedDomainListComponent(ComponentUpdateService* cus) {
  if (!MaskedDomainListComponentInstallerPolicy::IsEnabled()) {
    return;
  }

  VLOG(1) << "Registering Masked Domain List component.";

  auto policy = std::make_unique<MaskedDomainListComponentInstallerPolicy>(
      /*on_list_ready=*/base::BindRepeating(
          [](base::Version version,
             std::optional<mojo_base::ProtoWrapper> masked_domain_list) {
            if (masked_domain_list.has_value()) {
              VLOG(1) << "Received Masked Domain List";
              content::GetNetworkService()->UpdateMaskedDomainList(
                  std::move(masked_domain_list.value()),
                  /*exclusion_list=*/std::vector<std::string>());
            } else {
              LOG(ERROR) << "Could not read Masked Domain List file";
            }
          }));

  base::MakeRefCounted<ComponentInstaller>(std::move(policy),
                                           /*action_handler=*/nullptr,
                                           base::TaskPriority::USER_BLOCKING)
      ->Register(cus, base::OnceClosure());
}
}  // namespace component_updater
