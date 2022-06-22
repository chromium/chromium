// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/invalidations/fake_server_sync_invalidation_sender.h"

#include "base/containers/contains.h"
#include "base/time/time.h"
#include "components/sync/invalidations/fcm_handler.h"

namespace fake_server {

namespace {

// This has the same value as in
// components/sync/invalidations/sync_invalidations_service_impl.cc.
const char kSyncInvalidationsAppId[] = "com.google.chrome.sync.invalidations";

}  // namespace

FakeServerSyncInvalidationSender::FakeServerSyncInvalidationSender(
    FakeServer* fake_server)
    : fake_server_(fake_server) {
  DCHECK(fake_server_);
  fake_server_->AddObserver(this);
}

FakeServerSyncInvalidationSender::~FakeServerSyncInvalidationSender() {
  fake_server_->RemoveObserver(this);

  // Unsubscribe from all the remaining FCM handlers. This is mostly the case
  // for Android platform.
  for (syncer::FCMHandler* fcm_handler : fcm_handlers_) {
    fcm_handler->RemoveTokenObserver(this);
  }
}

void FakeServerSyncInvalidationSender::AddFCMHandler(
    syncer::FCMHandler* fcm_handler) {
  DCHECK(fcm_handler);
  DCHECK(!base::Contains(fcm_handlers_, fcm_handler));

  fcm_handlers_.push_back(fcm_handler);
  fcm_handler->AddTokenObserver(this);
}

void FakeServerSyncInvalidationSender::RemoveFCMHandler(
    syncer::FCMHandler* fcm_handler) {
  DCHECK(fcm_handler);
  DCHECK(base::Contains(fcm_handlers_, fcm_handler));

  fcm_handler->RemoveTokenObserver(this);
  base::Erase(fcm_handlers_, fcm_handler);
}

void FakeServerSyncInvalidationSender::OnCommit(
    const std::string& committer_invalidator_client_id,
    syncer::ModelTypeSet committed_model_types) {
  const std::map<std::string, syncer::ModelTypeSet>
      token_to_interested_data_types_map = GetTokenToInterestedDataTypesMap();

  for (const auto& token_and_data_types : token_to_interested_data_types_map) {
    const std::string& token = token_and_data_types.first;

    // Send the invalidation only for interested types.
    const syncer::ModelTypeSet invalidated_data_types =
        Intersection(committed_model_types, token_and_data_types.second);
    if (invalidated_data_types.Empty()) {
      continue;
    }

    sync_pb::SyncInvalidationsPayload payload;
    for (const syncer::ModelType data_type : invalidated_data_types) {
      payload.add_data_type_invalidations()->set_data_type_id(
          syncer::GetSpecificsFieldNumberFromModelType(data_type));
    }

    // Versions are used to keep hints ordered. Versions are not really used by
    // tests, just use current time.
    payload.set_version(base::Time::Now().ToJavaTime());
    payload.set_hint("hint");

    invalidations_to_deliver_[token].push_back(std::move(payload));
  }

  DeliverInvalidationsToHandlers();
}

void FakeServerSyncInvalidationSender::OnFCMRegistrationTokenChanged() {
  DeliverInvalidationsToHandlers();
}

std::map<std::string, syncer::ModelTypeSet>
FakeServerSyncInvalidationSender::GetTokenToInterestedDataTypesMap() {
  std::map<std::string, syncer::ModelTypeSet> result;
  for (const sync_pb::SyncEntity& entity :
       fake_server_->GetSyncEntitiesByModelType(syncer::DEVICE_INFO)) {
    const sync_pb::InvalidationSpecificFields& invalidation_fields =
        entity.specifics().device_info().invalidation_fields();
    const std::string& token = invalidation_fields.instance_id_token();
    if (token.empty()) {
      continue;
    }

    DCHECK(!result.count(token));
    result[token] = syncer::ModelTypeSet();
    for (const int field_number :
         invalidation_fields.interested_data_type_ids()) {
      syncer::ModelType data_type =
          syncer::GetModelTypeFromSpecificsFieldNumber(field_number);
      DCHECK(syncer::IsRealDataType(data_type));
      result[token].Put(data_type);
    }
  }
  return result;
}

void FakeServerSyncInvalidationSender::DeliverInvalidationsToHandlers() {
  for (auto& token_and_invalidations : invalidations_to_deliver_) {
    const std::string& token = token_and_invalidations.first;
    // Pass a message to each FCMHandler to simulate a message from the
    // GCMDriver.
    // TODO(crbug.com/1082115): Implement reflection blocking.
    syncer::FCMHandler* fcm_handler = GetFCMHandlerByToken(token);
    if (!fcm_handler) {
      continue;
    }

    for (const sync_pb::SyncInvalidationsPayload& payload :
         token_and_invalidations.second) {
      gcm::IncomingMessage message;
      message.raw_data = payload.SerializeAsString();
      fcm_handler->OnMessage(kSyncInvalidationsAppId, message);
    }

    token_and_invalidations.second.clear();
  }
}

syncer::FCMHandler* FakeServerSyncInvalidationSender::GetFCMHandlerByToken(
    const std::string& fcm_registration_token) const {
  for (syncer::FCMHandler* fcm_handler : fcm_handlers_) {
    if (fcm_registration_token == fcm_handler->GetFCMRegistrationToken()) {
      return fcm_handler;
    }
  }
  return nullptr;
}

}  // namespace fake_server
