// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/password_sharing_invitation_helper.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/nigori/cross_user_sharing_public_key.h"
#include "components/sync/engine/nigori/cross_user_sharing_public_private_key_pair.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/protocol/password_sharing_invitation_specifics.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/test/fake_server_nigori_helper.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

using password_manager::PasswordForm;
using password_manager::PasswordStoreInterface;
using password_manager::metrics_util::
    ProcessIncomingPasswordSharingInvitationResult;
using password_sharing_helper::CreateDefaultIncomingInvitation;
using password_sharing_helper::CreateDefaultSenderDisplayInfo;
using password_sharing_helper::CreateEncryptedIncomingInvitationSpecifics;
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
using testing::Pointee;
using testing::SizeIs;
using testing::UnorderedElementsAre;

namespace {

constexpr char kPasswordValue[] = "password";
constexpr char kUsernameValue[] = "username";

MATCHER_P(HasPasswordValue, password_value, "") {
  return base::UTF16ToUTF8(arg.password_value) == password_value;
}

MATCHER_P(HasUsernameElement, username_element, "") {
  return base::UTF16ToUTF8(arg.username_element) == username_element;
}

IncomingPasswordSharingInvitationSpecifics CreateInvitationSpecifics(
    const sync_pb::CrossUserSharingPublicKey& recipient_public_key) {
  return CreateEncryptedIncomingInvitationSpecifics(
      CreateDefaultIncomingInvitation(kUsernameValue, kPasswordValue),
      CreateDefaultSenderDisplayInfo(), recipient_public_key,
      /*sender_key_pair=*/
      syncer::CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair());
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Fill in fields in `password_data` required for computing password client tag.
// This is useful for testing collisions. Note that the `invitation` must
// contain only one password in a group.
void FillPasswordClientTagFromInvitation(
    const sync_pb::PasswordSharingInvitationData& invitation_data,
    sync_pb::PasswordSpecificsData* password_data) {
  const sync_pb::PasswordSharingInvitationData::PasswordGroupData&
      invitation_group_data = invitation_data.password_group_data();
  CHECK_EQ(invitation_group_data.element_data().size(), 1);
  const sync_pb::PasswordSharingInvitationData::PasswordGroupElementData&
      invitation_group_element_data = invitation_group_data.element_data(0);

  password_data->set_username_value(invitation_group_data.username_value());
  password_data->set_origin(invitation_group_element_data.origin());
  password_data->set_username_element(
      invitation_group_element_data.username_element());
  password_data->set_password_element(
      invitation_group_element_data.password_element());
  password_data->set_signon_realm(invitation_group_element_data.signon_realm());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

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
                              ->GetSyncEntitiesByDataType(
                                  syncer::INCOMING_PASSWORD_SHARING_INVITATION)
                              .size();
    *os << "Actual count: " << actual_count;

    return actual_count == expected_count_;
  }

 private:
  const size_t expected_count_;
};

// Waits for the Incoming Password Sharing Invitation data type to become
// inactive.
class IncomingPasswordSharingInvitationInactiveChecker
    : public SingleClientStatusChangeChecker {
 public:
  explicit IncomingPasswordSharingInvitationInactiveChecker(
      syncer::SyncServiceImpl* service)
      : SingleClientStatusChangeChecker(service) {}

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for Incoming Password Sharing Invitations to become "
           "inactive.";

    return !service()->GetActiveDataTypes().Has(
        syncer::INCOMING_PASSWORD_SHARING_INVITATION);
  }
};

class SingleClientIncomingPasswordSharingInvitationTest : public SyncTest {
 public:
  SingleClientIncomingPasswordSharingInvitationTest()
      : SyncTest(SINGLE_CLIENT) {
  }

  sync_pb::CrossUserSharingPublicKey GetPublicKeyFromServer() const {
    sync_pb::NigoriSpecifics nigori_specifics;
    bool success =
        fake_server::GetServerNigori(GetFakeServer(), &nigori_specifics);
    DCHECK(success);
    DCHECK(nigori_specifics.has_cross_user_sharing_public_key());
    return nigori_specifics.cross_user_sharing_public_key();
  }

  void InjectInvitationToServer(
      const sync_pb::IncomingPasswordSharingInvitationSpecifics&
          invitation_specifics) {
    EntitySpecifics specifics;
    specifics.mutable_incoming_password_sharing_invitation()->CopyFrom(
        invitation_specifics);
    GetFakeServer()->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/"",
            /*client_tag=*/
            specifics.incoming_password_sharing_invitation().guid(), specifics,
            /*creation_time=*/0, /*last_modified_time=*/0));
  }

  bool SetupSyncTransportWithoutPasswordAccountStorage() {
    if (!SetupClients()) {
      return false;
    }
    if (!GetClient(0)->SignInPrimaryAccount()) {
      return false;
    }
    if (!GetClient(0)->AwaitSyncTransportActive()) {
      return false;
    }

#if !BUILDFLAG(IS_ANDROID)
    // Explicitly opt out of account storage when signin is explicit.
    if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
      password_manager::features_util::OptOutOfAccountStorage(
          GetProfile(0)->GetPrefs(), GetSyncService(0));
    }
#endif

    return true;
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientIncomingPasswordSharingInvitationTest,
                       ShouldStoreIncomingPassword) {
  ASSERT_TRUE(SetupSync());

  sync_pb::PasswordSharingInvitationData invitation_data =
      CreateDefaultIncomingInvitation(kUsernameValue, kPasswordValue);
  sync_pb::UserDisplayInfo sender_display_info =
      CreateDefaultSenderDisplayInfo();

  PasswordFormsAddedChecker password_forms_added_checker(
      GetProfilePasswordStoreInterface(0),
      /*expected_new_password_forms=*/1);
  InjectInvitationToServer(CreateEncryptedIncomingInvitationSpecifics(
      invitation_data, sender_display_info,
      /*recipient_public_key=*/GetPublicKeyFromServer(),
      syncer::CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair()));

  EXPECT_TRUE(password_forms_added_checker.Wait());
  std::vector<std::unique_ptr<PasswordForm>> all_logins =
      GetAllLogins(GetProfilePasswordStoreInterface(0));
  ASSERT_EQ(1u, all_logins.size());
  const PasswordForm& password_form = *all_logins.front();
  const sync_pb::PasswordSharingInvitationData::PasswordGroupData&
      invitation_group_data = invitation_data.password_group_data();
  const sync_pb::PasswordSharingInvitationData::PasswordGroupElementData&
      invitation_group_element_data = invitation_group_data.element_data(0);
  EXPECT_EQ(password_form.signon_realm,
            invitation_group_element_data.signon_realm());
  EXPECT_EQ(password_form.url.spec(), invitation_group_element_data.origin());
  EXPECT_EQ(base::UTF16ToUTF8(password_form.username_element),
            invitation_group_element_data.username_element());
  EXPECT_EQ(base::UTF16ToUTF8(password_form.username_value),
            invitation_group_data.username_value());
  EXPECT_EQ(base::UTF16ToUTF8(password_form.password_element),
            invitation_group_element_data.password_element());
  EXPECT_EQ(base::UTF16ToUTF8(password_form.password_value),
            invitation_group_data.password_value());
  EXPECT_EQ(base::UTF16ToUTF8(password_form.display_name),
            invitation_group_element_data.display_name());
  EXPECT_EQ(password_form.icon_url.spec(),
            invitation_group_element_data.avatar_url());

  EXPECT_EQ(base::UTF16ToUTF8(password_form.sender_email),
            sender_display_info.email());
  EXPECT_EQ(base::UTF16ToUTF8(password_form.sender_name),
            sender_display_info.display_name());
  EXPECT_EQ(password_form.sender_profile_image_url,
            GURL(sender_display_info.profile_image_url()));
}

IN_PROC_BROWSER_TEST_F(SingleClientIncomingPasswordSharingInvitationTest,
                       ShouldStoreIncomingPasswordGroup) {
  ASSERT_TRUE(SetupSync());

  sync_pb::PasswordSharingInvitationData invitation_data =
      CreateDefaultIncomingInvitation(kUsernameValue, kPasswordValue);

  // Create another password data to send in a group with the same password but
  // different username_element fields.
  invitation_data.mutable_password_group_data()->add_element_data()->CopyFrom(
      CreateDefaultIncomingInvitation(kUsernameValue, kPasswordValue)
          .password_group_data()
          .element_data(0));
  invitation_data.mutable_password_group_data()
      ->mutable_element_data(0)
      ->set_username_element("username_element_1");
  invitation_data.mutable_password_group_data()
      ->mutable_element_data(1)
      ->set_username_element("username_element_2");

  sync_pb::UserDisplayInfo sender_display_info =
      CreateDefaultSenderDisplayInfo();

  PasswordFormsAddedChecker password_forms_added_checker(
      GetProfilePasswordStoreInterface(0),
      /*expected_new_password_forms=*/2);
  InjectInvitationToServer(CreateEncryptedIncomingInvitationSpecifics(
      invitation_data, sender_display_info,
      /*recipient_public_key=*/GetPublicKeyFromServer(),
      syncer::CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair()));

  EXPECT_TRUE(password_forms_added_checker.Wait());
  std::vector<std::unique_ptr<PasswordForm>> passwords =
      GetAllLogins(GetProfilePasswordStoreInterface(0));
  EXPECT_THAT(passwords,
              Contains(Pointee(HasPasswordValue(kPasswordValue))).Times(2));
  EXPECT_THAT(
      passwords,
      UnorderedElementsAre(Pointee(HasUsernameElement("username_element_1")),
                           Pointee(HasUsernameElement("username_element_2"))));
}

IN_PROC_BROWSER_TEST_F(SingleClientIncomingPasswordSharingInvitationTest,
                       ShouldIssueTombstoneAfterProcessingInvitation) {
  ASSERT_TRUE(SetupSync());

  PasswordFormsAddedChecker password_forms_added_checker(
      GetProfilePasswordStoreInterface(0),
      /*expected_new_password_forms=*/1);
  InjectInvitationToServer(CreateInvitationSpecifics(GetPublicKeyFromServer()));

  // Wait the invitation to be processed and the password stored.
  ASSERT_TRUE(password_forms_added_checker.Wait());

  // Check that all the invitations are eventually deleted from the server.
  // PasswordFormsAddedChecker above guarantees that there is an invitation
  // present on the server (which was injected earlier).
  EXPECT_TRUE(ServerPasswordInvitationChecker(/*expected_count=*/0).Wait());
}

// ChromeOS does not support signing out of a primary account, which these test
// relies on to initialize Nigori.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SingleClientIncomingPasswordSharingInvitationTest,
                       ShouldHandleIncomingInvitationsAtInitialSync) {
  // First, setup sync to initialize Nigori node with a public key to be able to
  // inject invitations.
  ASSERT_TRUE(SetupSync());

  // Then stop sync service, inject an invitation to the server, and re-enable
  // sync again.
  GetClient(0)->SignOutPrimaryAccount();
  InjectInvitationToServer(CreateInvitationSpecifics(GetPublicKeyFromServer()));
  PasswordFormsAddedChecker password_forms_added_checker(
      GetProfilePasswordStoreInterface(0),
      /*expected_new_password_forms=*/1);
  ASSERT_TRUE(GetClient(0)->SetupSync());

  // Wait the invitation to be processed and the password stored.
  ASSERT_TRUE(password_forms_added_checker.Wait());
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
  constexpr char kLocalPasswordValue[] = "local_password";
  base::HistogramTester histogram_tester;

  // First, setup sync to initialize Nigori node with a public key to be able to
  // inject invitations.
  ASSERT_TRUE(SetupSync());

  // Then stop sync service, inject an invitation and a different password
  // (but having the same client tag to cause a collision) to the server.
  GetClient(0)->SignOutPrimaryAccount();

  sync_pb::PasswordSharingInvitationData invitation_data =
      CreateDefaultIncomingInvitation(kUsernameValue, kPasswordValue);
  InjectInvitationToServer(CreateEncryptedIncomingInvitationSpecifics(
      invitation_data, CreateDefaultSenderDisplayInfo(),
      /*recipient_public_key=*/GetPublicKeyFromServer(),
      /*sender_key_pair=*/
      syncer::CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair()));

  sync_pb::PasswordSpecificsData password_data;
  password_data.set_password_value(kLocalPasswordValue);
  FillPasswordClientTagFromInvitation(invitation_data, &password_data);
  passwords_helper::InjectKeystoreEncryptedServerPassword(password_data,
                                                          GetFakeServer());

  PasswordFormsAddedChecker password_forms_added_checker(
      GetProfilePasswordStoreInterface(0),
      /*expected_new_password_forms=*/1);

  // Re-enable sync again.
  ASSERT_TRUE(GetClient(0)->SetupSync());

  // Wait the password to be stored.
  ASSERT_TRUE(password_forms_added_checker.Wait());

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
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProcessIncomingPasswordSharingInvitationResult",
      ProcessIncomingPasswordSharingInvitationResult::
          kCredentialsExistWithDifferentPassword,
      /*expected_bucket_count=*/1);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// The unconsented primary account isn't supported on ChromeOS.
// TODO(crbug.com/358053884): enable on Android once transport mode for
// Passwords is supported.
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientIncomingPasswordSharingInvitationTest,
                       ShouldStoreIncomingPasswordIntoAccountDB) {
  // First, setup sync (in transport mode) to initialize Nigori node with a
  // public key to be able to inject invitations.
  ASSERT_TRUE(SetupSyncTransportWithoutPasswordAccountStorage());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(ServerCrossUserSharingPublicKeyChangedChecker().Wait());

  // Let the user opt in to the account-scoped password storage, and wait for it
  // to become active.
  password_manager::features_util::OptInToAccountStorage(
      GetProfile(0)->GetPrefs(), GetSyncService(0));
  PasswordSyncActiveChecker(GetSyncService(0)).Wait();
  ASSERT_THAT(GetAllLogins(GetAccountPasswordStoreInterface(0)), IsEmpty());

  PasswordFormsAddedChecker password_forms_added_checker(
      GetAccountPasswordStoreInterface(0),
      /*expected_new_password_forms=*/1);
  InjectInvitationToServer(CreateInvitationSpecifics(GetPublicKeyFromServer()));
  EXPECT_TRUE(password_forms_added_checker.Wait());
  EXPECT_TRUE(ServerPasswordInvitationChecker(/*expected_count=*/0).Wait());

  EXPECT_THAT(GetAllLogins(GetProfilePasswordStoreInterface(0)), IsEmpty());
  EXPECT_THAT(GetAllLogins(GetAccountPasswordStoreInterface(0)),
              Contains(Pointee(
                  AllOf(Field(&PasswordForm::password_value,
                              base::UTF8ToUTF16(std::string(kPasswordValue))),
                        Field(&PasswordForm::type,
                              PasswordForm::Type::kReceivedViaSharing)))));
}

// This test verifies that Incoming Password Sharing Invitation data type is
// stopped when the Password data type is opted out in the transport mode.
IN_PROC_BROWSER_TEST_F(SingleClientIncomingPasswordSharingInvitationTest,
                       ShouldStopReceivingPasswordsWhenPasswordsOptedOut) {
  ASSERT_TRUE(SetupSyncTransportWithoutPasswordAccountStorage());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Passwords and hence password sharing invitations should be disabled by
  // default in transport mode.
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::INCOMING_PASSWORD_SHARING_INVITATION));

  // Let the user opt in to the account-scoped password storage, and wait for it
  // to become active.
  password_manager::features_util::OptInToAccountStorage(
      GetProfile(0)->GetPrefs(), GetSyncService(0));
  PasswordSyncActiveChecker(GetSyncService(0)).Wait();

  // Double check that both Passwords and Sharing Invitations are enabled.
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::INCOMING_PASSWORD_SHARING_INVITATION));

  password_manager::features_util::OptOutOfAccountStorageAndClearSettings(
      GetProfile(0)->GetPrefs(), GetSyncService(0));
  EXPECT_TRUE(
      IncomingPasswordSharingInvitationInactiveChecker(GetSyncService(0))
          .Wait());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

// This test verifies that Incoming Password Sharing Invitation data type is
// stopped when the Password data type is encountered error.
IN_PROC_BROWSER_TEST_F(SingleClientIncomingPasswordSharingInvitationTest,
                       ShouldStopIncomingInvitationsOnPasswordsFailure) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::INCOMING_PASSWORD_SHARING_INVITATION));

  // Simulate Passwords data type error.
  GetSyncService(0)->ReportDataTypeErrorForTest(syncer::PASSWORDS);
  ExcludeDataTypesFromCheckForDataTypeFailures({syncer::PASSWORDS});

  EXPECT_TRUE(
      IncomingPasswordSharingInvitationInactiveChecker(GetSyncService(0))
          .Wait());
}

}  // namespace
