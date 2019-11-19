// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/fake_server_invalidation_sender.h"

#include "chrome/browser/profiles/profile.h"
#include "components/invalidation/impl/fcm_network_handler.h"

namespace fake_server {

namespace {

const char kInvalidationsFCMAppId[] = "com.google.chrome.fcm.invalidations";

}  // namespace

FakeServerInvalidationSender::FakeServerInvalidationSender(
    const std::string& client_id,
    bool self_notify,
    base::RepeatingCallback<syncer::FCMNetworkHandler*()>
        fcm_network_handler_getter)
    : client_id_(client_id),
      self_notify_(self_notify),
      fcm_network_handler_getter_(fcm_network_handler_getter) {}

FakeServerInvalidationSender::~FakeServerInvalidationSender() {}

void FakeServerInvalidationSender::OnCommit(
    const std::string& committer_id,
    syncer::ModelTypeSet committed_model_types) {
  if (!self_notify_ && client_id_ == committer_id) {
    return;
  }
  syncer::FCMNetworkHandler* fcm_network_handler =
      fcm_network_handler_getter_.Run();
  // If there is no FCM network handler registered for this profile, there is
  // nothing to do. This could be the case during test Setup phase because the
  // FCM network handlers get assigned in SetupInvalidations() which happens
  // after SetupSync().
  if (fcm_network_handler == nullptr) {
    DLOG(WARNING) << "Received invalidations for the following data types in "
                     "invalidation sender "
                  << this << " will be dropped:"
                  << ModelTypeSetToString(committed_model_types);
    return;
  }
  // For each of the committed model types, pass a message to the FCM Network
  // Handler to simulate a message from the GCMDriver.
  for (syncer::ModelType type : committed_model_types) {
    std::string notification_type;
    bool result = RealModelTypeToNotificationType(type, &notification_type);
    // We shouldn't ever get commits for non-protocol types.
    DCHECK(result);

    gcm::IncomingMessage message;
    // Client doesn't parse the payload.
    message.data["payload"] = "any_payload";
    // version doesn't matter, it's not used in the client.
    message.data["version"] = "1234567890";
    // The public topic name should be stored in the external name field.
    message.data["external_name"] = notification_type;
    // The private topic name is stored in the sender_id field.
    message.sender_id =
        "/topics/private/" + notification_type + "-topic_server_user_id";
    fcm_network_handler->OnMessage(kInvalidationsFCMAppId, message);
  }
}

}  // namespace fake_server
