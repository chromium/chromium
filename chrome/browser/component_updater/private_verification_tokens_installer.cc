// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/private_verification_tokens_installer.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/private_verification_tokens/private_verification_tokens_service.h"
#include "chrome/browser/private_verification_tokens/private_verification_tokens_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/installer_policies/private_verification_tokens_installer_policy.h"
#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config.h"
#include "net/base/features.h"

namespace component_updater {

void RegisterPrivateVerificationTokensComponentIfEnabled(
    ComponentUpdateService* cus) {
  if (!base::FeatureList::IsEnabled(
          net::features::kEnablePrivateVerificationTokens)) {
    return;
  }

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<PrivateVerificationTokensInstallerPolicy>(
          base::BindRepeating(
              [](scoped_refptr<private_verification_tokens::
                                   PrivateVerificationTokensIssuerConfig>
                     issuer_config) {
                PrivateVerificationTokensServiceFactory::SetGlobalIssuerConfig(
                    issuer_config);
                if (g_browser_process && g_browser_process->profile_manager()) {
                  for (Profile* profile : g_browser_process->profile_manager()
                                              ->GetLoadedProfiles()) {
                    if (PrivateVerificationTokensService* service =
                            PrivateVerificationTokensServiceFactory::
                                GetForProfileIfExists(profile)) {
                      service->SetIssuerConfig(issuer_config);
                    }
                  }
                }
              })));

  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
