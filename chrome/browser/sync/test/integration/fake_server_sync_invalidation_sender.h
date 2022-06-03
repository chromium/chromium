// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_FAKE_SERVER_SYNC_INVALIDATION_SENDER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_FAKE_SERVER_SYNC_INVALIDATION_SENDER_H_

#include "components/sync/base/model_type.h"
#include "components/sync/test/fake_server/fake_server.h"

namespace syncer {
class FCMHandler;
}

namespace fake_server {

// This class is observing changes to the fake server, and sends invalidations
// to clients upon commits. Sent invalidation follows the same format expected
// by the sync invalidations framework (i.e. SyncInvalidationsService).
class FakeServerSyncInvalidationSender : public FakeServer::Observer {
 public:
  // |fake_server| must not be nullptr, and must outlive this object.
  FakeServerSyncInvalidationSender(
      FakeServer* fake_server,
      const std::vector<syncer::FCMHandler*>& fcm_handlers);
  ~FakeServerSyncInvalidationSender() override;
  FakeServerSyncInvalidationSender(const FakeServerSyncInvalidationSender&) =
      delete;
  FakeServerSyncInvalidationSender& operator=(
      const FakeServerSyncInvalidationSender&) = delete;

  // FakeServer::Observer implementation.
  void OnCommit(const std::string& committer_invalidator_client_id,
                syncer::ModelTypeSet committed_model_types) override;

 private:
  std::map<std::string, syncer::ModelTypeSet>
  GetTokenToInterestedDataTypesMap();

  FakeServer* fake_server_;
  std::vector<syncer::FCMHandler*> fcm_handlers_;
};

}  // namespace fake_server

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_FAKE_SERVER_SYNC_INVALIDATION_SENDER_H_
