// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/affiliated_remote_commands_invalidator.h"

#include <memory>

#include "base/time/default_clock.h"
#include "components/policy/core/common/remote_commands/remote_commands_invalidator_impl.h"

namespace policy {

AffiliatedRemoteCommandsInvalidator::AffiliatedRemoteCommandsInvalidator(
    CloudPolicyCore* core,
    AffiliatedInvalidationServiceProvider* invalidation_service_provider,
    PolicyInvalidationScope scope)
    : core_(core),
      invalidation_service_provider_(invalidation_service_provider),
      scope_(scope) {
  invalidation_service_provider_->RegisterConsumer(this);
}

AffiliatedRemoteCommandsInvalidator::~AffiliatedRemoteCommandsInvalidator() {
  invalidation_service_provider_->UnregisterConsumer(this);
}

void AffiliatedRemoteCommandsInvalidator::OnInvalidationServiceSet(
    invalidation::InvalidationService* invalidation_service) {
  // Destroy this invalidator if it exists.
  if (invalidator_) {
    invalidator_->Shutdown();
    invalidator_.reset();
  }
  // Create a new one if required.
  if (invalidation_service) {
    invalidator_ = std::make_unique<RemoteCommandsInvalidatorImpl>(
        core_, base::DefaultClock::GetInstance(), scope_);
    invalidator_->Initialize(invalidation_service);
  }
}

}  // namespace policy
