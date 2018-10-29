// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/fake_server_invalidation_service.h"

#include "components/invalidation/impl/invalidation_service_util.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/sync/base/invalidation_helper.h"

namespace fake_server {

FakeServerInvalidationService::FakeServerInvalidationService()
    : client_id_(invalidation::GenerateInvalidatorClientId()),
      self_notify_(true) {
  invalidator_registrar_.UpdateInvalidatorState(syncer::INVALIDATIONS_ENABLED);
}

FakeServerInvalidationService::~FakeServerInvalidationService() {
}

void FakeServerInvalidationService::RegisterInvalidationHandler(
      syncer::InvalidationHandler* handler) {
  invalidator_registrar_.RegisterHandler(handler);
}

bool FakeServerInvalidationService::UpdateRegisteredInvalidationIds(
      syncer::InvalidationHandler* handler,
      const syncer::ObjectIdSet& ids) {
  return invalidator_registrar_.UpdateRegisteredIds(handler, ids);
}

void FakeServerInvalidationService::UnregisterInvalidationHandler(
      syncer::InvalidationHandler* handler) {
  invalidator_registrar_.UnregisterHandler(handler);
}

syncer::InvalidatorState FakeServerInvalidationService::GetInvalidatorState()
    const {
  return invalidator_registrar_.GetInvalidatorState();
}

std::string FakeServerInvalidationService::GetInvalidatorClientId() const {
  return client_id_;
}

invalidation::InvalidationLogger*
FakeServerInvalidationService::GetInvalidationLogger() {
  return nullptr;
}

void FakeServerInvalidationService::RequestDetailedStatus(
    base::Callback<void(const base::DictionaryValue&)> caller) const {
  base::DictionaryValue value;
  caller.Run(value);
}

void FakeServerInvalidationService::EnableSelfNotifications() {
  self_notify_ = true;
}

void FakeServerInvalidationService::DisableSelfNotifications() {
  self_notify_ = false;
}

void FakeServerInvalidationService::OnCommit(
    const std::string& committer_id,
    syncer::ModelTypeSet committed_model_types) {
  syncer::ObjectIdSet object_ids = syncer::ModelTypeSetToObjectIdSet(
      committed_model_types);
  syncer::ObjectIdInvalidationMap invalidation_map;
  for (auto it = object_ids.begin(); it != object_ids.end(); ++it) {
    // TODO(pvalenzuela): Create more refined invalidations instead of
    // invalidating all items of a given type.

    if (self_notify_ || client_id_ != committer_id) {
      invalidation_map.Insert(syncer::Invalidation::InitUnknownVersion(*it));
    }
  }
  invalidator_registrar_.DispatchInvalidationsToHandlers(invalidation_map);
}

}  // namespace fake_server
