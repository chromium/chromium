// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/invalidation/affiliated_cloud_policy_invalidator.h"

#include <memory>

#include "base/check.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"

namespace policy {

AffiliatedCloudPolicyInvalidator::AffiliatedCloudPolicyInvalidator(
    PolicyInvalidationScope scope,
    CloudPolicyCore* core,
    AffiliatedInvalidationServiceProvider* invalidation_service_provider)
    : AffiliatedCloudPolicyInvalidator(scope,
                                       core,
                                       invalidation_service_provider,
                                       /*device_local_account_id=*/"") {}

AffiliatedCloudPolicyInvalidator::AffiliatedCloudPolicyInvalidator(
    PolicyInvalidationScope scope,
    CloudPolicyCore* core,
    AffiliatedInvalidationServiceProvider* invalidation_service_provider,
    const std::string& device_local_account_id)
    : scope_(scope),
      device_local_account_id_(device_local_account_id),
      core_(core),
      invalidation_service_provider_(invalidation_service_provider),
      highest_handled_invalidation_version_(0) {
  invalidation_service_provider_->RegisterConsumer(this);
}

AffiliatedCloudPolicyInvalidator::~AffiliatedCloudPolicyInvalidator() {
  DestroyInvalidator();
  invalidation_service_provider_->UnregisterConsumer(this);
}

void AffiliatedCloudPolicyInvalidator::OnInvalidationServiceSet(
    invalidation::InvalidationService* invalidation_service) {
  DestroyInvalidator();
  if (invalidation_service)
    CreateInvalidator(invalidation_service);
}

CloudPolicyInvalidator*
AffiliatedCloudPolicyInvalidator::GetInvalidatorForTest() const {
  return invalidator_.get();
}

void AffiliatedCloudPolicyInvalidator::CreateInvalidator(
    invalidation::InvalidationService* invalidation_service) {
  DCHECK(!invalidator_);
  invalidator_ = std::make_unique<CloudPolicyInvalidator>(
      scope_, core_, base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::DefaultClock::GetInstance(), highest_handled_invalidation_version_,
      device_local_account_id_);
  invalidator_->Initialize(invalidation_service);
}

void AffiliatedCloudPolicyInvalidator::DestroyInvalidator() {
  if (!invalidator_)
    return;

  highest_handled_invalidation_version_ =
      invalidator_->highest_handled_invalidation_version();
  invalidator_->Shutdown();
  invalidator_.reset();
}

}  // namespace policy
