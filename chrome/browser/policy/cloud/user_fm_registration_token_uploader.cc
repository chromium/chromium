// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_fm_registration_token_uploader.h"

#include <memory>
#include <set>
#include <string>
#include <variant>

#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/policy/cloud/fm_registration_token_uploader.h"
#include "chrome/browser/policy/policy_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/invalidation/invalidation_factory.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/remote_commands/remote_commands_constants.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#endif

namespace {

// Returns a set of all project numbers that will be used by user.
std::set<std::string> GetAllInvalidationProjectNumbers() {
  // Cannot be a static constant because project number is decided by feature,
  // which is not available during static initialization.
  return {
      std::string(policy::GetPolicyInvalidationProjectNumber(
          policy::PolicyInvalidationScope::kUser)),
      std::string(policy::GetRemoteCommandsInvalidationProjectNumber(
          policy::PolicyInvalidationScope::kUser)),
#if BUILDFLAG(IS_CHROMEOS)
      std::string(
          ash::cert_provisioning::GetCertProvisioningInvalidationProjectNumber(
              ash::cert_provisioning::CertScope::kUser))
#endif
  };
}

invalidation::ProfileInvalidationProvider* GetInvalidationProvider(
    Profile* profile) {
  return invalidation::ProfileInvalidationProviderFactory::GetForProfile(
      profile);
}

}  // namespace

namespace policy {

UserFmRegistrationTokenUploader::UserFmRegistrationTokenUploader(
    Profile* profile,
    CloudPolicyManager* policy_manager)
    : policy_manager_(policy_manager) {
  CHECK(profile);
  CHECK(policy_manager);

  // Wait for profile to be initialized and start `uploader_` on
  // `OnProfileInitializationComplete()`.
  profile_observation_.Observe(profile);
}

UserFmRegistrationTokenUploader::~UserFmRegistrationTokenUploader() = default;

void UserFmRegistrationTokenUploader::Shutdown() {
  profile_observation_.Reset();
  uploaders_.clear();
}

void UserFmRegistrationTokenUploader::OnProfileInitializationComplete(
    Profile* profile) {
  CHECK(profile_observation_.IsObservingSource(profile));
  profile_observation_.Reset();

  // Initialize now that profile creation is complete and the invalidation
  // service can safely be initialized.
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      GetInvalidationProvider(profile);
  if (!invalidation_provider) {
    VLOG(1) << "Invalidation provider does not exist.";
    return;
  }

  for (const auto& project_number : GetAllInvalidationProjectNumbers()) {
    auto invalidation_service_or_listener =
        invalidation_provider->GetInvalidationServiceOrListener(project_number);
    if (!std::holds_alternative<invalidation::InvalidationListener*>(
            invalidation_service_or_listener)) {
      continue;
    }

    uploaders_.emplace_back(std::make_unique<FmRegistrationTokenUploader>(
        PolicyInvalidationScope::kUser,
        std::get<invalidation::InvalidationListener*>(
            invalidation_service_or_listener),
        policy_manager_->core()));
  }
}

}  // namespace policy
