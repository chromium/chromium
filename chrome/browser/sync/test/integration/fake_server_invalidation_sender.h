// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_FAKE_SERVER_INVALIDATION_SENDER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_FAKE_SERVER_INVALIDATION_SENDER_H_

#include "base/macros.h"

#include "components/sync/base/model_type.h"
#include "components/sync/test/fake_server/fake_server.h"

namespace syncer {
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
      base::RepeatingCallback<syncer::FCMNetworkHandler*()>
          fcm_network_handler_getter);
  ~FakeServerInvalidationSender() override;

  // FakeServer::Observer implementation.
  void OnCommit(const std::string& committer_id,
                syncer::ModelTypeSet committed_model_types) override;

 private:
  const std::string client_id_;
  const bool self_notify_;
  const base::RepeatingCallback<syncer::FCMNetworkHandler*()>
      fcm_network_handler_getter_;

  DISALLOW_COPY_AND_ASSIGN(FakeServerInvalidationSender);
};

}  // namespace fake_server

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_FAKE_SERVER_INVALIDATION_SENDER_H_
