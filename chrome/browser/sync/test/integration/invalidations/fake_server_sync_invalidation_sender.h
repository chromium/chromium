// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_INVALIDATIONS_FAKE_SERVER_SYNC_INVALIDATION_SENDER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_INVALIDATIONS_FAKE_SERVER_SYNC_INVALIDATION_SENDER_H_

#include "base/memory/raw_ptr.h"
#include "components/sync/base/model_type.h"
#include "components/sync/invalidations/fcm_registration_token_observer.h"
#include "components/sync/protocol/sync_invalidations_payload.pb.h"
#include "components/sync/test/fake_server.h"

namespace syncer {
class FCMHandler;
}

namespace fake_server {

// This class is observing changes to the fake server, and sends invalidations
// to clients upon commits. Sent invalidation follows the same format expected
// by the sync invalidations framework (i.e. SyncInvalidationsService).
class FakeServerSyncInvalidationSender
    : public FakeServer::Observer,
      public syncer::FCMRegistrationTokenObserver {
 public:
  // |fake_server| must not be nullptr, and must outlive this object.
  explicit FakeServerSyncInvalidationSender(FakeServer* fake_server);
  ~FakeServerSyncInvalidationSender() override;
  FakeServerSyncInvalidationSender(const FakeServerSyncInvalidationSender&) =
      delete;
  FakeServerSyncInvalidationSender& operator=(
      const FakeServerSyncInvalidationSender&) = delete;

  // |fcm_handler| must not be nullptr, and must be removed using
  // RemoveFCMHandler(). If the FCM handler has registered token, all the
  // message for the token will be delivered immediately.
  void AddFCMHandler(syncer::FCMHandler* fcm_handler);

  // |fcm_handler| must not be nullptr, and must exist in |fcm_handlers_|.
  void RemoveFCMHandler(syncer::FCMHandler* fcm_handler);

  // FakeServer::Observer implementation.
  void OnWillCommit() override;
  void OnCommit(const std::string& committer_invalidator_client_id,
                syncer::ModelTypeSet committed_model_types) override;

  // syncer::FCMRegistrationTokenObserver implementation.
  void OnFCMRegistrationTokenChanged() override;

 private:
  // Returns a corresponding FCM handler having the same
  // |fcm_registration_token| if exists. Otherwise, returns nullptr.
  syncer::FCMHandler* GetFCMHandlerByToken(
      const std::string& fcm_registration_token) const;

  // Delivers all the incoming messages to the corresponding FCM handlers.
  // Messages for FCM tokens which are not registered will be kept.
  void DeliverInvalidationsToHandlers();

  // Updates |token_to_interested_data_types_map_before_commit_| from DeviceInfo
  // data type.
  void UpdateTokenToInterestedDataTypesMap();

  raw_ptr<FakeServer> fake_server_;
  std::vector<syncer::FCMHandler*> fcm_handlers_;

  // Cache of invalidations to be dispatched by
  // DeliverInvalidationsToHandlers(), keyed by FCM registration token. If no
  // handler is registered for a token, then the corresponding invalidations
  // will remain here until a handler is added.
  std::map<std::string, std::vector<sync_pb::SyncInvalidationsPayload>>
      invalidations_to_deliver_;

  // List of tokens with a list of interested data types. Used to send
  // invalidations to a corresponding FCMHandler.
  std::map<std::string, syncer::ModelTypeSet> token_to_interested_data_types_;
};

}  // namespace fake_server

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_INVALIDATIONS_FAKE_SERVER_SYNC_INVALIDATION_SENDER_H_
