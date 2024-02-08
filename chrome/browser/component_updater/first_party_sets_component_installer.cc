// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/first_party_sets_component_installer.h"

#include "components/component_updater/installer_policies/first_party_sets_component_installer_policy.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/common/content_features.h"
#include "net/base/features.h"

using component_updater::ComponentUpdateService;

namespace {

base::TaskPriority GetTaskPriority() {
  // We may use USER_BLOCKING here since First-Party Set initialization can
  // block network requests at startup.
  return content::FirstPartySetsHandler::GetInstance()->IsEnabled() &&
                 base::FeatureList::IsEnabled(
                     net::features::kWaitForFirstPartySetsInit)
             ? base::TaskPriority::USER_BLOCKING
             : base::TaskPriority::BEST_EFFORT;
}

}  // namespace

namespace component_updater {

void RegisterFirstPartySetsComponent(ComponentUpdateService* cus) {
  if (!content::FirstPartySetsHandler::GetInstance()->IsEnabled()) {
    return;
  }

  VLOG(1) << "Registering Related Website Sets component.";

  auto policy = std::make_unique<FirstPartySetsComponentInstallerPolicy>(
      /*on_sets_ready=*/base::BindOnce([](base::Version version,
                                          base::File sets_file) {
        VLOG(1) << "Received Related Website Sets";
        content::FirstPartySetsHandler::GetInstance()->SetPublicFirstPartySets(
            version, std::move(sets_file));
      }),
      GetTaskPriority());

  FirstPartySetsComponentInstallerPolicy* raw_policy = policy.get();
  // Dereferencing `raw_policy` this way is safe because the closure is invoked
  // by the ComponentInstaller instance, which owns `policy` (so they have the
  // same lifetime). Therefore if/when the closure is invoked, `policy` is still
  // alive.
  base::MakeRefCounted<ComponentInstaller>(
      std::move(policy), /*action_handler=*/nullptr, GetTaskPriority())
      ->Register(cus, base::BindOnce(
                          [](FirstPartySetsComponentInstallerPolicy* policy) {
                            policy->OnRegistrationComplete();
                          },
                          raw_policy));
}

}  // namespace component_updater
