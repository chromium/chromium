// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_cloud_policy_invalidator.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"
#include "chrome/browser/policy/policy_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/features.h"
#include "extensions/buildflags/buildflags.h"

namespace {

invalidation::ProfileInvalidationProvider* GetInvalidationProvider(
    Profile* profile) {
  return invalidation::ProfileInvalidationProviderFactory::GetForProfile(
      profile);
}

}  // namespace

namespace policy {

UserCloudPolicyInvalidator::UserCloudPolicyInvalidator(
    Profile* profile,
    CloudPolicyManager* policy_manager)
    : policy_manager_(policy_manager) {
  DCHECK(profile);

  // Register for notification that profile creation is complete. The
  // invalidator must not be initialized before then because the invalidation
  // service cannot be started because it depends on components initialized
  // after this object is instantiated.
  // TODO(stepco): Delayed initialization can be removed once the request
  // context can be accessed during profile-keyed service creation. Tracked by
  // bug 286209.
  // TODO(crbug.com/40113187): Investigate if this is still required.
  profile_observation_.Observe(profile);
}

UserCloudPolicyInvalidator::~UserCloudPolicyInvalidator() = default;

void UserCloudPolicyInvalidator::Shutdown() {
  profile_observation_.Reset();
  if (invalidator_) {
    invalidator_->Shutdown();
    invalidator_.reset();
  }
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (extension_install_invalidator_) {
    extension_install_invalidator_->Shutdown();
    extension_install_invalidator_.reset();
  }
#endif
}

void UserCloudPolicyInvalidator::OnProfileInitializationComplete(
    Profile* profile) {
  DCHECK(profile_observation_.IsObservingSource(profile));
  profile_observation_.Reset();

  // Initialize now that profile creation is complete and the invalidation
  // service can safely be initialized.
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      GetInvalidationProvider(profile);
  if (!invalidation_provider) {
    return;
  }

  invalidator_ = std::make_unique<CloudPolicyInvalidator>(
      PolicyInvalidationScope::kUser,
      invalidation_provider->GetInvalidationListener(
          policy::kPolicyInvalidationProjectNumber),
      policy_manager_->core(),
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::DefaultClock::GetInstance());
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (base::FeatureList::IsEnabled(
          policy::features::kEnableExtensionInstallPolicyFetching)) {
    extension_install_invalidator_ =
        std::make_unique<ExtensionInstallPolicyInvalidator>(
            PolicyInvalidationScope::kUser,
            invalidation_provider->GetInvalidationListener(
                policy::kPolicyInvalidationProjectNumber),
            policy_manager_->core(),
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            base::DefaultClock::GetInstance());
  }
#endif
}

}  // namespace policy
