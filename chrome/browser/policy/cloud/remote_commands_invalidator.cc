// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/remote_commands_invalidator.h"

#include <string>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/syslog_logging.h"
#include "chrome/browser/policy/cloud/policy_invalidation_util.h"
#include "chrome/common/chrome_features.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/invalidation/public/single_object_invalidation_set.h"

namespace policy {

RemoteCommandsInvalidator::RemoteCommandsInvalidator() {}

RemoteCommandsInvalidator::~RemoteCommandsInvalidator() {
  DCHECK_EQ(SHUT_DOWN, state_);
}

void RemoteCommandsInvalidator::Initialize(
    invalidation::InvalidationService* invalidation_service) {
  DCHECK_EQ(SHUT_DOWN, state_);
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(hunyadym): Remove after crbug.com/582506 is fixed.
  SYSLOG(INFO) << "Initialize RemoteCommandsInvalidator.";

  DCHECK(invalidation_service);
  invalidation_service_ = invalidation_service;

  state_ = STOPPED;
  OnInitialize();
}

void RemoteCommandsInvalidator::Shutdown() {
  DCHECK_NE(SHUT_DOWN, state_);
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(hunyadym): Remove after crbug.com/582506 is fixed.
  SYSLOG(INFO) << "Shutdown RemoteCommandsInvalidator.";

  Stop();

  state_ = SHUT_DOWN;
  OnShutdown();
}

void RemoteCommandsInvalidator::Start() {
  DCHECK_EQ(STOPPED, state_);
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(hunyadym): Remove after crbug.com/582506 is fixed.
  SYSLOG(INFO) << "Starting RemoteCommandsInvalidator.";

  state_ = STARTED;

  OnStart();
}

void RemoteCommandsInvalidator::Stop() {
  DCHECK_NE(SHUT_DOWN, state_);
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(hunyadym): Remove after crbug.com/582506 is fixed.
  SYSLOG(INFO) << "Stopping RemoteCommandsInvalidator.";

  if (state_ == STARTED) {
    Unregister();
    state_ = STOPPED;

    OnStop();
  }
}

void RemoteCommandsInvalidator::OnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  DCHECK_EQ(STARTED, state_);
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(hunyadym): Remove after crbug.com/582506 is fixed.
  SYSLOG(INFO) << "RemoteCommandsInvalidator state changed: " << state;

  invalidation_service_enabled_ = state == syncer::INVALIDATIONS_ENABLED;
  UpdateInvalidationsEnabled();
}

void RemoteCommandsInvalidator::OnIncomingInvalidation(
    const syncer::ObjectIdInvalidationMap& invalidation_map) {
  DCHECK_EQ(STARTED, state_);
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(hunyadym): Remove after crbug.com/582506 is fixed.
  SYSLOG(INFO) << "RemoteCommandsInvalidator received invalidation.";

  if (!invalidation_service_enabled_)
    LOG(WARNING) << "Unexpected invalidation received.";

  const syncer::SingleObjectInvalidationSet& list =
      invalidation_map.ForObject(object_id_);
  if (list.IsEmpty()) {
    NOTREACHED();
    return;
  }

  // Acknowledge all invalidations.
  for (const auto& it : list)
    it.Acknowledge();
  // TODO(hunyadym): Remove after crbug.com/582506 is fixed.
  SYSLOG(INFO) << "Invalidations acknowledged.";

  DoRemoteCommandsFetch();
}

std::string RemoteCommandsInvalidator::GetOwnerName() const {
  return "RemoteCommands";
}

bool RemoteCommandsInvalidator::IsPublicTopic(
    const syncer::Topic& topic) const {
  return IsPublicInvalidationTopic(topic);
}

void RemoteCommandsInvalidator::ReloadPolicyData(
    const enterprise_management::PolicyData* policy) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(hunyadym): Remove after crbug.com/582506 is fixed.
  SYSLOG(INFO) << "RemoteCommandsInvalidator ReloadPolicyData.";

  if (state_ != STARTED)
    return;

  // Create the ObjectId based on the policy data.
  // If the policy does not specify an the ObjectId, then unregister.
  invalidation::ObjectId object_id;
  if (!policy || !GetRemoteCommandObjectIdFromPolicy(*policy, &object_id)) {
    Unregister();
    return;
  }

  // If the policy object id in the policy data is different from the currently
  // registered object id, update the object registration.
  if (!is_registered_ || !(object_id == object_id_))
    Register(object_id);
}

void RemoteCommandsInvalidator::Register(
    const invalidation::ObjectId& object_id) {
  // TODO(hunyadym): Remove after crbug.com/582506 is fixed.
  SYSLOG(INFO) << "Register RemoteCommandsInvalidator.";

  // Register this handler with the invalidation service if needed.
  if (!is_registered_) {
    OnInvalidatorStateChange(invalidation_service_->GetInvalidatorState());
    invalidation_service_->RegisterInvalidationHandler(this);
    is_registered_ = true;
  }

  object_id_ = object_id;
  UpdateInvalidationsEnabled();

  // Update registration with the invalidation service.
  syncer::ObjectIdSet ids;
  ids.insert(object_id);
  CHECK(invalidation_service_->UpdateRegisteredInvalidationIds(this, ids));
}

void RemoteCommandsInvalidator::Unregister() {
  // TODO(hunyadym): Remove after crbug.com/582506 is fixed.
  SYSLOG(INFO) << "Unregister RemoteCommandsInvalidator.";
  if (is_registered_) {
    CHECK(invalidation_service_->UpdateRegisteredInvalidationIds(
        this, syncer::ObjectIdSet()));
    invalidation_service_->UnregisterInvalidationHandler(this);
    is_registered_ = false;
    UpdateInvalidationsEnabled();
  }
}

void RemoteCommandsInvalidator::UpdateInvalidationsEnabled() {
  invalidations_enabled_ = invalidation_service_enabled_ && is_registered_;
}

}  // namespace policy
