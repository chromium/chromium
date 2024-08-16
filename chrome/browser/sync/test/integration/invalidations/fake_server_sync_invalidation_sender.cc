// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/invalidations/fake_server_sync_invalidation_sender.h"

#include <vector>

#include "base/logging.h"
#include "base/time/time.h"
#include "components/gcm_driver/instance_id/fake_gcm_driver_for_instance_id.h"
#include "components/sync/base/time.h"

namespace fake_server {

FakeServerSyncInvalidationSender::FakeServerSyncInvalidationSender(
    FakeServer* fake_server)
    : fake_server_(fake_server) {
  DCHECK(fake_server_);
  fake_server_->AddObserver(this);
}

FakeServerSyncInvalidationSender::~FakeServerSyncInvalidationSender() {
  fake_server_->RemoveObserver(this);
  for (instance_id::FakeGCMDriverForInstanceID* fake_gcm_driver :
       fake_gcm_drivers_) {
    fake_gcm_driver->RemoveConnectionObserver(this);
  }
}

void FakeServerSyncInvalidationSender::AddFakeGCMDriver(
    instance_id::FakeGCMDriverForInstanceID* fake_gcm_driver) {
  // It's safe to cast since SyncTest uses FakeGCMProfileService.
  fake_gcm_drivers_.push_back(fake_gcm_driver);
  fake_gcm_driver->AddConnectionObserver(this);

  DVLOG(1) << "Added FakeGCMDriver";

  // If there were incoming invalidations, deliver them to the given GCMDriver.
  DeliverInvalidationsToHandlers();
}

void FakeServerSyncInvalidationSender::RemoveFakeGCMDriver(
    instance_id::FakeGCMDriverForInstanceID* fake_gcm_driver) {
  fake_gcm_driver->RemoveConnectionObserver(this);
  std::erase(fake_gcm_drivers_, fake_gcm_driver);
}

void FakeServerSyncInvalidationSender::OnWillCommit() {
  token_to_interested_data_types_.clear();
  UpdateTokenToInterestedDataTypesMap();
}

void FakeServerSyncInvalidationSender::OnCommit(
    syncer::DataTypeSet committed_data_types) {
  // Update token to interested data types mapping. This is needed to support
  // newly added DeviceInfos during commit request.
  UpdateTokenToInterestedDataTypesMap();
  for (const auto& token_and_data_types : token_to_interested_data_types_) {
    const std::string& token = token_and_data_types.first;

    // Send the invalidation only for interested types.
    const syncer::DataTypeSet invalidated_data_types =
        Intersection(committed_data_types, token_and_data_types.second);
    if (invalidated_data_types.empty()) {
      continue;
    }

    sync_pb::SyncInvalidationsPayload payload;
    for (const syncer::DataType data_type : invalidated_data_types) {
      payload.add_data_type_invalidations()->set_data_type_id(
          syncer::GetSpecificsFieldNumberFromDataType(data_type));
    }

    // Versions are used to keep hints ordered. Versions are not really used by
    // tests, just use current time.
    payload.set_version(base::Time::Now().InMillisecondsSinceUnixEpoch());
    payload.set_hint("hint");

    invalidations_to_deliver_[token].push_back(std::move(payload));
  }

  DeliverInvalidationsToHandlers();
}

void FakeServerSyncInvalidationSender::OnConnected(
    const net::IPEndPoint& ip_endpoint) {
  // Try to deliver invalidations once GCMDriver is connected.
  DVLOG(1) << "GCM driver connected";
  DeliverInvalidationsToHandlers();
}

void FakeServerSyncInvalidationSender::DeliverInvalidationsToHandlers() {
  DVLOG(1) << "Trying to deliver invalidations for "
           << invalidations_to_deliver_.size()
           << " FCM tokens. Known target tokens from DeviceInfo: "
           << token_to_interested_data_types_.size();
  std::set<std::string> processed_tokens;
  for (const auto& token_and_invalidations : invalidations_to_deliver_) {
    const std::string& token = token_and_invalidations.first;

    // Pass a message to GCMDriver to simulate a message from the server.
    // TODO(crbug.com/40130815): Implement reflection blocking.
    instance_id::FakeGCMDriverForInstanceID* fake_gcm_driver =
        GetFakeGCMDriverByToken(token);
    if (!fake_gcm_driver) {
      DVLOG(1) << "Could not find FakeGCMDriver for token: " << token;
      continue;
    }

    for (const sync_pb::SyncInvalidationsPayload& payload :
         token_and_invalidations.second) {
      gcm::IncomingMessage message;
      message.raw_data = payload.SerializeAsString();
      fake_gcm_driver->DispatchMessage(kSyncInvalidationsAppId, message);
    }

    processed_tokens.insert(token);
  }

  for (const std::string& token_to_remove : processed_tokens) {
    invalidations_to_deliver_.erase(token_to_remove);
  }
}

instance_id::FakeGCMDriverForInstanceID*
FakeServerSyncInvalidationSender::GetFakeGCMDriverByToken(
    const std::string& fcm_registration_token) const {
  for (instance_id::FakeGCMDriverForInstanceID* fake_gcm_driver :
       fake_gcm_drivers_) {
#if !BUILDFLAG(IS_ANDROID)
    // On Android platform FCM registration token is returned from Java
    // implementation, so HasTokenForAppId() does not contain these tokens.
    // Since Android does not support several profiles, for the simplicity just
    // check for AppHandler registration.
    if (!fake_gcm_driver->HasTokenForAppId(kSyncInvalidationsAppId,
                                           fcm_registration_token)) {
      continue;
    }
#endif  // !BUILDFLAG(IS_ANDROID)

    // AppHandler may not be registered while SyncSetup() is not called yet, the
    // server should keep invalidations to deliver them later.
    if (fake_gcm_driver->GetAppHandler(kSyncInvalidationsAppId)) {
      return fake_gcm_driver;
    }
  }
  return nullptr;
}

void FakeServerSyncInvalidationSender::UpdateTokenToInterestedDataTypesMap() {
  std::map<std::string, base::Time> token_to_mtime;
  for (const sync_pb::SyncEntity& entity :
       fake_server_->GetSyncEntitiesByDataType(syncer::DEVICE_INFO)) {
    const sync_pb::InvalidationSpecificFields& invalidation_fields =
        entity.specifics().device_info().invalidation_fields();
    const std::string& token = invalidation_fields.instance_id_token();
    if (token.empty()) {
      continue;
    }

    // If several DeviceInfos have the same FCM registration token, select the
    // latest updated one. This may happen after resetting sync engine and
    // changing cache GUID without signout.
    // TODO(crbug.com/40225423): remove once fixed.
    const base::Time last_updated = syncer::ProtoTimeToTime(
        entity.specifics().device_info().last_updated_timestamp());
    if (token_to_mtime.find(token) != token_to_mtime.end() &&
        token_to_mtime[token] >= last_updated) {
      continue;
    }

    token_to_mtime[token] = last_updated;
    token_to_interested_data_types_[token] = syncer::DataTypeSet();
    for (const int field_number :
         invalidation_fields.interested_data_type_ids()) {
      const syncer::DataType data_type =
          syncer::GetDataTypeFromSpecificsFieldNumber(field_number);
      DCHECK(syncer::IsRealDataType(data_type));
      token_to_interested_data_types_[token].Put(data_type);
    }
  }
}

}  // namespace fake_server
