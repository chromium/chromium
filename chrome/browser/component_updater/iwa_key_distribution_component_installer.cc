// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/iwa_key_distribution_component_installer.h"

#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/installer_policies/iwa_key_distribution_component_installer_policy.h"
#include "components/webapps/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"
#include "content/public/common/content_features.h"

namespace {

// Tells whether the component is supported on a particular platform wrt to
// the feature flags.
bool IsComponentSupported() {
#if BUILDFLAG(IS_CHROMEOS)
  return true;
#elif BUILDFLAG(IS_WIN)
  // Key Distribution component is necessary for full IWAs support as it
  // involves the IWA allowlist necessary to install IWAs in prod...
  return base::FeatureList::IsEnabled(features::kIsolatedWebApps);
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // ...however, on Mac/Linux, the component logic is not fully supported. A
  // separate flag enables developing and testing both: IWAs and the component
  // separately on these systems.
  return base::FeatureList::IsEnabled(
      component_updater::kIwaKeyDistributionComponent);
#else
  return false;
#endif
}

bool IsOnDemandUpdateSupported() {
  // `switches::kDisableComponentUpdate` is set by default in
  // browsertests.
  return IsComponentSupported() &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableComponentUpdate) &&
         g_browser_process && g_browser_process->component_updater();
}

void QueueOnDemandUpdate(
    base::PassKey<web_app::IwaKeyDistributionInfoProvider>) {
  CHECK(g_browser_process);
  CHECK(IsOnDemandUpdateSupported());

  VLOG(1) << "Queueing on-demand update for the Iwa Key Distribution Component";
  component_updater::IwaKeyDistributionComponentInstallerPolicy::
      QueueOnDemandUpdate(
          g_browser_process->component_updater()->GetOnDemandUpdater());
}

}  // namespace

namespace component_updater {

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kIwaKeyDistributionComponent, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

void RegisterIwaKeyDistributionComponent(ComponentUpdateService* cus) {
  if (!IsComponentSupported()) {
    return;
  }

  base::MakeRefCounted<ComponentInstaller>(
      IsOnDemandUpdateSupported()
          ? std::make_unique<IwaKeyDistributionComponentInstallerPolicy>(
                base::BindRepeating(&QueueOnDemandUpdate))
          : std::make_unique<IwaKeyDistributionComponentInstallerPolicy>(
                /*queue_on_demand_update=*/base::NullCallback()))
      ->Register(cus, base::DoNothing());
}

}  // namespace component_updater
