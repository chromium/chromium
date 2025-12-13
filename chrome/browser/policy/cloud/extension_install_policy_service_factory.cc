// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/extension_install_policy_service_factory.h"

#include "chrome/browser/policy/cloud/extension_install_policy_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "content/public/browser/browser_context.h"
#include "components/policy/core/common/features.h"

namespace policy {

// static
ExtensionInstallPolicyService*
ExtensionInstallPolicyServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionInstallPolicyService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExtensionInstallPolicyServiceFactory*
ExtensionInstallPolicyServiceFactory::GetInstance() {
  static base::NoDestructor<ExtensionInstallPolicyServiceFactory> instance;
  return instance.get();
}

ExtensionInstallPolicyServiceFactory::ExtensionInstallPolicyServiceFactory()
    : ProfileKeyedServiceFactory(
          "ExtensionInstallPolicyService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

ExtensionInstallPolicyServiceFactory::~ExtensionInstallPolicyServiceFactory() =
    default;

std::unique_ptr<KeyedService>
ExtensionInstallPolicyServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          features::kEnableExtensionInstallPolicyFetching)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<ExtensionInstallPolicyService>(profile);
}

}  // namespace policy
