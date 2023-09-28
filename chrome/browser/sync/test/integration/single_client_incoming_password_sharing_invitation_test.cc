// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/cycle/entity_change_metric_recording.h"
#include "components/sync/engine/nigori/cross_user_sharing_public_key.h"
#include "components/sync/engine/nigori/cross_user_sharing_public_private_key_pair.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/protocol/password_sharing_invitation_specifics.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/test/fake_server_nigori_helper.h"
#include "content/public/test/browser_test.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using password_manager::PasswordForm;
using password_manager::PasswordStoreInterface;
using passwords_helper::GetAccountPasswordStoreInterface;
using passwords_helper::GetAllLogins;
using passwords_helper::GetProfilePasswordStoreInterface;
using sync_pb::EntitySpecifics;
using sync_pb::IncomingPasswordSharingInvitationSpecifics;
using sync_pb::PasswordSharingInvitationData;
using sync_pb::SyncEntity;
using syncer::SyncServiceImpl;
using testing::AllOf;
using testing::Contains;
using testing::Field;
using testing::IsEmpty;
using testing::SizeIs;

namespace {

constexpr char kPasswordValue[] = "password";
constexpr char kSignonRealm[] = "signon_realm";
constexpr char kOrigin[] = "http://abc.com/";
constexpr char kUsernameElement[] = "username_element";
constexpr char kUsernameValue[] = "username";
constexpr char kPasswordElement[] = "password_element";
constexpr char kPasswordDisplayName[] = "password_display_name";
constexpr char kPasswordAvatarUrl[] = "http://avatar.url/";

constexpr char kSenderEmail[] = "sender@gmail.com";
constexpr char kSenderDisplayName[] = "Sender Name";
constexpr char kSenderProfileImageUrl[] = "http://sender.url/image";

constexpr char kLocalPasswordValue[] = "local_password_value";

constexpr uint32_t kSenderKeyVersion = 1;

sync_pb::CrossUserSharingPublicKey PublicKeyToProto(
    const syncer::CrossUserSharingPublicKey& public_key) {
  sync_pb::CrossUserSharingPublicKey proto;
  auto raw_public_key = public_key.GetRawPublicKey();
  proto.set_x25519_public_key(
      std::string(raw_public_key.begin(), raw_public_key.end()));
  proto.set_version(kSenderKeyVersion);
  return proto;
}

PasswordSharingInvitationData CreateUnencryptedInvitationData() {
  PasswordSharingInvitationData password_invitation_data;
  PasswordSharingInvitationData::PasswordData* password_data =
      password_invitation_data.mutable_password_data();

  password_data->set_password_value(kPasswordValue);
  password_data->set_signon_realm(kSignonRealm);
  password_data->set_origin(kOrigin);
  password_data->set_username_element(kUsernameElement);
  password_data->set_username_value(kUsernameValue);
  password_data->set_password_element(kPasswordElement);
  password_data->set_display_name(kPasswordDisplayName);
  password_data->set_avatar_url(kPasswordAvatarUrl);

  return password_invitation_data;
}

std::vector<uint8_t> EncryptInvitationData(
    const PasswordSharingInvitationData& unencrypted_password_data,
    const sync_pb::CrossUserSharingPublicKey& recipient_public_key,
    const syncer::CrossUserSharingPublicPrivateKeyPair& sender_key_pair) {
  std::unique_ptr<syncer::CryptographerImpl> sender_cryptographer =
      syncer::CryptographerImpl::CreateEmpty();

  // Clone `sender_key_pair` since the cryptographer requires it to be moved.
  absl::optional<syncer::CrossUserSharingPublicPrivateKeyPair>
      sender_key_pair_copy =
          syncer::CrossUserSharingPublicPrivateKeyPair::CreateByImport(
              sender_key_pair.GetRawPrivateKey());
  DCHECK(sender_key_pair_copy);
  sender_cryptographer->EmplaceKeyPair(std::move(sender_key_pair_copy.value()),
                                       kSenderKeyVersion);
  sender_cryptographer->SelectDefaultCrossUserSharingKey(kSenderKeyVersion);

  std::string serialized_data;
  bool success = unencrypted_password_data.SerializeToString(&serialized_data);
  DCHECK(success);

  absl::optional<std::vector<uint8_t>> result =
      sender_cryptographer->AuthEncryptForCrossUserSharing(
          base::as_bytes(base::make_span(serialized_data)),
          base::as_bytes(
              base::make_span(recipient_public_key.x25519_public_key())));
  DCHECK(result);

  return result.value();
}

IncomingPasswordSharingInvitationSpecifics CreateInvitationSpecifics(
    const sync_pb::CrossUserSharingPublicKey& recipient_public_key) {
  syncer::CrossUserSharingPublicPrivateKeyPair sender_key_pair =
      syncer::CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();
  std::vector<uint8_t> encrypted_password = EncryptInvitationData(
      CreateUnencryptedInvitationData(), recipient_public_key, sender_key_pair);
  IncomingPasswordSharingInvitationSpecifics specifics;
  specifics.set_encrypted_password_sharing_invitation_data(
      std::string(encrypted_password.begin(), encrypted_password.end()));
  specifics.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  specifics.set_recipient_key_version(recipient_public_key.version());

  absl::optional<syncer::CrossUserSharingPublicKey> sender_public_key =
      syncer::CrossUserSharingPublicKey::CreateByImport(
          sender_key_pair.GetRawPublicKey());
  DCHECK(sender_public_key);
  sync_pb::UserInfo* sender_info = specifics.mutable_sender_info();
  sender_info->mutable_cross_user_sharing_public_key()->CopyFrom(
      PublicKeyToProto(sender_public_key.value()));
  sender_info->mutable_user_display_info()->set_email(kSenderEmail);
  sender_info->mutable_user_display_info()->set_display_name(
      kSenderDisplayName);
  sender_info->mutable_user_display_info()->set_profile_image_url(
      kSenderProfileImageUrl);

  return specifics;
}

// Waits for the incoming password to be stored locally.
class PasswordStoredChecker : public SingleClientStatusChangeChecker {
 public:
  PasswordStoredChecker(SyncServiceImpl* sync_service,
                        PasswordStoreInterface* password_store,
                        size_t expected_count)
      : SingleClientStatusChangeChecker(sync_service),
        password_store_(password_store),
        expected_count_(expected_count) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for " << expected_count_ << " passwords in the store. ";

    size_t current_count = GetAllLogins(password_store_).size();
    *os << "Current password count in the store: " << current_count;
    return current_count == expected_count_;
  }

 private:
  const raw_ptr<PasswordStoreInterface> password_store_;
  const size_t expected_count_;
};

// Waits until all the incoming invitations are deleted from the fake server.
class ServerPasswordInvitationChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  explicit ServerPasswordInvitationChecker(size_t expected_count)
      : expected_count_(expected_count) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for incoming invitation entity count on the server: "
        << expected_count_ << ". ";

    size_t actual_count = fake_server()
                              ->GetSyncEntitiesByModelType(
                                  syncer::INCOMING_PASSWORD_SHARING_INVITATION)
                              .size();
    *os << "Actual count: " << actual_count;

    return actual_count == expected_count_;
  }

 private:
  const size_t expected_count_;
};

class SingleClientIncomingPasswordSharingInvitationTest : public SyncTest {
 public:
  SingleClientIncomingPasswordSharingInvitationTest()
      : SyncTest(SINGLE_CLIENT) {
    override_features_.InitWithFeatures(
        /*enabled_features=*/
        {password_manager::features::kPasswordManagerEnableReceiverService,
         syncer::kSharingOfferKeyPairBootstrap},
        /*disabled_features=*/{});
  }

  sync_pb::CrossUserSharingPublicKey GetPublicKeyFromServer() const {
    sync_pb::NigoriSpecifics nigori_specifics;
    bool success =
        fake_server::GetServerNigori(GetFakeServer(), &nigori_specifics);
    DCHECK(success);
    DCHECK(nigori_specifics.has_cross_user_sharing_public_key());
    return nigori_specifics.cross_user_sharing_public_key();
  }

  void InjectInvitationToServer() {
    EntitySpecifics specifics;
    specifics.mutable_incoming_password_sharing_invitation()->CopyFrom(
        CreateInvitationSpecifics(GetPublicKeyFromServer()));
    GetFakeServer()->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/"",
            /*client_tag=*/
            specifics.incoming_password_sharing_invitation().guid(), specifics,
            /*creation_time=*/0, /*last_modified_time=*/0));
  }

  void InjectTestPasswordToFakeServer() {
    sync_pb::PasswordSpecificsData password_data;
    // Used for computing the client tag.
    password_data.set_origin(kOrigin);
    password_data.set_username_element(kUsernameElement);
    password_data.set_username_value(kUsernameValue);
    password_data.set_password_element(kPasswordElement);
    password_data.set_signon_realm(kSignonRealm);
    // Other data.
    password_data.set_password_value(kLocalPasswordValue);

    passwords_helper::InjectKeystoreEncryptedServerPassword(password_data,
                                                            GetFakeServer());
  }

 private:
  base::test::ScopedFeatureList override_features_;
};

IN_PROC_BROWSER_TEST_F(SingleClientIncomingPasswordSharingInvitationTest,
                       ShouldStoreIncomingPassword) {
  ASSERT_TRUE(SetupSync());
  InjectInvitationToServer();
  EXPECT_TRUE(PasswordStoredChecker(GetSyncService(0),
                                    GetProfilePasswordStoreInterface(0),
                                    /*expected_count=*/1)
                  .Wait());
  std::vector<std::unique_ptr<PasswordForm>> all_logins =
      GetAllLogins(GetProfilePasswordStoreInterface(0));
  ASSERT_EQ(1u, all_logins.size());
  const PasswordForm& password_form = *all_logins.front();
  EXPECT_EQ(password_form.signon_realm, kSignonRealm);
  EXPECT_EQ(password_form.url.spec(), kOrigin);
  EXPECT_EQ(base::UTF16ToUTF8(password_form.username_element),
            kUsernameElement);
  EXPECT_EQ(base::UTF16ToUTF8(password_form.username_value), kUsernameValue);
  EXPECT_EQ(base::UTF16ToUTF8(password_form.password_element),
            kPasswordElement);
  EXPECT_EQ(base::UTF16ToUTF8(password_form.password_value), kPasswordValue);
  EXPECT_EQ(base::UTF16ToUTF8(password_form.display_name),
            kPasswordDisplayName);
  // TODO(crbug.com/1468523): check the remaining fields including sender
  // profile image.
  // EXPECT_EQ(password_form.icon_url.spec(), kPasswordAvatarUrl);
  EXPECT_EQ(base::UTF16ToUTF8(password_form.sender_email), kSenderEmail);
  EXPECT_EQ(base::UTF16ToUTF8(password_form.sender_name), kSenderDisplayName);
}

IN_PROC_BROWSER_TEST_F(SingleClientIncomingPasswordSharingInvitationTest,
                       ShouldIssueTombstoneAfterProcessingInvitation) {
  ASSERT_TRUE(SetupSync());

  InjectInvitationToServer();

  // Wait the invitation to be processed and the password stored.
  ASSERT_TRUE(PasswordStoredChecker(GetSyncService(0),
                                    GetProfilePasswordStoreInterface(0),
                                    /*expected_count=*/1)
                  .Wait());

  // Check that all the invitations are eventually deleted from the server.
  // PasswordStoredChecker above guarantees that there is an invitation present
  // on the server (which was injected earlier).
  EXPECT_TRUE(ServerPasswordInvitationChecker(/*expected_count=*/0).Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientIncomingPasswordSharingInvitationTest,
                       ShouldHandleIncomingInvitationsAtInitialSync) {
  // First, setup sync to initialize Nigori node with a public key to be able to
  // inject invitations.
  ASSERT_TRUE(SetupSync());

  // Then stop sync service, inject an invitation to the server, and re-enable
  // sync again.
  GetClient(0)->StopSyncServiceAndClearData();
  InjectInvitationToServer();
  ASSERT_TRUE(GetClient(0)->EnableSyncFeature());

  // Wait the invitation to be processed and the password stored.
  ASSERT_TRUE(PasswordStoredChecker(GetSyncService(0),
                                    GetProfilePasswordStoreInterface(0),
                                    /*expected_count=*/1)
                  .Wait());
  EXPECT_THAT(GetAllLogins(GetProfilePasswordStoreInterface(0)),
              Contains(Pointee(
                  AllOf(Field(&PasswordForm::password_value,
                              base::UTF8ToUTF16(std::string(kPasswordValue))),
                        Field(&PasswordForm::type,
                              PasswordForm::Type::kReceivedViaSharing)))));

  // Check that all the invitations are deleted from the server.
  EXPECT_TRUE(ServerPasswordInvitationChecker(/*expected_count=*/0).Wait());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientIncomingPasswordSharingInvitationTest,
    ShouldIgnoreIncomingInvitationIfPasswordExistsAtInitialSync) {
  base::HistogramTester histogram_tester;

  // First, setup sync to initialize Nigori node with a public key to be able to
  // inject invitations.
  ASSERT_TRUE(SetupSync());

  // Then stop sync service, inject an invitation and a different password
  // (but having the same client tag to cause a collision) to the server, and
  // re-enable sync again.
  GetClient(0)->StopSyncServiceAndClearData();
  InjectTestPasswordToFakeServer();
  InjectInvitationToServer();
  ASSERT_TRUE(GetClient(0)->EnableSyncFeature());

  // Wait the password to be stored.
  ASSERT_TRUE(PasswordStoredChecker(GetSyncService(0),
                                    GetProfilePasswordStoreInterface(0),
                                    /*expected_count=*/1)
                  .Wait());

  // Verify that the invitation has been processed and a tombstone has been
  // uploaded.
  ASSERT_TRUE(ServerPasswordInvitationChecker(/*expected_count=*/0).Wait());

  // The invitation should be ignored because the same password already exists.
  EXPECT_THAT(
      GetAllLogins(GetProfilePasswordStoreInterface(0)),
      Contains(Pointee(AllOf(
          Field(&PasswordForm::password_value,
                base::UTF8ToUTF16(std::string(kLocalPasswordValue))),
          Field(&PasswordForm::type, PasswordForm::Type::kFormSubmission)))));

  // Double check that the invitation has not been processed and stored locally
  // before the remote password has been stored. The following histogram is
  // reported if the remote password exists locally during the initial sync
  // merge.
  // TODO(crbug.com/1478704): add a new histogram to record when an invitation
  // is ignored because of existing local password.
  histogram_tester.ExpectTotalCount(
      "PasswordManager.MergeSyncData.UpdateLoginSyncError",
      /*expected_count=*/0);
}

// The unconsented primary account isn't supported on ChromeOS.
// TODO(crbug.com/1348950): enable on Android once transport mode for Passwords
// is supported.
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientIncomingPasswordSharingInvitationTest,
                       ShouldStoreIncomingPasswordIntoAccountDB) {
  // First, setup sync (in transport mode) to initialize Nigori node with a
  // public key to be able to inject invitations.
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(CrossUserSharingKeysChecker().Wait());

  // Let the user opt in to the account-scoped password storage, and wait for it
  // to become active.
  password_manager::features_util::OptInToAccountStorage(
      GetProfile(0)->GetPrefs(), GetSyncService(0));
  PasswordSyncActiveChecker(GetSyncService(0)).Wait();
  ASSERT_THAT(GetAllLogins(GetAccountPasswordStoreInterface(0)), IsEmpty());

  InjectInvitationToServer();
  EXPECT_TRUE(PasswordStoredChecker(GetSyncService(0),
                                    GetAccountPasswordStoreInterface(0),
                                    /*expected_count=*/1)
                  .Wait());
  EXPECT_TRUE(ServerPasswordInvitationChecker(/*expected_count=*/0).Wait());

  EXPECT_THAT(GetAllLogins(GetProfilePasswordStoreInterface(0)), IsEmpty());
  EXPECT_THAT(GetAllLogins(GetAccountPasswordStoreInterface(0)),
              Contains(Pointee(
                  AllOf(Field(&PasswordForm::password_value,
                              base::UTF8ToUTF16(std::string(kPasswordValue))),
                        Field(&PasswordForm::type,
                              PasswordForm::Type::kReceivedViaSharing)))));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

}  // namespace
