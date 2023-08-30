// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/password_manager/password_sender_service_factory.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/sharing/password_sender_service.h"
#include "components/password_manager/core/browser/sharing/recipient_info.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/nigori/cross_user_sharing_public_private_key_pair.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "content/public/test/browser_test.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

using password_manager::PasswordForm;
using password_manager::PasswordRecipient;
using password_manager::PasswordSenderService;
using password_manager::PublicKey;

namespace {

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
        fake_server()->GetSyncEntitiesByModelType(
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
    override_features_.InitWithFeatures(
        /*enabled_features=*/
        {password_manager::features::kPasswordManagerEnableSenderService,
         syncer::kSharingOfferKeyPairBootstrap},
        /*disabled_features=*/{});
  }

  PasswordSenderService* GetPasswordSenderService() {
    return PasswordSenderServiceFactory::GetForProfile(GetProfile(0));
  }

  PublicKey GenerateRecipientPublicKey() {
    sync_pb::CrossUserSharingPublicKey proto_public_key;
    proto_public_key.set_version(kRecipientPublicKeyVersion);
    std::array<uint8_t, X25519_PUBLIC_VALUE_LEN> public_key =
        syncer::CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair()
            .GetRawPublicKey();
    proto_public_key.set_x25519_public_key(public_key.data(),
                                           public_key.size());
    return PublicKey::FromProto(proto_public_key);
  }

 private:
  base::test::ScopedFeatureList override_features_;
};

IN_PROC_BROWSER_TEST_F(SingleClientOutgoingPasswordSharingInvitationTest,
                       SanityCheck) {
  ASSERT_TRUE(SetupSync());
}

IN_PROC_BROWSER_TEST_F(SingleClientOutgoingPasswordSharingInvitationTest,
                       ShouldCommitSentPassword) {
  ASSERT_TRUE(SetupSync());

  PasswordRecipient recipient = {.user_id = kRecipientUserId,
                                 .public_key = GenerateRecipientPublicKey()};
  GetPasswordSenderService()->SendPasswords({MakePasswordForm()}, recipient);

  ASSERT_TRUE(InvitationCommittedChecker(/*expected_entities_count=*/1).Wait());
  std::vector<sync_pb::SyncEntity> entities =
      GetFakeServer()->GetSyncEntitiesByModelType(
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

}  // namespace
