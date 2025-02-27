// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/probabilistic_reveal_token_component_installer.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/installer_policies/probabilistic_reveal_token_component_installer_policy.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace component_updater {

void OnProbabilisticRevealTokenRegistryReady(
    std::optional<std::string> json_content) {
  if (!json_content) {
    VLOG(1) << "Failed to receive Probabilistic Reveal Token Registry.";
    return;
  }
  VLOG(1) << "Received Probabilistic Reveal Token Registry.";

  std::optional<base::Value> json = base::JSONReader::Read(
      json_content.value(), base::JSON_ALLOW_TRAILING_COMMAS);
  if (!json.has_value()) {
    VLOG(1) << "Failed to parse Probabilistic Reveal Token Registry json.";
    return;
  }

  base::Value::Dict* json_dict = json->GetIfDict();
  if (!json_dict) {
    VLOG(1) << "Failed to get top level Probabilistic Reveal Token "
               "Registry dictionary.";
    return;
  }

  // TODO(crbug.com/399358280): Move PRT registry storage to the browser
  // process.
  content::GetNetworkService()->UpdateProbabilisticRevealTokenRegistry(
      std::move(*json_dict));
}

void RegisterProbabilisticRevealTokenComponent(
    component_updater::ComponentUpdateService* cus) {
  VLOG(1) << "Registering Probabilistic Reveal Token component.";
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<ProbabilisticRevealTokenComponentInstallerPolicy>(
          /*on_registry_ready=*/base::BindRepeating(
              OnProbabilisticRevealTokenRegistryReady)));

  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
