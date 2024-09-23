// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_INVALIDATIONS_FAKE_SERVER_SYNC_INVALIDATION_SENDER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_INVALIDATIONS_FAKE_SERVER_SYNC_INVALIDATION_SENDER_H_

#include "base/memory/raw_ptr.h"
#include "components/gcm_driver/gcm_connection_observer.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/sync_invalidations_payload.pb.h"
#include "components/sync/test/fake_server.h"

namespace instance_id {
class FakeGCMDriverForInstanceID;
}  // namespace instance_id

namespace fake_server {

// This class is observing changes to the fake server, and sends invalidations
// to clients upon commits. Sent invalidation follows the same format expected
// by the sync invalidations framework (i.e. SyncInvalidationsService).
class FakeServerSyncInvalidationSender : public FakeServer::Observer,
                                         public gcm::GCMConnectionObserver {
 public:
  // This has the same value as in
  // components/sync/invalidations/sync_invalidations_service_impl.cc.
  static constexpr char kSyncInvalidationsAppId[] =
      "com.google.chrome.sync.invalidations";

  // |fake_server| must not be nullptr, and must outlive this object.
  explicit FakeServerSyncInvalidationSender(FakeServer* fake_server);
  ~FakeServerSyncInvalidationSender() override;
  FakeServerSyncInvalidationSender(const FakeServerSyncInvalidationSender&) =
      delete;
  FakeServerSyncInvalidationSender& operator=(
      const FakeServerSyncInvalidationSender&) = delete;

  // Add |fake_gcm_driver| to send invalidations to from the fake server.
  void AddFakeGCMDriver(
      instance_id::FakeGCMDriverForInstanceID* fake_gcm_driver);

  // Remove |fake_gcm_driver| to stop sending invalidations.
  void RemoveFakeGCMDriver(
      instance_id::FakeGCMDriverForInstanceID* fake_gcm_driver);

  // FakeServer::Observer implementation.
  void OnWillCommit() override;
  void OnCommit(syncer::DataTypeSet committed_data_types) override;

  // gcm::GCMConnectionObserver implementation.
  void OnConnected(const net::IPEndPoint& ip_endpoint) override;

 private:
  instance_id::FakeGCMDriverForInstanceID* GetFakeGCMDriverByToken(
      const std::string& fcm_registration_token) const;

  // Delivers all the incoming messages to the corresponding FCM handlers.
  // Messages for FCM tokens which are not registered will be kept.
  void DeliverInvalidationsToHandlers();

  // Updates |token_to_interested_data_types_map_before_commit_| from DeviceInfo
  // data type.
  void UpdateTokenToInterestedDataTypesMap();

  const raw_ptr<FakeServer> fake_server_;

  // Cache of invalidations to be dispatched by
  // DeliverInvalidationsToHandlers(), keyed by FCM registration token. If no
  // handler is registered for a token, then the corresponding invalidations
  // will remain here until a handler is added.
  std::map<std::string, std::vector<sync_pb::SyncInvalidationsPayload>>
      invalidations_to_deliver_;

  // List of tokens with a list of interested data types. Used to send
  // invalidations to a corresponding client.
  std::map<std::string, syncer::DataTypeSet> token_to_interested_data_types_;

  std::vector<raw_ptr<instance_id::FakeGCMDriverForInstanceID>>
      fake_gcm_drivers_;
};

}  // namespace fake_server

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_INVALIDATIONS_FAKE_SERVER_SYNC_INVALIDATION_SENDER_H_
