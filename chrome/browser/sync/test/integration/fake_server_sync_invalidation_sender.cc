// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/fake_server_sync_invalidation_sender.h"

#include "components/sync/invalidations/fcm_handler.h"
#include "components/sync/protocol/sync_invalidations_payload.pb.h"

namespace fake_server {

namespace {

// This has the same value as in
// components/sync/invalidations/sync_invalidations_service_impl.cc.
const char kSyncInvalidationsAppId[] = "com.google.chrome.sync.invalidations";

}  // namespace

FakeServerSyncInvalidationSender::FakeServerSyncInvalidationSender(
    FakeServer* fake_server,
    const std::vector<syncer::FCMHandler*>& fcm_handlers)
    : fake_server_(fake_server), fcm_handlers_(fcm_handlers) {
  DCHECK(fake_server_);
  fake_server_->AddObserver(this);
}

FakeServerSyncInvalidationSender::~FakeServerSyncInvalidationSender() {
  fake_server_->RemoveObserver(this);
}

void FakeServerSyncInvalidationSender::OnCommit(
    const std::string& committer_invalidator_client_id,
    syncer::ModelTypeSet committed_model_types) {
  const std::map<std::string, syncer::ModelTypeSet>
      token_to_interested_data_types_map = GetTokenToInterestedDataTypesMap();
  // Pass a message to each FCMHandler to simulate a message from the
  // GCMDriver.
  // TODO(crbug.com/1082115): Implement reflection blocking.
  for (syncer::FCMHandler* fcm_handler : fcm_handlers_) {
    const std::string& token = fcm_handler->GetFCMRegistrationToken();
    if (!token_to_interested_data_types_map.count(token)) {
      continue;
    }

    // Send the invalidation only for interested types.
    const syncer::ModelTypeSet invalidated_data_types = Intersection(
        committed_model_types, token_to_interested_data_types_map.at(token));
    if (invalidated_data_types.Empty()) {
      continue;
    }
    sync_pb::SyncInvalidationsPayload payload;
    for (const syncer::ModelType data_type : invalidated_data_types) {
      payload.add_data_type_invalidations()->set_data_type_id(
          syncer::GetSpecificsFieldNumberFromModelType(data_type));
    }

    gcm::IncomingMessage message;
    message.data["payload"] = payload.SerializeAsString();
    fcm_handler->OnMessage(kSyncInvalidationsAppId, message);
  }
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

}  // namespace fake_server
