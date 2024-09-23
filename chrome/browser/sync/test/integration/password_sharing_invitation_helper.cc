// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/password_sharing_invitation_helper.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/uuid.h"
#include "components/sync/engine/nigori/cross_user_sharing_public_key.h"
#include "components/sync/nigori/cryptographer_impl.h"

namespace password_sharing_helper {

namespace {
constexpr char kSignonRealm[] = "signon_realm";
constexpr char kOrigin[] = "http://abc.com/";
constexpr char kUsernameElement[] = "username_element";
constexpr char kPasswordElement[] = "password_element";
constexpr char kPasswordDisplayName[] = "password_display_name";
constexpr char kPasswordAvatarUrl[] = "http://avatar.url/";

constexpr char kSenderEmail[] = "sender@gmail.com";
constexpr char kSenderDisplayName[] = "Sender Name";
constexpr char kSenderProfileImageUrl[] = "http://sender.url/image";

constexpr int kDefaultKeyVersion = 0;

sync_pb::CrossUserSharingPublicKey PublicKeyToProto(
    const syncer::CrossUserSharingPublicKey& public_key) {
  sync_pb::CrossUserSharingPublicKey proto;
  auto raw_public_key = public_key.GetRawPublicKey();
  proto.set_x25519_public_key(
      std::string(raw_public_key.begin(), raw_public_key.end()));
  proto.set_version(kDefaultKeyVersion);
  return proto;
}

std::unique_ptr<syncer::CryptographerImpl> InitializeCryptographer(
    const syncer::CrossUserSharingPublicPrivateKeyPair& key_pair) {
  std::unique_ptr<syncer::CryptographerImpl> cryptographer =
      syncer::CryptographerImpl::CreateEmpty();

  // Clone `key_pair` since the cryptographer requires it to be moved.
  std::optional<syncer::CrossUserSharingPublicPrivateKeyPair> key_pair_copy =
      syncer::CrossUserSharingPublicPrivateKeyPair::CreateByImport(
          key_pair.GetRawPrivateKey());
  CHECK(key_pair_copy);
  cryptographer->SetKeyPair(std::move(key_pair_copy.value()),
                            kDefaultKeyVersion);
  cryptographer->SelectDefaultCrossUserSharingKey(kDefaultKeyVersion);

  return cryptographer;
}

// Encrypts the invitation data to simulate the sending client.
std::vector<uint8_t> EncryptInvitationData(
    const sync_pb::PasswordSharingInvitationData& unencrypted_password_data,
    const sync_pb::CrossUserSharingPublicKey& recipient_public_key,
    const syncer::CrossUserSharingPublicPrivateKeyPair& sender_key_pair) {
  std::unique_ptr<syncer::CryptographerImpl> sender_cryptographer =
      InitializeCryptographer(sender_key_pair);

  std::string serialized_data;
  bool success = unencrypted_password_data.SerializeToString(&serialized_data);
  CHECK(success);

  std::optional<std::vector<uint8_t>> result =
      sender_cryptographer->AuthEncryptForCrossUserSharing(
          base::as_bytes(base::make_span(serialized_data)),
          base::as_bytes(
              base::make_span(recipient_public_key.x25519_public_key())));
  CHECK(result);

  return result.value();
}
}  // namespace

sync_pb::PasswordSharingInvitationData DecryptInvitationData(
    const std::string& encrypted_data,
    const sync_pb::CrossUserSharingPublicKey& sender_public_key,
    const syncer::CrossUserSharingPublicPrivateKeyPair& recipient_key_pair) {
  std::unique_ptr<syncer::CryptographerImpl> recipient_cryptographer =
      InitializeCryptographer(recipient_key_pair);

  std::optional<std::vector<uint8_t>> decrypted_data =
      recipient_cryptographer->AuthDecryptForCrossUserSharing(
          base::as_byte_span(encrypted_data),
          base::as_byte_span(sender_public_key.x25519_public_key()),
          kDefaultKeyVersion);
  CHECK(decrypted_data);

  sync_pb::PasswordSharingInvitationData invitation_data;
  CHECK(invitation_data.ParseFromArray(decrypted_data->data(),
                                       decrypted_data->size()));
  return invitation_data;
}

sync_pb::IncomingPasswordSharingInvitationSpecifics
CreateEncryptedIncomingInvitationSpecifics(
    const sync_pb::PasswordSharingInvitationData& invitation_data,
    const sync_pb::UserDisplayInfo& sender_display_info,
    const sync_pb::CrossUserSharingPublicKey& recipient_public_key,
    const syncer::CrossUserSharingPublicPrivateKeyPair& sender_key_pair) {
  sync_pb::IncomingPasswordSharingInvitationSpecifics specifics;
  std::vector<uint8_t> encrypted_password = EncryptInvitationData(
      invitation_data, recipient_public_key, sender_key_pair);
  specifics.set_encrypted_password_sharing_invitation_data(
      std::string(encrypted_password.begin(), encrypted_password.end()));
  specifics.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  specifics.set_recipient_key_version(recipient_public_key.version());

  std::optional<syncer::CrossUserSharingPublicKey> sender_public_key =
      syncer::CrossUserSharingPublicKey::CreateByImport(
          sender_key_pair.GetRawPublicKey());
  CHECK(sender_public_key);

  sync_pb::UserInfo* sender_info = specifics.mutable_sender_info();
  sender_info->mutable_cross_user_sharing_public_key()->CopyFrom(
      PublicKeyToProto(sender_public_key.value()));
  sender_info->mutable_user_display_info()->CopyFrom(sender_display_info);

  return specifics;
}

sync_pb::PasswordSharingInvitationData CreateDefaultIncomingInvitation(
    const std::string& username_value,
    const std::string& password_value) {
  sync_pb::PasswordSharingInvitationData password_invitation_data;
  sync_pb::PasswordSharingInvitationData::PasswordGroupData*
      password_group_data =
          password_invitation_data.mutable_password_group_data();

  password_group_data->set_username_value(username_value);
  password_group_data->set_password_value(password_value);

  sync_pb::PasswordSharingInvitationData::PasswordGroupElementData*
      element_data = password_group_data->add_element_data();

  element_data->set_signon_realm(kSignonRealm);
  element_data->set_origin(kOrigin);
  element_data->set_username_element(kUsernameElement);
  element_data->set_password_element(kPasswordElement);
  element_data->set_display_name(kPasswordDisplayName);
  element_data->set_avatar_url(kPasswordAvatarUrl);

  return password_invitation_data;
}

sync_pb::UserDisplayInfo CreateDefaultSenderDisplayInfo() {
  sync_pb::UserDisplayInfo sender_display_info;
  sender_display_info.set_email(kSenderEmail);
  sender_display_info.set_display_name(kSenderDisplayName);
  sender_display_info.set_profile_image_url(kSenderProfileImageUrl);
  return sender_display_info;
}

}  // namespace password_sharing_helper
