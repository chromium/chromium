// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/password_sender_service_factory.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/password_sharing_invitation_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/sharing/password_sender_service.h"
#include "components/password_manager/core/browser/sharing/recipient_info.h"
#include "components/sync/engine/nigori/cross_user_sharing_public_private_key_pair.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/protocol/password_sharing_invitation_specifics.pb.h"
#include "components/sync/test/fake_server_nigori_helper.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace {

using password_manager::PasswordForm;
using password_manager::PasswordRecipient;
using password_manager::PasswordSenderService;
using password_manager::PublicKey;
using testing::UnorderedElementsAre;

constexpr char kRecipientUserId[] = "recipient_user_id";
constexpr char kPasswordValue[] = "password";
constexpr char kSignonRealm[] = "signon_realm";
constexpr char kOrigin[] = "http://abc.com/";
constexpr char kUsernameElement[] = "username_element";
constexpr char kUsernameValue[] = "username";
constexpr char kPasswordElement[] = "password_element";
constexpr char kPasswordDisplayName[] = "password_display_name";
constexpr char kPasswordAvatarUrl[] = "http://avatar.url/";

constexpr uint32_t kRecipientPublicKeyVersion = 1;

PasswordForm MakePasswordForm() {
  PasswordForm password_form;
  password_form.password_value = base::UTF8ToUTF16(std::string(kPasswordValue));
  password_form.signon_realm = kSignonRealm;
  password_form.url = GURL(kOrigin);
  password_form.username_element =
      base::UTF8ToUTF16(std::string(kUsernameElement));
  password_form.username_value = base::UTF8ToUTF16(std::string(kUsernameValue));
  password_form.password_element =
      base::UTF8ToUTF16(std::string(kPasswordElement));
  password_form.display_name =
      base::UTF8ToUTF16(std::string(kPasswordDisplayName));
  password_form.icon_url = GURL(kPasswordAvatarUrl);
  return password_form;
}

class InvitationCommittedChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  explicit InvitationCommittedChecker(size_t expected_entities_count)
      : expected_entities_count_(expected_entities_count) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for outgoing password sharing invitation to be committed.";

    std::vector<sync_pb::SyncEntity> entities =
        fake_server()->GetSyncEntitiesByDataType(
            syncer::OUTGOING_PASSWORD_SHARING_INVITATION);
    *os << " Actual entities: " << entities.size()
        << ", expected: " << expected_entities_count_;
    return entities.size() == expected_entities_count_;
  }

 private:
  const size_t expected_entities_count_;
};

class SingleClientOutgoingPasswordSharingInvitationTest : public SyncTest {
 public:
  SingleClientOutgoingPasswordSharingInvitationTest()
      : SyncTest(SINGLE_CLIENT) {
  }

  PasswordSenderService* GetPasswordSenderService() {
    return PasswordSenderServiceFactory::GetForProfile(GetProfile(0));
  }

  sync_pb::CrossUserSharingPublicKey PublicKeyFromKeyPair(
      const syncer::CrossUserSharingPublicPrivateKeyPair& key_pair) {
    std::array<uint8_t, X25519_PUBLIC_VALUE_LEN> public_key =
        key_pair.GetRawPublicKey();
    sync_pb::CrossUserSharingPublicKey proto_public_key;
    proto_public_key.set_version(kRecipientPublicKeyVersion);
    proto_public_key.set_x25519_public_key(public_key.data(),
                                           public_key.size());
    return proto_public_key;
  }

  sync_pb::CrossUserSharingPublicKey GetPublicKeyFromServer() const {
    sync_pb::NigoriSpecifics nigori_specifics;
    bool success =
        fake_server::GetServerNigori(GetFakeServer(), &nigori_specifics);
    DCHECK(success);
    DCHECK(nigori_specifics.has_cross_user_sharing_public_key());
    return nigori_specifics.cross_user_sharing_public_key();
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientOutgoingPasswordSharingInvitationTest,
                       ShouldCommitSentPassword) {
  ASSERT_TRUE(SetupSync());

  PasswordRecipient recipient = {
      .user_id = kRecipientUserId,
      .public_key = PublicKey::FromProto(PublicKeyFromKeyPair(
          syncer::CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair()))};
  GetPasswordSenderService()->SendPasswords({MakePasswordForm()}, recipient);

  ASSERT_TRUE(InvitationCommittedChecker(/*expected_entities_count=*/1).Wait());
  std::vector<sync_pb::SyncEntity> entities =
      GetFakeServer()->GetSyncEntitiesByDataType(
          syncer::OUTGOING_PASSWORD_SHARING_INVITATION);
  ASSERT_EQ(1u, entities.size());

  const sync_pb::OutgoingPasswordSharingInvitationSpecifics& specifics =
      entities.front().specifics().outgoing_password_sharing_invitation();
  EXPECT_EQ(specifics.recipient_user_id(), kRecipientUserId);
  EXPECT_FALSE(specifics.guid().empty());
  EXPECT_FALSE(specifics.has_client_only_unencrypted_data());
  EXPECT_FALSE(specifics.encrypted_password_sharing_invitation_data().empty());
  EXPECT_EQ(specifics.recipient_key_version(), kRecipientPublicKeyVersion);
}

IN_PROC_BROWSER_TEST_F(SingleClientOutgoingPasswordSharingInvitationTest,
                       ShouldCommitSentPasswordGroup) {
  ASSERT_TRUE(SetupSync());

  syncer::CrossUserSharingPublicPrivateKeyPair recipient_key_pair =
      syncer::CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();
  PasswordRecipient recipient = {.user_id = kRecipientUserId,
                                 .public_key = PublicKey::FromProto(
                                     PublicKeyFromKeyPair(recipient_key_pair))};
  PasswordForm form1 = MakePasswordForm();
  PasswordForm form2 = form1;
  form2.username_element = base::UTF8ToUTF16(std::string("username_element_2"));
  GetPasswordSenderService()->SendPasswords({form1, form2}, recipient);

  ASSERT_TRUE(InvitationCommittedChecker(/*expected_entities_count=*/1).Wait());
  std::vector<sync_pb::SyncEntity> entities =
      GetFakeServer()->GetSyncEntitiesByDataType(
          syncer::OUTGOING_PASSWORD_SHARING_INVITATION);
  ASSERT_EQ(1u, entities.size());

  const sync_pb::OutgoingPasswordSharingInvitationSpecifics& specifics =
      entities.front().specifics().outgoing_password_sharing_invitation();
  EXPECT_EQ(specifics.recipient_user_id(), kRecipientUserId);
  EXPECT_FALSE(specifics.guid().empty());
  EXPECT_TRUE(base::Uuid::ParseLowercase(specifics.guid()).is_valid());
  EXPECT_FALSE(specifics.has_client_only_unencrypted_data());
  EXPECT_FALSE(specifics.encrypted_password_sharing_invitation_data().empty());
  EXPECT_EQ(specifics.recipient_key_version(), kRecipientPublicKeyVersion);

  // Verify the invitation data fields.
  sync_pb::PasswordSharingInvitationData decrypted_invitation_data =
      password_sharing_helper::DecryptInvitationData(
          specifics.encrypted_password_sharing_invitation_data(),
          GetPublicKeyFromServer(), recipient_key_pair);
  const sync_pb::PasswordSharingInvitationData::PasswordGroupData&
      password_group_data = decrypted_invitation_data.password_group_data();
  EXPECT_EQ(password_group_data.element_data_size(), 2);
  EXPECT_EQ(password_group_data.password_value(), kPasswordValue);
  EXPECT_EQ(password_group_data.username_value(), kUsernameValue);

  // Both passwords have only one different field, so check the common fields
  // first.
  for (int i = 0; i < password_group_data.element_data_size(); ++i) {
    const sync_pb::PasswordSharingInvitationData::PasswordGroupElementData&
        element_data = password_group_data.element_data(i);
    EXPECT_EQ(element_data.scheme(),
              static_cast<int>(PasswordForm::Scheme::kHtml));
    EXPECT_EQ(element_data.signon_realm(), kSignonRealm);
    EXPECT_EQ(element_data.origin(), kOrigin);
    EXPECT_EQ(element_data.password_element(), kPasswordElement);
    EXPECT_EQ(element_data.display_name(), kPasswordDisplayName);
    EXPECT_EQ(element_data.avatar_url(), kPasswordAvatarUrl);
  }

  // Check different fields.
  std::vector<std::string> username_elements = {
      password_group_data.element_data(0).username_element(),
      password_group_data.element_data(1).username_element()};
  EXPECT_THAT(username_elements,
              UnorderedElementsAre(kUsernameElement, "username_element_2"));
}

// The unconsented primary account isn't supported on ChromeOS.
// TODO(crbug.com/358053884): enable on Android once transport mode for
// Passwords is supported.
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientOutgoingPasswordSharingInvitationTest,
                       ShouldCommitSentPasswordInTransportMode) {
  // First, setup sync (in transport mode) to initialize Nigori node with a
  // cross user sharing key pair to be able to send passwords.
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // On Desktop, passwords currently require an opt-in for transport mode.
  password_manager::features_util::OptInToAccountStorage(
      GetProfile(0)->GetPrefs(), GetSyncService(0));

  PasswordRecipient recipient = {
      .user_id = kRecipientUserId,
      .public_key = PublicKey::FromProto(PublicKeyFromKeyPair(
          syncer::CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair()))};
  GetPasswordSenderService()->SendPasswords({MakePasswordForm()}, recipient);

  EXPECT_TRUE(InvitationCommittedChecker(/*expected_entities_count=*/1).Wait());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

}  // namespace
