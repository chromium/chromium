// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_impl.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "ash/constants/web_app_id_constants.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/notimplemented.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/ui/passwords/settings/fake_password_import_controller.h"
#include "chrome/browser/ui/passwords/settings/mock_password_import_controller.h"
#include "chrome/browser/ui/passwords/settings/password_import_controller_interface.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/webauthn/enclave_manager_interface.h"
#include "chrome/browser/webauthn/mock_enclave_manager.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/device_reauth/device_reauth_metrics_util.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/leak_detection/mock_bulk_leak_check_service.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/sharing/mock_password_sender_service.h"
#include "components/password_manager/core/browser/sharing/password_sharing_recipients_downloader.h"
#include "components/password_manager/core/browser/sharing/recipients_fetcher_impl.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/protocol/password_sharing_recipients.pb.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/version_info/channel.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/clipboard/test/clipboard_test_util.h"
#include "ui/base/clipboard/test/test_clipboard.h"

namespace extensions {
namespace {

using api::passwords_private::FamilyFetchResults;
using api::passwords_private::ImportResults;
using api::passwords_private::PasswordUiEntry;
using api::passwords_private::PublicKey;
using api::passwords_private::RecipientInfo;
using api::passwords_private::UrlCollection;
using device_reauth::ReauthResult;
using password_manager::PasswordForm;
using password_manager::PasswordRecipient;
using password_manager::TestPasswordStore;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Ne;
using ::testing::Optional;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

constexpr char kSharingRecipientId1[] = "user id 1";
constexpr char kSharingRecipientKeyValue1[] = "key 1";
constexpr char kSharingRecipientKeyValue2[] = "key 2";
constexpr char kSharingRecipientId2[] = "user id 2";
constexpr char kSharingRecipientDisplayName1[] = "User One";
constexpr char kSharingRecipientDisplayName2[] = "User Two";
constexpr char kSharingRecipientEmail1[] = "user1@example.com";
constexpr char kSharingRecipientEmail2[] = "user2@example.com";
constexpr char kSharingRecipientProfileImageUrl1[] = "image1.example.com";
constexpr char kSharingRecipientProfileImageUrl2[] = "image2.example.com";
constexpr char kTestUserId[] = "12345";
constexpr char kTestUserName[] = "Theo Tester";
constexpr char kTestEmail[] = "theo@example.com";
constexpr char kTestProfileImageUrl[] =
    "https://3837fjsdjaka.image.example.com";
constexpr char kTestPublicKeyBase64[] =
    "MDEyMzQ1Njc4OTAxMjM0NTY3ODkwMTIzNDU2Nzg5MTI=";
constexpr uint32_t kTestPublicKeyVersion = 42;

PasswordForm CreateSampleForm(
    PasswordForm::Store store = PasswordForm::Store::kProfileStore,
    const std::u16string& username = u"test@gmail.com") {
  PasswordForm form;
  form.signon_realm = "https://abc1.com";
  form.url = GURL("https://abc1.com");
  form.username_value = username;
  form.password_value = u"test";
  form.in_store = store;
  return form;
}

sync_pb::WebauthnCredentialSpecifics CreatePasskey() {
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_sync_id(base::RandBytesAsString(16));
  passkey.set_credential_id(base::RandBytesAsString(16));
  passkey.set_rp_id("abc1.com");
  passkey.set_user_id({1, 2, 3, 4});
  passkey.set_user_name("passkey_username");
  passkey.set_user_display_name("passkey_display_name");
  return passkey;
}

MATCHER_P(PasswordUiEntryDataEquals, expected, "") {
  return testing::Value(expected.get().is_passkey, arg.is_passkey) &&
         testing::Value(expected.get().affiliated_domains[0].signon_realm,
                        arg.affiliated_domains[0].signon_realm) &&
         testing::Value(expected.get().username, arg.username) &&
         testing::Value(expected.get().display_name, arg.display_name) &&
         testing::Value(expected.get().stored_in, arg.stored_in);
}

void ExpectAuthentication(scoped_refptr<PasswordsPrivateDelegateImpl> delegate,
                          bool successful) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  auto biometric_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  EXPECT_CALL(*biometric_authenticator, AuthenticateWithMessage)
      .WillOnce(base::test::RunOnceCallback<1>(successful));
  delegate->SetDeviceAuthenticatorForTesting(
      std::move(biometric_authenticator));
#else
  NOTIMPLEMENTED();
#endif
}

class PasswordsPrivateDelegateImplTest : public testing::Test {
 public:
  PasswordsPrivateDelegateImplTest() {
    password_manager::PasswordManager::RegisterProfilePrefs(
        testing_pref_service_.registry());

    profile_store_->Init();
    account_store_->Init();

    ui::TestClipboard::CreateForCurrentThread();
    sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);

    ON_CALL(mock_affiliation_service_, GetPSLExtensions)
        .WillByDefault(
            [](base::OnceCallback<void(std::vector<std::string>)> callback) {
              std::move(callback).Run({});
            });
    ON_CALL(mock_affiliation_service_, GetGroupingInfo)
        .WillByDefault(
            [](std::vector<affiliations::FacetURI> facets,
               affiliations::AffiliationService::GroupsCallback callback) {
              std::vector<affiliations::GroupedFacets> groups;
              for (const auto& facet_uri : facets) {
                affiliations::GroupedFacets group;
                group.facets.emplace_back(facet_uri);
                groups.push_back(std::move(group));
              }
              std::move(callback).Run(groups);
            });
  }

  ~PasswordsPrivateDelegateImplTest() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
    profile_store_->ShutdownOnUIThread();
    account_store_->ShutdownOnUIThread();
  }

  // Sets up a testing password store and fills it with |forms|.
  void SetUpPasswordStores(std::vector<PasswordForm> forms) {
    for (const PasswordForm& form : forms) {
      if (form.IsUsingAccountStore()) {
        account_store_->AddLogin(password_manager::FromPasswordForm(form));
      } else if (form.IsUsingProfileStore()) {
        profile_store_->AddLogin(password_manager::FromPasswordForm(form));
      } else {
        NOTREACHED() << "Store not set";
      }
    }
    // Spin the loop to allow PasswordStore tasks being processed.
    RunUntilIdle();
  }

  scoped_refptr<PasswordsPrivateDelegateImpl> CreateDelegate() {
    return base::MakeRefCounted<PasswordsPrivateDelegateImpl>(
        &testing_pref_service_, identity_test_env_.identity_manager(),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        &password_sender_service_, &sync_service_,
        /*trust_safety_sentiment_service=*/nullptr,
        /*affiliation_service=*/&mock_affiliation_service_, profile_store_,
        account_store_, &passkey_model_, &mock_bulk_leak_check_service_,
        /*event_router=*/nullptr, &web_app_install_manager_, &enclave_manager_,
        base::NullCallback(), base::DoNothing());
  }

  // Queries and returns the list of saved credentials, blocking until finished.
  PasswordsPrivateDelegate::UiEntries GetCredentials(
      PasswordsPrivateDelegate& delegate) {
    PasswordsPrivateDelegate::UiEntries result;
    base::RunLoop run_loop;
    delegate.GetSavedPasswordsList(base::BindLambdaForTesting(
        [&](const PasswordsPrivateDelegate::UiEntries& entries) {
          for (const auto& entry : entries) {
            result.emplace_back(entry.Clone());
          }
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  syncer::TestSyncService* sync_service() { return &sync_service_; }
  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }
  network::TestURLLoaderFactory& url_loader_factory() {
    return test_url_loader_factory_;
  }
  password_manager::TestPasswordStore* profile_store() {
    return profile_store_.get();
  }
  password_manager::TestPasswordStore* account_store() {
    return account_store_.get();
  }
  webauthn::TestPasskeyModel* passkey_model() { return &passkey_model_; }
  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return &testing_pref_service_;
  }
  password_manager::MockPasswordSenderService* password_sender_service() {
    return &password_sender_service_;
  }
  affiliations::MockAffiliationService* mock_affiliation_service() {
    return &mock_affiliation_service_;
  }
  MockEnclaveManager* enclave_manager() { return &enclave_manager_; }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }
  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  sync_preferences::TestingPrefServiceSyncable testing_pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  password_manager::MockPasswordSenderService password_sender_service_;
  syncer::TestSyncService sync_service_;
  scoped_refptr<TestPasswordStore> profile_store_ =
      base::MakeRefCounted<TestPasswordStore>(
          password_manager::IsAccountStore(false));
  scoped_refptr<TestPasswordStore> account_store_ =
      base::MakeRefCounted<TestPasswordStore>(
          password_manager::IsAccountStore(true));
  webauthn::TestPasskeyModel passkey_model_;
  password_manager::MockBulkLeakCheckService mock_bulk_leak_check_service_;
  web_app::WebAppInstallManager web_app_install_manager_{
      &testing_pref_service_};
  MockEnclaveManager enclave_manager_;
  affiliations::MockAffiliationService mock_affiliation_service_;
};

TEST_F(PasswordsPrivateDelegateImplTest, GetSavedPasswordsList) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();

  base::MockCallback<PasswordsPrivateDelegate::UiEntriesCallback> callback;
  EXPECT_CALL(callback, Run).Times(0);
  delegate->GetSavedPasswordsList(callback.Get());

  EXPECT_CALL(callback, Run);
  SetUpPasswordStores({});

  EXPECT_CALL(callback, Run);
  delegate->GetSavedPasswordsList(callback.Get());
}

TEST_F(PasswordsPrivateDelegateImplTest, GetActionableErrorFromAccountStore) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  EXPECT_EQ(password_manager::ActionableError::kNoError,
            delegate->GetActionableError());

  account_store()->SetError(
      password_manager::ActionableError::kTrustedVaultKeyNeeded);

  EXPECT_EQ(password_manager::ActionableError::kTrustedVaultKeyNeeded,
            delegate->GetActionableError());
}

TEST_F(PasswordsPrivateDelegateImplTest,
       GetActionableErrorPrioritizesAccountStore) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  profile_store()->SetError(password_manager::ActionableError::kKeychainError);
  account_store()->SetError(
      password_manager::ActionableError::kTrustedVaultKeyNeeded);

  EXPECT_EQ(password_manager::ActionableError::kTrustedVaultKeyNeeded,
            delegate->GetActionableError());
}

TEST_F(PasswordsPrivateDelegateImplTest,
       GetReturnProfileErrorIfNoAccountError) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  profile_store()->SetError(password_manager::ActionableError::kKeychainError);
  account_store()->SetError(password_manager::ActionableError::kNoError);

  EXPECT_EQ(password_manager::ActionableError::kKeychainError,
            delegate->GetActionableError());
}

TEST_F(PasswordsPrivateDelegateImplTest,
       PasswordsDuplicatedInStoresAreRepresentedAsSingleEntity) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();

  PasswordForm account_password =
      CreateSampleForm(PasswordForm::Store::kAccountStore);
  PasswordForm profile_password =
      CreateSampleForm(PasswordForm::Store::kProfileStore);

  SetUpPasswordStores({account_password, profile_password});

  base::MockCallback<PasswordsPrivateDelegate::UiEntriesCallback> callback;
  EXPECT_CALL(callback, Run(SizeIs(1)))
      .WillOnce([&](const PasswordsPrivateDelegate::UiEntries& passwords) {
        EXPECT_EQ(api::passwords_private::PasswordStoreSet::kDeviceAndAccount,
                  passwords[0].stored_in);
      });

  delegate->GetSavedPasswordsList(callback.Get());
}

TEST_F(PasswordsPrivateDelegateImplTest, GetPasswordExceptionsList) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();

  base::MockCallback<PasswordsPrivateDelegate::ExceptionEntriesCallback>
      callback;
  EXPECT_CALL(callback, Run).Times(0);
  delegate->GetPasswordExceptionsList(callback.Get());

  EXPECT_CALL(callback, Run);
  SetUpPasswordStores({});

  EXPECT_CALL(callback, Run);
  delegate->GetPasswordExceptionsList(callback.Get());
}

TEST_F(PasswordsPrivateDelegateImplTest,
       ExceptionsDuplicatedInStoresAreRepresentedAsSingleEntity) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  PasswordForm account_exception;
  account_exception.blocked_by_user = true;
  account_exception.url = GURL("https://test.com");
  account_exception.in_store = PasswordForm::Store::kAccountStore;
  PasswordForm profile_exception;
  profile_exception.url = GURL("https://test.com");
  profile_exception.blocked_by_user = true;
  profile_exception.in_store = PasswordForm::Store::kProfileStore;

  SetUpPasswordStores({account_exception, profile_exception});

  base::MockCallback<PasswordsPrivateDelegate::ExceptionEntriesCallback>
      callback;

  EXPECT_CALL(callback, Run(SizeIs(1)));
  delegate->GetPasswordExceptionsList(callback.Get());
}

TEST_F(PasswordsPrivateDelegateImplTest, AddPassword) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  RunUntilIdle();

  base::MockCallback<PasswordsPrivateDelegate::UiEntriesCallback> callback;
  EXPECT_CALL(callback, Run(SizeIs(0)));
  delegate->GetSavedPasswordsList(callback.Get());

  EXPECT_TRUE(delegate->AddPassword(/*url=*/"example1.com",
                                    /*username=*/u"username1",
                                    /*password=*/u"password1", /*note=*/u"",
                                    /*use_account_store=*/true));
  EXPECT_TRUE(delegate->AddPassword(
      /*url=*/"http://example2.com/login?param=value",
      /*username=*/u"", /*password=*/u"password2", /*note=*/u"note",
      /*use_account_store=*/false));
  RunUntilIdle();

  PasswordUiEntry expected_entry1;
  expected_entry1.affiliated_domains.emplace_back();
  expected_entry1.affiliated_domains.back().signon_realm =
      "https://example1.com/";
  expected_entry1.username = "username1";
  expected_entry1.note.emplace();
  expected_entry1.stored_in =
      api::passwords_private::PasswordStoreSet::kAccount;
  PasswordUiEntry expected_entry2;
  expected_entry2.affiliated_domains.emplace_back();
  expected_entry2.affiliated_domains.back().signon_realm =
      "http://example2.com/";
  expected_entry2.username = "";
  expected_entry2.note = "note";
  expected_entry2.stored_in = api::passwords_private::PasswordStoreSet::kDevice;
  EXPECT_CALL(callback,
              Run(testing::UnorderedElementsAre(
                  PasswordUiEntryDataEquals(testing::ByRef(expected_entry1)),
                  PasswordUiEntryDataEquals(testing::ByRef(expected_entry2)))));
  delegate->GetSavedPasswordsList(callback.Get());
}

TEST_F(PasswordsPrivateDelegateImplTest, TestReauthFailedOnImport) {
  base::HistogramTester histogram_tester;
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();

  auto fake_controller = std::make_unique<FakePasswordImportController>();
  auto* fake_controller_ptr = fake_controller.get();
  delegate->SetImportControllerForTesting(std::move(fake_controller));

  const auto kExpectedStatus =
      password_manager::ImportResults::Status::DISMISSED;
  fake_controller_ptr->set_import_result_status(kExpectedStatus);

  ExpectAuthentication(delegate, /*successful=*/false);

  base::MockCallback<PasswordsPrivateDelegate::ImportResultsCallback>
      import_callback;
  EXPECT_CALL(import_callback,
              Run(::testing::Field(
                  &ImportResults::status,
                  api::passwords_private::ImportResultsStatus::kDismissed)))
      .Times(1);

  delegate->ContinueImport(/*selected_ids=*/{1}, import_callback.Get());
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample("PasswordManager.ImportResultsStatus2",
                                      kExpectedStatus, 1);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       ContinueImportLogsImportResultsStatus) {
  base::HistogramTester histogram_tester;
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();

  auto fake_controller = std::make_unique<FakePasswordImportController>();
  auto* fake_controller_ptr = fake_controller.get();
  delegate->SetImportControllerForTesting(std::move(fake_controller));

  const auto kExpectedStatus =
      password_manager::ImportResults::Status::BAD_FORMAT;
  fake_controller_ptr->set_import_result_status(kExpectedStatus);

  base::MockCallback<PasswordsPrivateDelegate::ImportResultsCallback> callback;
  EXPECT_CALL(callback,
              Run(::testing::Field(
                  &ImportResults::status,
                  api::passwords_private::ImportResultsStatus::kBadFormat)))
      .Times(1);
  delegate->ContinueImport(/*selected_ids=*/{}, callback.Get());
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample("PasswordManager.ImportResultsStatus2",
                                      kExpectedStatus, 1);
}

TEST_F(PasswordsPrivateDelegateImplTest, ResetImporter) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();

  auto mock_controller = std::make_unique<MockPasswordImportController>();
  auto* mock_controller_ptr = mock_controller.get();
  delegate->SetImportControllerForTesting(std::move(mock_controller));

  EXPECT_CALL(*mock_controller_ptr, ResetImporter).Times(1);
  delegate->ResetImporter(/*delete_file=*/false);
}

TEST_F(PasswordsPrivateDelegateImplTest, ChangeCredential_Password) {
  PasswordForm sample_form = CreateSampleForm();
  SetUpPasswordStores({sample_form});
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  RunUntilIdle();

  PasswordUiEntry updated_credential = GetCredentials(*delegate).at(0).Clone();
  updated_credential.password = "new_pass";
  updated_credential.username = "new_user";

  EXPECT_TRUE(delegate->ChangeCredential(updated_credential));
  RunUntilIdle();

  const PasswordsPrivateDelegate::UiEntries& credentials =
      GetCredentials(*delegate);
  EXPECT_EQ(credentials.size(), 1u);
  const PasswordUiEntry& refreshed_credential = credentials.at(0);
  EXPECT_EQ(refreshed_credential.username, "new_user");
  EXPECT_EQ(refreshed_credential.note, std::nullopt);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       ChangeCredential_PasswordInBothStores) {
  PasswordForm profile_form = CreateSampleForm();
  PasswordForm account_form = profile_form;
  account_form.in_store = PasswordForm::Store::kAccountStore;
  SetUpPasswordStores({profile_form, account_form});

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  RunUntilIdle();

  PasswordUiEntry updated_credential = GetCredentials(*delegate).at(0).Clone();
  updated_credential.password = "new_pass";
  updated_credential.username = "new_user";

  EXPECT_TRUE(delegate->ChangeCredential(updated_credential));
  RunUntilIdle();

  const PasswordsPrivateDelegate::UiEntries& credentials =
      GetCredentials(*delegate);
  EXPECT_EQ(credentials.size(), 1u);
  const PasswordUiEntry& refreshed_credential = credentials.at(0);
  EXPECT_EQ(refreshed_credential.username, "new_user");
  EXPECT_EQ(refreshed_credential.stored_in,
            api::passwords_private::PasswordStoreSet::kDeviceAndAccount);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       ChangeCredential_PasswordInAccountStore) {
  PasswordForm profile_form = CreateSampleForm();
  profile_form.password_value = u"different_pass";
  PasswordForm account_form = CreateSampleForm();
  account_form.in_store = PasswordForm::Store::kAccountStore;
  SetUpPasswordStores({profile_form, account_form});

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  RunUntilIdle();

  const PasswordsPrivateDelegate::UiEntries& credentials =
      GetCredentials(*delegate);
  EXPECT_EQ(credentials.size(), 2u);
  const auto account_credential_it = std::ranges::find(
      credentials, api::passwords_private::PasswordStoreSet::kAccount,
      &PasswordUiEntry::stored_in);
  ASSERT_NE(account_credential_it, credentials.end());

  PasswordUiEntry updated_credential = account_credential_it->Clone();
  updated_credential.password = "new_pass";
  updated_credential.username = "new_user";

  EXPECT_TRUE(delegate->ChangeCredential(updated_credential));
  RunUntilIdle();

  const PasswordsPrivateDelegate::UiEntries& updated_credentials =
      GetCredentials(*delegate);
  EXPECT_EQ(updated_credentials.size(), 2u);
  const auto refreshed_credential_it = std::ranges::find(
      updated_credentials, api::passwords_private::PasswordStoreSet::kAccount,
      &PasswordUiEntry::stored_in);
  ASSERT_NE(account_credential_it, updated_credentials.end());
  EXPECT_EQ(refreshed_credential_it->username, "new_user");
}

TEST_F(PasswordsPrivateDelegateImplTest, ChangeCredential_Passkey) {
  sync_pb::WebauthnCredentialSpecifics passkey = CreatePasskey();
  passkey_model()->AddNewPasskeyForTesting(passkey);

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  RunUntilIdle();

  const PasswordsPrivateDelegate::UiEntries& credentials =
      GetCredentials(*delegate);
  EXPECT_EQ(credentials.size(), 1u);
  const PasswordUiEntry& existing_credential = credentials.at(0);
  EXPECT_TRUE(existing_credential.is_passkey);

  PasswordUiEntry updated_credential = existing_credential.Clone();
  updated_credential.username = "new_user";
  updated_credential.display_name = "new_display_name";

  EXPECT_TRUE(delegate->ChangeCredential(updated_credential));
  RunUntilIdle();

  const PasswordsPrivateDelegate::UiEntries& updated_credentials =
      GetCredentials(*delegate);
  EXPECT_EQ(updated_credentials.size(), 1u);
  EXPECT_EQ(updated_credentials.at(0).username, "new_user");
  EXPECT_EQ(updated_credentials.at(0).display_name, "new_display_name");
}

TEST_F(PasswordsPrivateDelegateImplTest, ChangeCredential_NotFound) {
  SetUpPasswordStores({});
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  RunUntilIdle();
  EXPECT_FALSE(delegate->ChangeCredential(PasswordUiEntry()));
}

TEST_F(PasswordsPrivateDelegateImplTest, ChangeCredential_EmptyPassword) {
  PasswordForm sample_form = CreateSampleForm();
  SetUpPasswordStores({sample_form});
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  RunUntilIdle();

  PasswordUiEntry updated_credential = GetCredentials(*delegate).at(0).Clone();
  updated_credential.password = "";
  updated_credential.username = "new_user";

  EXPECT_FALSE(delegate->ChangeCredential(updated_credential));
}

TEST_F(PasswordsPrivateDelegateImplTest, TestCopyPasswordCallbackResult) {
  base::HistogramTester histogram_tester;
  PasswordForm form = CreateSampleForm();
  SetUpPasswordStores({form});

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  RunUntilIdle();

  ExpectAuthentication(delegate, /*successful=*/true);

  base::MockCallback<PasswordsPrivateDelegate::PlaintextPasswordCallback>
      password_callback;
  EXPECT_CALL(password_callback, Run(Eq(std::u16string())));
  delegate->RequestPlaintextPassword(
      0, api::passwords_private::PlaintextReason::kCopy,
      password_callback.Get());

  std::u16string result;
  result = ui::clipboard_test_util::ReadText(
      ui::Clipboard::GetForCurrentThread(), ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr);
  EXPECT_EQ(form.password_value, result);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccessPasswordInSettings",
      password_manager::metrics_util::ACCESS_PASSWORD_COPIED, 1);
}

TEST_F(
    PasswordsPrivateDelegateImplTest,
    TestCopyPasswordCallbackResult_ClipboardClearTimerStarted) {
  PasswordForm form = CreateSampleForm();
  SetUpPasswordStores({form});

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  {
    base::RunLoop run_loop;
    delegate->GetSavedPasswordsList(base::BindLambdaForTesting(
        [&](const PasswordsPrivateDelegate::UiEntries&) { run_loop.Quit(); }));
    run_loop.Run();
  }

  ExpectAuthentication(delegate, /*successful=*/true);

  base::MockCallback<PasswordsPrivateDelegate::PlaintextPasswordCallback>
      password_callback;
  EXPECT_CALL(password_callback, Run(Eq(std::u16string())));
  delegate->RequestPlaintextPassword(
      0, api::passwords_private::PlaintextReason::kCopy,
      password_callback.Get());

  std::u16string result;
  result = ui::clipboard_test_util::ReadText(
      ui::Clipboard::GetForCurrentThread(), ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr);
  EXPECT_EQ(form.password_value, result);

  // Simulate the clipboard clear timer expiring.
  FastForwardBy(base::Seconds(120));

  // Verify that the clipboard is now empty.
  result = ui::clipboard_test_util::ReadText(
      ui::Clipboard::GetForCurrentThread(), ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr);
  EXPECT_TRUE(result.empty());
}

TEST_F(
    PasswordsPrivateDelegateImplTest,
    TestCopyPasswordCallbackResult_ClipboardClearTimerStarted_NotClearedIfOverwritten) {
  PasswordForm form = CreateSampleForm();
  SetUpPasswordStores({form});

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  {
    base::RunLoop run_loop;
    delegate->GetSavedPasswordsList(base::BindLambdaForTesting(
        [&](const PasswordsPrivateDelegate::UiEntries&) { run_loop.Quit(); }));
    run_loop.Run();
  }

  ExpectAuthentication(delegate, /*successful=*/true);

  base::MockCallback<PasswordsPrivateDelegate::PlaintextPasswordCallback>
      password_callback;
  EXPECT_CALL(password_callback, Run(Eq(std::u16string())));
  delegate->RequestPlaintextPassword(
      0, api::passwords_private::PlaintextReason::kCopy,
      password_callback.Get());

  std::u16string result;
  result = ui::clipboard_test_util::ReadText(
      ui::Clipboard::GetForCurrentThread(), ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr);
  EXPECT_EQ(form.password_value, result);

  // Simulate user copying another piece of text before the timer fires.
  {
    ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(u"new text");
  }

  // Simulate the clipboard clear timer expiring.
  FastForwardBy(base::Seconds(120));

  // Verify that the clipboard still contains the new text and has not been
  // cleared.
  result = ui::clipboard_test_util::ReadText(
      ui::Clipboard::GetForCurrentThread(), ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr);
  EXPECT_EQ(u"new text", result);
}

TEST_F(PasswordsPrivateDelegateImplTest, CopyPlaintextBackupPassword) {
  PasswordForm form = CreateSampleForm();
  form.SetPasswordBackupNote(u"backup");
  SetUpPasswordStores({form});

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  RunUntilIdle();

  ExpectAuthentication(delegate, /*successful=*/true);

  base::MockCallback<base::OnceCallback<void(bool)>> result_callback;
  EXPECT_CALL(result_callback, Run(Eq(true)));
  delegate->CopyPlaintextBackupPassword(0, result_callback.Get());

  std::u16string result;
  result = ui::clipboard_test_util::ReadText(
      ui::Clipboard::GetForCurrentThread(), ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr);
  EXPECT_EQ(result, form.GetPasswordBackup());

  // Simulate the clipboard clear timer expiring.
  FastForwardBy(base::Seconds(120));

  // Verify that the clipboard is now empty.
  result = ui::clipboard_test_util::ReadText(
      ui::Clipboard::GetForCurrentThread(), ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr);
  EXPECT_TRUE(result.empty());
}

TEST_F(
    PasswordsPrivateDelegateImplTest,
    CopyPlaintextBackupPassword_NotClearedIfOverwritten) {
  PasswordForm form = CreateSampleForm();
  form.SetPasswordBackupNote(u"backup");
  SetUpPasswordStores({form});

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  {
    base::RunLoop run_loop;
    delegate->GetSavedPasswordsList(base::BindLambdaForTesting(
        [&](const PasswordsPrivateDelegate::UiEntries&) { run_loop.Quit(); }));
    run_loop.Run();
  }

  ExpectAuthentication(delegate, /*successful=*/true);

  base::MockCallback<base::OnceCallback<void(bool)>> result_callback;
  EXPECT_CALL(result_callback, Run(Eq(true)));
  delegate->CopyPlaintextBackupPassword(0, result_callback.Get());

  std::u16string result;
  result = ui::clipboard_test_util::ReadText(
      ui::Clipboard::GetForCurrentThread(), ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr);
  EXPECT_EQ(result, form.GetPasswordBackup());

  // Simulate user copying another piece of text before the timer fires.
  {
    ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(u"new text");
  }

  // Simulate the clipboard clear timer expiring.
  FastForwardBy(base::Seconds(120));

  // Verify that the clipboard still contains the new text and has not been
  // cleared.
  result = ui::clipboard_test_util::ReadText(
      ui::Clipboard::GetForCurrentThread(), ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr);
  EXPECT_EQ(u"new text", result);
}

TEST_F(PasswordsPrivateDelegateImplTest, TestShouldActivateAccountStorage) {
  sync_service()->SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, false);

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  delegate->SetAccountStorageEnabled(true);

  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPasswords));
}

TEST_F(PasswordsPrivateDelegateImplTest, TestShouldDisableAccountStorage) {
  sync_service()->SetSignedIn(signin::ConsentLevel::kSignin);
  ASSERT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPasswords));

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  delegate->SetAccountStorageEnabled(false);

  EXPECT_FALSE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPasswords));
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
TEST_F(PasswordsPrivateDelegateImplTest, TestCopyPasswordCallbackResultFail) {
  base::HistogramTester histogram_tester;
  SetUpPasswordStores({CreateSampleForm()});

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  RunUntilIdle();

  ExpectAuthentication(delegate, /*successful=*/false);

  base::Time before_call =
      ui::Clipboard::GetForCurrentThread()->GetLastModifiedTime();

  base::MockCallback<PasswordsPrivateDelegate::PlaintextPasswordCallback>
      password_callback;
  EXPECT_CALL(password_callback, Run(Eq(std::nullopt)));
  delegate->RequestPlaintextPassword(
      0, api::passwords_private::PlaintextReason::kCopy,
      password_callback.Get());
  std::u16string result;
  result = ui::clipboard_test_util::ReadText(
      ui::Clipboard::GetForCurrentThread(), ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr);
  EXPECT_EQ(std::u16string(), result);
  EXPECT_EQ(before_call,
            ui::Clipboard::GetForCurrentThread()->GetLastModifiedTime());

  histogram_tester.ExpectTotalCount("PasswordManager.AccessPasswordInSettings",
                                    0);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       CopyPlaintextBackupPasswordFailsAuthentication) {
  PasswordForm form = CreateSampleForm();
  form.SetPasswordBackupNote(u"backup");
  SetUpPasswordStores({form});

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  RunUntilIdle();

  ExpectAuthentication(delegate, /*successful=*/false);

  base::MockCallback<base::OnceCallback<void(bool)>> result_callback;
  EXPECT_CALL(result_callback, Run(Eq(false)));
  delegate->CopyPlaintextBackupPassword(0, result_callback.Get());

  std::u16string result;
  result = ui::clipboard_test_util::ReadText(
      ui::Clipboard::GetForCurrentThread(), ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr);
  EXPECT_EQ(result, std::u16string());
}
#endif

TEST_F(PasswordsPrivateDelegateImplTest, TestPassedReauthOnView) {
  base::HistogramTester histogram_tester;
  SetUpPasswordStores({CreateSampleForm()});

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  RunUntilIdle();

  ExpectAuthentication(delegate, /*successful=*/true);

  base::MockCallback<PasswordsPrivateDelegate::PlaintextPasswordCallback>
      password_callback;
  EXPECT_CALL(password_callback, Run(Eq(u"test")));
  delegate->RequestPlaintextPassword(
      0, api::passwords_private::PlaintextReason::kView,
      password_callback.Get());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccessPasswordInSettings",
      password_manager::metrics_util::ACCESS_PASSWORD_VIEWED, 1);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
TEST_F(PasswordsPrivateDelegateImplTest, TestFailedReauthOnView) {
  base::HistogramTester histogram_tester;
  SetUpPasswordStores({CreateSampleForm()});

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  RunUntilIdle();

  ExpectAuthentication(delegate, /*successful=*/false);

  base::MockCallback<PasswordsPrivateDelegate::PlaintextPasswordCallback>
      password_callback;
  EXPECT_CALL(password_callback, Run(Eq(std::nullopt)));
  delegate->RequestPlaintextPassword(
      0, api::passwords_private::PlaintextReason::kView,
      password_callback.Get());

  histogram_tester.ExpectTotalCount("PasswordManager.AccessPasswordInSettings",
                                    0);
}
#endif

TEST_F(PasswordsPrivateDelegateImplTest,
       GetUrlCollectionValueWithSchemeWhenIpAddress) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  const std::optional<UrlCollection> urls =
      delegate->GetUrlCollection("127.0.0.1");
  EXPECT_TRUE(urls.has_value());
  EXPECT_EQ("127.0.0.1", urls.value().shown);
  EXPECT_EQ("http://127.0.0.1/", urls.value().signon_realm);
  EXPECT_EQ("http://127.0.0.1/", urls.value().link);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       GetUrlCollectionValueWithSchemeWhenWebAddress) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  const std::optional<UrlCollection> urls =
      delegate->GetUrlCollection("example.com/login");
  EXPECT_TRUE(urls.has_value());
  EXPECT_EQ("example.com", urls.value().shown);
  EXPECT_EQ("https://example.com/", urls.value().signon_realm);
  EXPECT_EQ("https://example.com/login", urls.value().link);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       GetUrlCollectionStrippedValueWhenFullUrl) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  const std::optional<UrlCollection> urls = delegate->GetUrlCollection(
      "http://username:password@example.com/login?param=value#ref");
  EXPECT_TRUE(urls.has_value());
  EXPECT_EQ("example.com", urls.value().shown);
  EXPECT_EQ("http://example.com/", urls.value().signon_realm);
  EXPECT_EQ("http://example.com/login", urls.value().link);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       GetUrlCollectionNoValueWhenUnsupportedScheme) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  const std::optional<UrlCollection> urls =
      delegate->GetUrlCollection("scheme://unsupported");
  EXPECT_FALSE(urls.has_value());
}

TEST_F(PasswordsPrivateDelegateImplTest,
       GetUrlCollectionNoValueWhenInvalidUrl) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  const std::optional<UrlCollection> urls =
      delegate->GetUrlCollection("https://^/invalid");
  EXPECT_FALSE(urls.has_value());
}

TEST_F(PasswordsPrivateDelegateImplTest, TestMovePasswordsToAccountStore) {
  sync_service()->SetSignedIn(signin::ConsentLevel::kSignin);

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  PasswordForm form1 = CreateSampleForm(PasswordForm::Store::kProfileStore);

  SetUpPasswordStores({form1});

  int first_id =
      delegate->GetIdForCredential(password_manager::CredentialUIEntry(form1));

  delegate->MovePasswordsToAccount({first_id});
  RunUntilIdle();
}

TEST_F(PasswordsPrivateDelegateImplTest, VerifyCastingOfImportEntryStatus) {
  static_assert(
      std::to_underlying(api::passwords_private::ImportEntryStatus::kNone) ==
      int{password_manager::ImportEntry::Status::NONE});
  static_assert(std::to_underlying(
                    api::passwords_private::ImportEntryStatus::kUnknownError) ==
                int{password_manager::ImportEntry::Status::UNKNOWN_ERROR});
  static_assert(
      std::to_underlying(
          api::passwords_private::ImportEntryStatus::kMissingPassword) ==
      int{password_manager::ImportEntry::Status::MISSING_PASSWORD});
  static_assert(std::to_underlying(
                    api::passwords_private::ImportEntryStatus::kMissingUrl) ==
                int{password_manager::ImportEntry::Status::MISSING_URL});
  static_assert(std::to_underlying(
                    api::passwords_private::ImportEntryStatus::kInvalidUrl) ==
                int{password_manager::ImportEntry::Status::INVALID_URL});
  static_assert(
      std::to_underlying(api::passwords_private::ImportEntryStatus::kLongUrl) ==
      int{password_manager::ImportEntry::Status::LONG_URL});
  static_assert(std::to_underlying(
                    api::passwords_private::ImportEntryStatus::kLongPassword) ==
                int{password_manager::ImportEntry::Status::LONG_PASSWORD});
  static_assert(std::to_underlying(
                    api::passwords_private::ImportEntryStatus::kLongUsername) ==
                int{password_manager::ImportEntry::Status::LONG_USERNAME});
  static_assert(
      std::to_underlying(
          api::passwords_private::ImportEntryStatus::kConflictProfile) ==
      int{password_manager::ImportEntry::Status::CONFLICT_PROFILE});
  static_assert(
      std::to_underlying(
          api::passwords_private::ImportEntryStatus::kConflictAccount) ==
      int{password_manager::ImportEntry::Status::CONFLICT_ACCOUNT});
  static_assert(std::to_underlying(
                    api::passwords_private::ImportEntryStatus::kLongNote) ==
                int{password_manager::ImportEntry::Status::LONG_NOTE});
  static_assert(
      std::to_underlying(
          api::passwords_private::ImportEntryStatus::kLongConcatenatedNote) ==
      int{password_manager::ImportEntry::Status::LONG_CONCATENATED_NOTE});
  static_assert(
      std::to_underlying(api::passwords_private::ImportEntryStatus::kValid) ==
      int{password_manager::ImportEntry::Status::VALID});
}

TEST_F(PasswordsPrivateDelegateImplTest, VerifyCastingOfImportResultsStatus) {
  static_assert(
      std::to_underlying(api::passwords_private::ImportResultsStatus::kNone) ==
      int{password_manager::ImportResults::Status::NONE});
  static_assert(
      std::to_underlying(
          api::passwords_private::ImportResultsStatus::kUnknownError) ==
      int{password_manager::ImportResults::Status::UNKNOWN_ERROR});
  static_assert(std::to_underlying(
                    api::passwords_private::ImportResultsStatus::kSuccess) ==
                int{password_manager::ImportResults::Status::SUCCESS});
  static_assert(std::to_underlying(
                    api::passwords_private::ImportResultsStatus::kIoError) ==
                int{password_manager::ImportResults::Status::IO_ERROR});
  static_assert(std::to_underlying(
                    api::passwords_private::ImportResultsStatus::kBadFormat) ==
                int{password_manager::ImportResults::Status::BAD_FORMAT});
  static_assert(std::to_underlying(
                    api::passwords_private::ImportResultsStatus::kDismissed) ==
                int{password_manager::ImportResults::Status::DISMISSED});
  static_assert(
      std::to_underlying(
          api::passwords_private::ImportResultsStatus::kMaxFileSize) ==
      int{password_manager::ImportResults::Status::MAX_FILE_SIZE});
  static_assert(
      std::to_underlying(
          api::passwords_private::ImportResultsStatus::kImportAlreadyActive) ==
      int{password_manager::ImportResults::Status::IMPORT_ALREADY_ACTIVE});
  static_assert(
      std::to_underlying(
          api::passwords_private::ImportResultsStatus::kNumPasswordsExceeded) ==
      int{password_manager::ImportResults::Status::NUM_PASSWORDS_EXCEEDED});
  static_assert(std::to_underlying(
                    api::passwords_private::ImportResultsStatus::kConflicts) ==
                int{password_manager::ImportResults::Status::CONFLICTS});
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
TEST_F(PasswordsPrivateDelegateImplTest,
       SwitchBiometricAuthBeforeFillingState) {
  base::MockCallback<
      extensions::PasswordsPrivateDelegate::AuthenticationCallback>
      result_callback;

  prefs()->SetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling, false);

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  ExpectAuthentication(delegate, /*successful=*/true);

  EXPECT_CALL(result_callback, Run(/*auth_succeeded=*/true));
  delegate->SwitchBiometricAuthBeforeFillingState(result_callback.Get());
  EXPECT_TRUE(prefs()->GetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling));
}

TEST_F(PasswordsPrivateDelegateImplTest,
       SwitchBiometricAuthBeforeFillingStateAuthenticationFailed) {
  base::MockCallback<
      extensions::PasswordsPrivateDelegate::AuthenticationCallback>
      result_callback;

  prefs()->SetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling, false);

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  ExpectAuthentication(delegate, /*successful=*/false);

  EXPECT_CALL(result_callback, Run(/*auth_succeeded=*/false));
  delegate->SwitchBiometricAuthBeforeFillingState(result_callback.Get());

  EXPECT_FALSE(prefs()->GetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling));
}
#endif

#if BUILDFLAG(IS_MAC)
TEST_F(PasswordsPrivateDelegateImplTest,
       SwitchBiometricAuthBeforeFillingCancelsLastTry) {
  auto biometric_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  auto* biometric_authenticator_ptr = biometric_authenticator.get();

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  EXPECT_CALL(*biometric_authenticator_ptr, AuthenticateWithMessage);
  delegate->SetDeviceAuthenticatorForTesting(
      std::move(biometric_authenticator));

  delegate->SwitchBiometricAuthBeforeFillingState(base::DoNothing());

  EXPECT_CALL(*biometric_authenticator_ptr, Cancel);
  ExpectAuthentication(delegate, /*successful=*/true);
  delegate->SwitchBiometricAuthBeforeFillingState(base::DoNothing());
}
#endif

#if BUILDFLAG(IS_WIN)
TEST_F(PasswordsPrivateDelegateImplTest,
       SwitchBiometricAuthBeforeFillingDoesntCancelLastTry) {
  base::MockCallback<
      extensions::PasswordsPrivateDelegate::AuthenticationCallback>
      result_callback;

  auto biometric_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  auto* biometric_authenticator_ptr = biometric_authenticator.get();

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  EXPECT_CALL(*biometric_authenticator_ptr, AuthenticateWithMessage);
  delegate->SetDeviceAuthenticatorForTesting(
      std::move(biometric_authenticator));

  delegate->SwitchBiometricAuthBeforeFillingState(result_callback.Get());

  EXPECT_CALL(*biometric_authenticator_ptr, Cancel).Times(0);
  EXPECT_CALL(result_callback, Run(false));
  delegate->SwitchBiometricAuthBeforeFillingState(result_callback.Get());
}
#endif

TEST_F(PasswordsPrivateDelegateImplTest, GetCredentialGroups) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();

  PasswordForm password1 =
      CreateSampleForm(PasswordForm::Store::kProfileStore, u"username1");
  PasswordForm password2 =
      CreateSampleForm(PasswordForm::Store::kProfileStore, u"username2");
  const std::u16string backup_password = u"backup";
  password2.SetPasswordBackupNote(backup_password);
  api::passwords_private::BackupPasswordInfo backup_password_info;
  backup_password_info.value = base::UTF16ToUTF8(backup_password);
  backup_password_info.creation_date =
      base::UTF16ToUTF8(base::LocalizedTimeFormatWithPattern(
          password2.GetPasswordBackupDateCreated().value(), "MMM dd"));

  SetUpPasswordStores({password1, password2});
  {
    TestPasswordStore::PasswordMap logins_map =
        password_manager::GetAllLoginsSync(profile_store());
    ASSERT_EQ(1u, logins_map.size());
    ASSERT_EQ(2u, logins_map.begin()->second.size());
  }

  PasswordsPrivateDelegate::CredentialsGroups groups =
      delegate->GetCredentialGroups();
  EXPECT_EQ(1u, groups.size());
  EXPECT_EQ(2u, groups[0].entries.size());
  EXPECT_EQ("abc1.com", groups[0].name);
  EXPECT_EQ("https://abc1.com/favicon.ico", groups[0].icon_url);

  PasswordUiEntry expected_entry1;
  expected_entry1.affiliated_domains.emplace_back();
  expected_entry1.affiliated_domains.back().signon_realm = "https://abc1.com";
  expected_entry1.username = "username1";
  expected_entry1.stored_in = api::passwords_private::PasswordStoreSet::kDevice;
  PasswordUiEntry expected_entry2;
  expected_entry2.affiliated_domains.emplace_back();
  expected_entry2.affiliated_domains.back().signon_realm = "https://abc1.com";
  expected_entry2.username = "username2";
  expected_entry2.stored_in = api::passwords_private::PasswordStoreSet::kDevice;
  expected_entry2.backup_password = std::move(backup_password_info);
  EXPECT_THAT(groups[0].entries,
              testing::UnorderedElementsAre(
                  PasswordUiEntryDataEquals(testing::ByRef(expected_entry1)),
                  PasswordUiEntryDataEquals(testing::ByRef(expected_entry2))));
}

TEST_F(PasswordsPrivateDelegateImplTest, PasswordManagerAppInstalled) {
  base::HistogramTester histogram_tester;
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  static_cast<web_app::WebAppInstallManagerObserver*>(delegate.get())
      ->OnWebAppInstalledWithOsHooks(ash::kPasswordManagerAppId);

  EXPECT_THAT(histogram_tester.GetAllSamples("PasswordManager.ShortcutMetric"),
              base::BucketsAre(base::Bucket(1, 1)));

  static_cast<web_app::WebAppInstallManagerObserver*>(delegate.get())
      ->OnWebAppInstalledWithOsHooks("other_app_id");

  histogram_tester.ExpectUniqueSample("PasswordManager.ShortcutMetric", 1, 1);
}

TEST_F(PasswordsPrivateDelegateImplTest, GetPasskeyInGroups) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();

  sync_pb::WebauthnCredentialSpecifics passkey = CreatePasskey();
  passkey_model()->AddNewPasskeyForTesting(passkey);

  PasswordForm password =
      CreateSampleForm(PasswordForm::Store::kProfileStore, u"username1");
  SetUpPasswordStores({password});

  PasswordsPrivateDelegate::CredentialsGroups groups =
      delegate->GetCredentialGroups();
  EXPECT_EQ(1u, groups.size());
  EXPECT_EQ(2u, groups[0].entries.size());
  EXPECT_EQ("abc1.com", groups[0].name);
  EXPECT_EQ("https://abc1.com/favicon.ico", groups[0].icon_url);

  PasswordUiEntry expected_entry1;
  expected_entry1.affiliated_domains.emplace_back();
  expected_entry1.affiliated_domains.back().signon_realm = "https://abc1.com";
  expected_entry1.username = "username1";
  expected_entry1.stored_in = api::passwords_private::PasswordStoreSet::kDevice;
  PasswordUiEntry expected_entry2;
  expected_entry2.is_passkey = true;
  expected_entry2.affiliated_domains.emplace_back();
  expected_entry2.affiliated_domains.back().signon_realm = "https://abc1.com";
  expected_entry2.username = passkey.user_name();
  expected_entry2.display_name = passkey.user_display_name();
  expected_entry2.stored_in =
      api::passwords_private::PasswordStoreSet::kAccount;
  EXPECT_THAT(groups[0].entries,
              testing::UnorderedElementsAre(
                  PasswordUiEntryDataEquals(testing::ByRef(expected_entry1)),
                  PasswordUiEntryDataEquals(testing::ByRef(expected_entry2))));
}

TEST_F(PasswordsPrivateDelegateImplTest, RemovePasskey) {
  base::UserActionTester user_action_tester;
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();

  sync_pb::WebauthnCredentialSpecifics passkey = CreatePasskey();
  passkey_model()->AddNewPasskeyForTesting(std::move(passkey));
  SetUpPasswordStores({});

  PasswordsPrivateDelegate::CredentialsGroups groups =
      delegate->GetCredentialGroups();
  PasswordUiEntry& passkey_entry = groups.at(0).entries.at(0);
  ASSERT_TRUE(passkey_entry.is_passkey);
  EXPECT_EQ(user_action_tester.GetActionCount("PasswordManager_RemovePasskey"),
            0);

  delegate->RemoveCredential(passkey_entry.id, passkey_entry.stored_in);
  groups = delegate->GetCredentialGroups();
  EXPECT_TRUE(groups.empty());
  EXPECT_EQ(user_action_tester.GetActionCount("PasswordManager_RemovePasskey"),
            1);

  delegate->RemoveCredential(
      /*id=*/42, api::passwords_private::PasswordStoreSet::kAccount);
  EXPECT_EQ(user_action_tester.GetActionCount("PasswordManager_RemovePasskey"),
            1);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       RemovePasswordFromAccountStoreTracksRemovalReason) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();

  PasswordForm password =
      CreateSampleForm(PasswordForm::Store::kAccountStore, u"username");
  password.signon_realm = "https://facebook.com";
  password.url = GURL("https://facebook.com");

  SetUpPasswordStores({password});

  PasswordsPrivateDelegate::CredentialsGroups groups =
      delegate->GetCredentialGroups();
  PasswordUiEntry& password_entry = groups.at(0).entries.at(0);

  delegate->RemoveCredential(password_entry.id, password_entry.stored_in);

  EXPECT_EQ(prefs()->GetInteger(
                password_manager::prefs::kPasswordRemovalReasonForAccount),
            1 << static_cast<int>(
                password_manager::metrics_util::
                    PasswordManagerCredentialRemovalReason::kSettings));
  EXPECT_EQ(prefs()->GetInteger(
                password_manager::prefs::kPasswordRemovalReasonForProfile),
            0);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       RemovePasswordFromProfileStoreTracksRemovalReason) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();

  PasswordForm password =
      CreateSampleForm(PasswordForm::Store::kProfileStore, u"username");
  password.signon_realm = "https://facebook.com";
  password.url = GURL("https://facebook.com");

  SetUpPasswordStores({password});

  PasswordsPrivateDelegate::CredentialsGroups groups =
      delegate->GetCredentialGroups();
  PasswordUiEntry& password_entry = groups.at(0).entries.at(0);

  delegate->RemoveCredential(password_entry.id, password_entry.stored_in);

  EXPECT_EQ(prefs()->GetInteger(
                password_manager::prefs::kPasswordRemovalReasonForAccount),
            0);
  EXPECT_EQ(prefs()->GetInteger(
                password_manager::prefs::kPasswordRemovalReasonForProfile),
            1 << static_cast<int>(
                password_manager::metrics_util::
                    PasswordManagerCredentialRemovalReason::kSettings));
}

TEST_F(PasswordsPrivateDelegateImplTest, RemoveBackupPassword) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();

  PasswordForm password =
      CreateSampleForm(PasswordForm::Store::kProfileStore, u"username");
  password.signon_realm = "https://facebook.com";
  password.url = GURL("https://facebook.com");
  password.SetPasswordBackupNote(u"backup");

  SetUpPasswordStores({password});

  PasswordsPrivateDelegate::CredentialsGroups groups =
      delegate->GetCredentialGroups();
  PasswordUiEntry& password_entry = groups.at(0).entries.at(0);

  delegate->RemoveBackupPassword(password_entry.id);
  RunUntilIdle();

  EXPECT_THAT(password_manager::GetAllLoginsSync(account_store()),
              testing::SizeIs(0));
  ASSERT_THAT(password_manager::GetAllLoginsSync(profile_store()),
              testing::SizeIs(1));
  EXPECT_FALSE(password_manager::GetAllLoginsSync(profile_store())
                   .begin()
                   ->second.begin()
                   ->GetPasswordBackup()
                   .has_value());
}

TEST_F(PasswordsPrivateDelegateImplTest, SharePasswordWithTwoRecipients) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  PasswordForm password = CreateSampleForm();
  SetUpPasswordStores({password});

  PasswordsPrivateDelegate::ShareRecipients recipients;
  RecipientInfo recipient1;
  PublicKey public_key1;
  public_key1.value = kSharingRecipientKeyValue1;
  recipient1.public_key = std::move(public_key1);
  recipient1.user_id = kSharingRecipientId1;
  recipient1.display_name = kSharingRecipientDisplayName1;
  recipient1.email = kSharingRecipientEmail1;
  recipient1.profile_image_url = kSharingRecipientProfileImageUrl1;
  recipients.push_back(std::move(recipient1));

  RecipientInfo recipient2;
  PublicKey public_key2;
  public_key2.value = kSharingRecipientKeyValue2;
  recipient2.public_key = std::move(public_key2);
  recipient2.user_id = kSharingRecipientId2;
  recipient2.display_name = kSharingRecipientDisplayName2;
  recipient2.email = kSharingRecipientEmail2;
  recipient2.profile_image_url = kSharingRecipientProfileImageUrl2;
  recipients.push_back(std::move(recipient2));

  password_manager::PublicKey expected_public_key1, expected_public_key2;
  expected_public_key1.key = kSharingRecipientKeyValue1;
  expected_public_key2.key = kSharingRecipientKeyValue2;

  EXPECT_CALL(
      *password_sender_service(),
      SendPasswords(
          ElementsAre(AllOf(
              Field(&PasswordForm::username_value, password.username_value),
              Field(&PasswordForm::password_value, password.password_value),
              Field(&PasswordForm::signon_realm, password.signon_realm))),
          AllOf(Field("user id", &PasswordRecipient::user_id,
                      kSharingRecipientId1),
                Field("public key", &PasswordRecipient::public_key,
                      expected_public_key1))));
  EXPECT_CALL(
      *password_sender_service(),
      SendPasswords(
          ElementsAre(AllOf(
              Field(&PasswordForm::username_value, password.username_value),
              Field(&PasswordForm::password_value, password.password_value),
              Field(&PasswordForm::signon_realm, password.signon_realm))),
          AllOf(Field("user id", &PasswordRecipient::user_id,
                      kSharingRecipientId2),
                Field("public key", &PasswordRecipient::public_key,
                      expected_public_key2))));

  delegate->SharePassword(/*id=*/0, recipients);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       ShareAllPasswordsRepresentedByUiEntry) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  PasswordForm password1 =
      CreateSampleForm(PasswordForm::Store::kProfileStore, u"username1");
  password1.signon_realm = "https://facebook.com";
  password1.url = GURL("https://facebook.com");

  PasswordForm password2 = password1;
  password2.signon_realm = "https://m.facebook.com";
  password2.url = GURL("https://m.facebook.com");

  PasswordForm password3 =
      CreateSampleForm(PasswordForm::Store::kProfileStore, u"username3");

  SetUpPasswordStores({password1, password2, password3});

  PasswordsPrivateDelegate::CredentialsGroups groups =
      delegate->GetCredentialGroups();
  ASSERT_EQ(groups.size(), 2U);
  int id_with_two_affiliated_domains = -1;
  for (const api::passwords_private::CredentialGroup& group : groups) {
    for (const PasswordUiEntry& entry : group.entries) {
      if (entry.affiliated_domains.size() == 2) {
        id_with_two_affiliated_domains = entry.id;
        break;
      }
    }
  }
  ASSERT_NE(-1, id_with_two_affiliated_domains);

  PasswordsPrivateDelegate::ShareRecipients recipients;
  RecipientInfo recipient;
  PublicKey public_key;
  public_key.value = kSharingRecipientKeyValue1;
  recipient.public_key = std::move(public_key);
  recipient.user_id = kSharingRecipientId1;
  recipient.display_name = kSharingRecipientDisplayName1;
  recipient.email = kSharingRecipientEmail1;
  recipient.profile_image_url = kSharingRecipientProfileImageUrl1;
  recipients.push_back(std::move(recipient));

  password_manager::PublicKey expected_public_key;
  expected_public_key.key = kSharingRecipientKeyValue1;

  EXPECT_CALL(
      *password_sender_service(),
      SendPasswords(
          UnorderedElementsAre(
              Field(&PasswordForm::signon_realm, "https://facebook.com"),
              Field(&PasswordForm::signon_realm, "https://m.facebook.com")),
          AllOf(Field("user id", &PasswordRecipient::user_id,
                      kSharingRecipientId1),
                Field("public key", &PasswordRecipient::public_key,
                      expected_public_key))))
      .Times(1);

  delegate->SharePassword(/*id=*/id_with_two_affiliated_domains, recipients);
}

TEST_F(PasswordsPrivateDelegateImplTest, ShareNonExistentPassword) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();

  PasswordsPrivateDelegate::ShareRecipients recipients;
  RecipientInfo recipient;
  recipient.user_id = kSharingRecipientId1;
  recipients.push_back(std::move(recipient));

  EXPECT_CALL(*password_sender_service(), SendPasswords).Times(0);

  delegate->SharePassword(/*id=*/100, recipients);
}

TEST_F(PasswordsPrivateDelegateImplTest, DisconnectCloudAuthenticator) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();

  EXPECT_CALL(*enclave_manager(), Unenroll).Times(1);

  delegate->DisconnectCloudAuthenticator(
      base::BindLambdaForTesting([](bool success) { EXPECT_TRUE(success); }));
}

TEST_F(PasswordsPrivateDelegateImplTest, IsConnectedToCloudAuthenticator) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();

  EXPECT_CALL(*enclave_manager(), IsRegistered).Times(1);

  delegate->IsConnectedToCloudAuthenticator();
}

class PasswordsPrivateDelegateImplFetchFamilyMembersTest
    : public PasswordsPrivateDelegateImplTest {
 public:
  PasswordsPrivateDelegateImplFetchFamilyMembersTest() {
    delegate_ = CreateDelegate();
    delegate_->SetRecipientsFetcherForTesting(
        std::make_unique<password_manager::RecipientsFetcherImpl>(
            version_info::Channel::DEFAULT,
            url_loader_factory().GetSafeWeakWrapper(),
            identity_test_env()->identity_manager()));
    identity_test_env()->MakePrimaryAccountAvailable(
        "test@email.com", signin::ConsentLevel::kSignin);
    identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  }

  ~PasswordsPrivateDelegateImplFetchFamilyMembersTest() override {
    if (delegate_) {
      delegate_->SetRecipientsFetcherForTesting(nullptr);
    }
    delegate_ = nullptr;
  }

  void SetServerResponse(sync_pb::PasswordSharingRecipientsResponse::
                             PasswordSharingRecipientsResult result,
                         net::HttpStatusCode status = net::HTTP_OK,
                         bool recipient_has_public_key = false) {
    sync_pb::PasswordSharingRecipientsResponse response;
    response.set_result(result);
    if (result == sync_pb::PasswordSharingRecipientsResponse::SUCCESS) {
      sync_pb::UserInfo* user_info = response.add_recipients();
      user_info->set_user_id(kTestUserId);
      user_info->mutable_user_display_info()->set_display_name(kTestUserName);
      user_info->mutable_user_display_info()->set_email(kTestEmail);
      user_info->mutable_user_display_info()->set_profile_image_url(
          kTestProfileImageUrl);
      if (recipient_has_public_key) {
        const password_manager::PublicKey kTestPublicKey = {
            kTestPublicKeyBase64, kTestPublicKeyVersion};
        user_info->mutable_cross_user_sharing_public_key()->CopyFrom(
            kTestPublicKey.ToProto());
      }
    }
    url_loader_factory().AddResponse(
        password_manager::PasswordSharingRecipientsDownloader::
            GetPasswordSharingRecipientsURL(version_info::Channel::DEFAULT)
                .spec(),
        response.SerializeAsString(), status);
  }

  PasswordsPrivateDelegateImpl* delegate() { return delegate_.get(); }

 private:
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate_;
};

TEST_F(PasswordsPrivateDelegateImplFetchFamilyMembersTest,
       FetchFamilyMembersSucceedsWithoutPublicKey) {
  SetServerResponse(sync_pb::PasswordSharingRecipientsResponse::SUCCESS);

  base::MockCallback<PasswordsPrivateDelegate::FetchFamilyResultsCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(AllOf(Field(&FamilyFetchResults::status,
                      api::passwords_private::FamilyFetchStatus::kSuccess),
                Field(&FamilyFetchResults::family_members,
                      ElementsAre(AllOf(
                          Field(&RecipientInfo::user_id, kTestUserId),
                          Field(&RecipientInfo::display_name, kTestUserName),
                          Field(&RecipientInfo::email, kTestEmail),
                          Field(&RecipientInfo::is_eligible, false),
                          Field(&RecipientInfo::public_key, Eq(std::nullopt)),
                          Field(&RecipientInfo::profile_image_url,
                                kTestProfileImageUrl)))))));

  delegate()->FetchFamilyMembers(callback.Get());
  RunUntilIdle();
}

TEST_F(PasswordsPrivateDelegateImplFetchFamilyMembersTest,
       FetchFamilyMembersSucceedsWithPublicKey) {
  SetServerResponse(sync_pb::PasswordSharingRecipientsResponse::SUCCESS,
                    net::HTTP_OK, /*recipient_has_public_key=*/true);

  base::MockCallback<PasswordsPrivateDelegate::FetchFamilyResultsCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(AllOf(Field(&FamilyFetchResults::status,
                      api::passwords_private::FamilyFetchStatus::kSuccess),
                Field(&FamilyFetchResults::family_members,
                      ElementsAre(AllOf(
                          Field(&RecipientInfo::user_id, kTestUserId),
                          Field(&RecipientInfo::display_name, kTestUserName),
                          Field(&RecipientInfo::email, kTestEmail),
                          Field(&RecipientInfo::is_eligible, true),
                          Field(&RecipientInfo::public_key,
                                Optional(AllOf(Field(&PublicKey::value,
                                                     kTestPublicKeyBase64),
                                               Field(&PublicKey::version,
                                                     kTestPublicKeyVersion)))),
                          Field(&RecipientInfo::profile_image_url,
                                kTestProfileImageUrl)))))));

  delegate()->FetchFamilyMembers(callback.Get());
  RunUntilIdle();
}

TEST_F(PasswordsPrivateDelegateImplFetchFamilyMembersTest,
       FetchFamilyMembersFailsWithUnknownError) {
  SetServerResponse(sync_pb::PasswordSharingRecipientsResponse::UNKNOWN);

  base::MockCallback<PasswordsPrivateDelegate::FetchFamilyResultsCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(AllOf(Field(&FamilyFetchResults::status,
                      api::passwords_private::FamilyFetchStatus::kUnknownError),
                Field(&FamilyFetchResults::family_members, IsEmpty()))));

  delegate()->FetchFamilyMembers(callback.Get());
  RunUntilIdle();
}

TEST_F(PasswordsPrivateDelegateImplFetchFamilyMembersTest,
       FetchFamilyMembersFailsWithNoFamilyMembersError) {
  SetServerResponse(
      sync_pb::PasswordSharingRecipientsResponse::NOT_FAMILY_MEMBER);

  base::MockCallback<PasswordsPrivateDelegate::FetchFamilyResultsCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(AllOf(Field(&FamilyFetchResults::status,
                      api::passwords_private::FamilyFetchStatus::kNoMembers),
                Field(&FamilyFetchResults::family_members, IsEmpty()))));

  delegate()->FetchFamilyMembers(callback.Get());
  RunUntilIdle();
}

TEST_F(PasswordsPrivateDelegateImplFetchFamilyMembersTest,
       FetchFamilyMembersFailsWithAnotherRequestInFlight) {
  base::MockCallback<PasswordsPrivateDelegate::FetchFamilyResultsCallback>
      callback1;
  delegate()->FetchFamilyMembers(callback1.Get());

  base::MockCallback<PasswordsPrivateDelegate::FetchFamilyResultsCallback>
      callback2;
  EXPECT_CALL(
      callback2,
      Run(AllOf(Field(&FamilyFetchResults::status,
                      api::passwords_private::FamilyFetchStatus::kUnknownError),
                Field(&FamilyFetchResults::family_members, IsEmpty()))));
  delegate()->FetchFamilyMembers(callback2.Get());

  RunUntilIdle();
}

TEST_F(PasswordsPrivateDelegateImplFetchFamilyMembersTest,
       FetchFamilyMembersFailsWithNetworkError) {
  url_loader_factory().AddResponse(
      password_manager::PasswordSharingRecipientsDownloader::
          GetPasswordSharingRecipientsURL(version_info::Channel::DEFAULT)
              .spec(),
      /*content=*/std::string(), net::HTTP_INTERNAL_SERVER_ERROR);

  base::MockCallback<PasswordsPrivateDelegate::FetchFamilyResultsCallback>
      callback;
  EXPECT_CALL(
      callback,
      Run(AllOf(Field(&FamilyFetchResults::status,
                      api::passwords_private::FamilyFetchStatus::kUnknownError),
                Field(&FamilyFetchResults::family_members, IsEmpty()))));

  delegate()->FetchFamilyMembers(callback.Get());
  RunUntilIdle();
}

TEST_F(PasswordsPrivateDelegateImplTest, GetCredentialGroups_SyncOn) {
  sync_service()->SetSignedIn(signin::ConsentLevel::kSync);

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();

  PasswordForm password =
      CreateSampleForm(PasswordForm::Store::kProfileStore, u"username2");

  SetUpPasswordStores({password});

  PasswordsPrivateDelegate::CredentialsGroups groups =
      delegate->GetCredentialGroups();
  EXPECT_EQ(1u, groups.size());
  EXPECT_EQ(1u, groups[0].entries.size());
  EXPECT_EQ("abc1.com", groups[0].name);
  EXPECT_EQ(
      "https://t1.gstatic.com/"
      "faviconV2?client=PASSWORD_MANAGER&type=FAVICON&fallback_opts=TYPE,SIZE,"
      "URL,TOP_DOMAIN&size=32&url=https%3A%2F%2Fabc1.com%2F",
      groups[0].icon_url);
}

TEST_F(PasswordsPrivateDelegateImplTest, GetCredentialGroups_SyncOff) {
  sync_service()->SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service()->SetSignedOut();

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();

  PasswordForm password =
      CreateSampleForm(PasswordForm::Store::kProfileStore, u"username2");

  SetUpPasswordStores({password});

  PasswordsPrivateDelegate::CredentialsGroups groups =
      delegate->GetCredentialGroups();
  EXPECT_EQ(1u, groups.size());
  EXPECT_EQ(1u, groups[0].entries.size());
  EXPECT_EQ("abc1.com", groups[0].name);
  EXPECT_EQ("https://abc1.com/favicon.ico", groups[0].icon_url);
}

TEST_F(PasswordsPrivateDelegateImplTest, GetCredentialGroups_Butter) {
  identity_test_env()->MakePrimaryAccountAvailable(
      "test@email.com", signin::ConsentLevel::kSignin);
  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();

  PasswordForm password =
      CreateSampleForm(PasswordForm::Store::kAccountStore, u"username2");

  SetUpPasswordStores({password});

  PasswordsPrivateDelegate::CredentialsGroups groups =
      delegate->GetCredentialGroups();
  EXPECT_EQ(1u, groups.size());
  EXPECT_EQ(1u, groups[0].entries.size());
  EXPECT_EQ("abc1.com", groups[0].name);
  EXPECT_EQ(
      "https://t1.gstatic.com/"
      "faviconV2?client=PASSWORD_MANAGER&type=FAVICON&fallback_opts=TYPE,SIZE,"
      "URL,TOP_DOMAIN&size=32&url=https%3A%2F%2Fabc1.com%2F",
      groups[0].icon_url);
}

TEST_F(PasswordsPrivateDelegateImplTest, DeleteAllData) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  PasswordForm form_profile =
      CreateSampleForm(PasswordForm::Store::kProfileStore);
  PasswordForm form_account =
      CreateSampleForm(PasswordForm::Store::kAccountStore);
  SetUpPasswordStores({form_profile, form_account});
  RunUntilIdle();

  sync_pb::WebauthnCredentialSpecifics passkey = CreatePasskey();
  passkey_model()->AddNewPasskeyForTesting(passkey);

  EXPECT_THAT(password_manager::GetAllLoginsSync(profile_store()),
              testing::SizeIs(1));
  EXPECT_THAT(password_manager::GetAllLoginsSync(account_store()),
              testing::SizeIs(1));
  EXPECT_THAT(passkey_model()->GetPasskeys(
                  webauthn::PasskeyModel::AnyRp(),
                  webauthn::PasskeyModel::ShadowedCredentials::kInclude),
              SizeIs(1));

  ExpectAuthentication(delegate, /*successful=*/true);
  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(true));
  delegate->DeleteAllPasswordManagerData(callback.Get());
  RunUntilIdle();
  EXPECT_THAT(password_manager::GetAllLoginsSync(profile_store()),
              testing::IsEmpty());
  EXPECT_THAT(password_manager::GetAllLoginsSync(account_store()),
              testing::IsEmpty());
  EXPECT_THAT(passkey_model()->GetPasskeys(
                  webauthn::PasskeyModel::AnyRp(),
                  webauthn::PasskeyModel::ShadowedCredentials::kInclude),
              IsEmpty());
}

TEST_F(PasswordsPrivateDelegateImplTest,
       DeleteAllDataRecordsPasswordRemovalReason) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();

  ExpectAuthentication(delegate, /*successful=*/true);
  base::test::TestFuture<bool> completion_future;
  delegate->DeleteAllPasswordManagerData(completion_future.GetCallback());
  ASSERT_TRUE(completion_future.Take());

  int expected_reason =
      1 << static_cast<int>(password_manager::metrics_util::
                                PasswordManagerCredentialRemovalReason::
                                    kDeleteAllPasswordManagerData);
  EXPECT_EQ(prefs()->GetInteger(
                password_manager::prefs::kPasswordRemovalReasonForAccount),
            expected_reason);
  EXPECT_EQ(prefs()->GetInteger(
                password_manager::prefs::kPasswordRemovalReasonForProfile),
            expected_reason);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
TEST_F(PasswordsPrivateDelegateImplTest, DeleteAllDataWithReauthFailed) {
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();
  PasswordForm form_profile =
      CreateSampleForm(PasswordForm::Store::kProfileStore);
  PasswordForm form_account =
      CreateSampleForm(PasswordForm::Store::kAccountStore);
  SetUpPasswordStores({form_profile, form_account});
  RunUntilIdle();

  sync_pb::WebauthnCredentialSpecifics passkey = CreatePasskey();
  passkey_model()->AddNewPasskeyForTesting(passkey);
  RunUntilIdle();

  ExpectAuthentication(delegate, /*successful=*/false);
  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(false));
  delegate->DeleteAllPasswordManagerData(callback.Get());
  RunUntilIdle();
  EXPECT_THAT(password_manager::GetAllLoginsSync(profile_store()),
              testing::SizeIs(1));
  EXPECT_THAT(password_manager::GetAllLoginsSync(account_store()),
              testing::SizeIs(1));
  EXPECT_THAT(passkey_model()->GetPasskeys(
                  webauthn::PasskeyModel::AnyRp(),
                  webauthn::PasskeyModel::ShadowedCredentials::kInclude),
              SizeIs(1));
}
#endif

TEST_F(PasswordsPrivateDelegateImplTest, GetSavedPasswordsListMapsApcField) {
  PasswordForm sample_form = CreateSampleForm();
  SetUpPasswordStores({sample_form});
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();

  const PasswordsPrivateDelegate::UiEntries& credentials =
      GetCredentials(*delegate);
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_FALSE(credentials.at(0).is_automatic_password_change_supported);
}

}  // namespace
}  // namespace extensions
