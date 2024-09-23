// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_PASSWORD_SHARING_INVITATION_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_PASSWORD_SHARING_INVITATION_HELPER_H_

#include <string>

#include "components/sync/engine/nigori/cross_user_sharing_public_private_key_pair.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/protocol/password_sharing_invitation_specifics.pb.h"

namespace password_sharing_helper {

// Decrypts the invitation data sent to the recipient.
sync_pb::PasswordSharingInvitationData DecryptInvitationData(
    const std::string& encrypted_data,
    const sync_pb::CrossUserSharingPublicKey& sender_public_key,
    const syncer::CrossUserSharingPublicPrivateKeyPair& recipient_key_pair);

// Creates an incoming password sharing invitation specifics including
// encrypting the `invitation_data`.
sync_pb::IncomingPasswordSharingInvitationSpecifics
CreateEncryptedIncomingInvitationSpecifics(
    const sync_pb::PasswordSharingInvitationData& invitation_data,
    const sync_pb::UserDisplayInfo& sender_display_info,
    const sync_pb::CrossUserSharingPublicKey& recipient_public_key,
    const syncer::CrossUserSharingPublicPrivateKeyPair& sender_key_pair);

// Creates incoming password sharing invitation data with the given password
// value.
sync_pb::PasswordSharingInvitationData CreateDefaultIncomingInvitation(
    const std::string& username_value,
    const std::string& password_value);

// Creates default UserDisplayInfo for sender.
sync_pb::UserDisplayInfo CreateDefaultSenderDisplayInfo();

}  // namespace password_sharing_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_PASSWORD_SHARING_INVITATION_HELPER_H_
