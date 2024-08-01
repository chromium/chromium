// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_fm_registration_token_uploader.h"

#include <memory>
#include <variant>

#include "base/functional/overloaded.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/policy/cloud/fm_registration_token_uploader.h"
#include "chrome/browser/profiles/profile.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"

namespace {

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
  uploader_.reset();
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

  auto invalidation_service_or_listener =
      invalidation_provider->GetInvalidationServiceOrListener(
          kPolicyFCMInvalidationSenderID,
          invalidation::InvalidationListener::kProjectNumberEnterprise);

  std::visit(base::Overloaded{
                 [](invalidation::InvalidationService* service) {
                   // Token uploader is not needed for the legacy invalidation
                   // stack.
                 },
                 [this](invalidation::InvalidationListener* listener) {
                   uploader_ = std::make_unique<FmRegistrationTokenUploader>(
                       PolicyInvalidationScope::kUser, listener,
                       policy_manager_->core());
                 }},
             invalidation_service_or_listener);
}

}  // namespace policy
