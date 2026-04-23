// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/groups/enterprise_groups_handler_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/enterprise/browser/groups/enterprise_groups_handler.h"
#include "components/enterprise/browser/groups/groups_features.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"

namespace enterprise_groups {

// static
EnterpriseGroupsProfileHandlerFactory*
EnterpriseGroupsProfileHandlerFactory::GetInstance() {
  static base::NoDestructor<EnterpriseGroupsProfileHandlerFactory> instance;
  return instance.get();
}

// static
policy::EnterpriseGroupsProfileHandler*
EnterpriseGroupsProfileHandlerFactory::GetForProfile(Profile* profile) {
  return static_cast<policy::EnterpriseGroupsProfileHandler*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

EnterpriseGroupsProfileHandlerFactory::EnterpriseGroupsProfileHandlerFactory()
    : ProfileKeyedServiceFactory("EnterpriseGroupsProfileHandler",
                                 ProfileSelections::BuildForRegularProfile()) {}

EnterpriseGroupsProfileHandlerFactory::
    ~EnterpriseGroupsProfileHandlerFactory() = default;

std::unique_ptr<KeyedService>
EnterpriseGroupsProfileHandlerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(kEnterpriseGroupsExperiments)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile->GetCloudPolicyManager()) {
    return nullptr;
  }
  return std::make_unique<policy::EnterpriseGroupsProfileHandler>(
      profile->GetCloudPolicyManager()->core(),
      g_browser_process->local_state(),
      // Profile paths are guaranteed to be UTF8-encoded.
      profile->GetPath().BaseName().AsUTF8Unsafe());
}

}  // namespace enterprise_groups
