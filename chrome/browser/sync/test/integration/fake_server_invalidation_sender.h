// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_FAKE_SERVER_INVALIDATION_SENDER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_FAKE_SERVER_INVALIDATION_SENDER_H_

#include "components/sync/base/model_type.h"
#include "components/sync/test/fake_server.h"

namespace invalidation {
class FCMNetworkHandler;
}

namespace fake_server {

// This class is observing changes to the fake server, and sends invalidations
// to different clients upon commits. Sent invalidation follows the same format
// expected by the FCM invalidations framework.
class FakeServerInvalidationSender : public FakeServer::Observer {
 public:
  FakeServerInvalidationSender(
      const std::string& client_id,
      bool self_notify,
      base::RepeatingCallback<invalidation::FCMNetworkHandler*()>
          fcm_network_handler_getter);
  FakeServerInvalidationSender(const FakeServerInvalidationSender& other) =
      delete;
  FakeServerInvalidationSender& operator=(
      const FakeServerInvalidationSender& other) = delete;
  ~FakeServerInvalidationSender() override;

  // FakeServer::Observer implementation.
  void OnCommit(const std::string& committer_invalidator_client_id,
                syncer::ModelTypeSet committed_model_types) override;

 private:
  const std::string client_id_;
  const bool self_notify_;
  const base::RepeatingCallback<invalidation::FCMNetworkHandler*()>
      fcm_network_handler_getter_;
};

}  // namespace fake_server

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_FAKE_SERVER_INVALIDATION_SENDER_H_
