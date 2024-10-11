// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_impl.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "ash/constants/web_app_id_constants.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/password_manager/password_sender_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/passwords/settings/password_manager_porter_interface.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/browser/webapps/webapps_client_desktop.h"
#include "chrome/browser/webauthn/change_pin_controller.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/enclave_manager_interface.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/device_reauth/device_reauth_metrics_util.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/content/browser/password_manager_log_router_factory.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/import/import_results.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/sharing/mock_password_sender_service.h"
#include "components/password_manager/core/browser/sharing/password_sharing_recipients_downloader.h"
#include "components/password_manager/core/browser/sharing/recipients_fetcher_impl.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/password_sharing_recipients.pb.h"
#include "components/sync/test/test_sync_service.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/test_event_router.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/test/test_clipboard.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#endif

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
using ::testing::StrictMock;

namespace extensions {

using api::passwords_private::FamilyFetchResults;
using api::passwords_private::ImportResults;
using api::passwords_private::PasswordUiEntry;
using api::passwords_private::PublicKey;
using api::passwords_private::RecipientInfo;
using api::passwords_private::UrlCollection;

namespace {

constexpr char kHistogramName[] = "PasswordManager.AccessPasswordInSettings";
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

using MockPlaintextPasswordCallback =
    base::MockCallback<PasswordsPrivateDelegate::PlaintextPasswordCallback>;
using MockRequestCredentialsDetailsCallback =
    base::MockCallback<PasswordsPrivateDelegate::UiEntriesCallback>;

class MockPasswordManagerPorter : public PasswordManagerPorterInterface {
 public:
  MOCK_METHOD(bool,
              Export,
              (base::WeakPtr<content::WebContents> web_contents),
              (override));
  MOCK_METHOD(void, CancelExport, (), (override));
  MOCK_METHOD(password_manager::ExportProgressStatus,
              GetExportProgressStatus,
              (),
              (override));
  MOCK_METHOD(void,
              Import,
              (content::WebContents * web_contents,
               PasswordForm::Store to_store,
               ImportResultsCallback results_callback),
              (override));
  MOCK_METHOD(void,
              ContinueImport,
              (const std::vector<int>& selected_ids,
               ImportResultsCallback results_callback),
              (override));
  MOCK_METHOD(void, ResetImporter, (bool delete_file), (override));
};

class MockChangePinController : public ChangePinController {
 public:
  MOCK_METHOD(void,
              IsChangePinFlowAvailable,
              (base::OnceCallback<void(bool)> pin_available_callback),
              (override));
  MOCK_METHOD(void,
              StartChangePin,
              (base::OnceCallback<void(bool)>),
              (override));
};

class FakePasswordManagerPorter : public PasswordManagerPorterInterface {
 public:
  bool Export(base::WeakPtr<content::WebContents> web_contents) override {
    return true;
  }

  void CancelExport() override {}

  password_manager::ExportProgressStatus GetExportProgressStatus() override {
    return password_manager::ExportProgressStatus::kSucceeded;
  }

  void Import(content::WebContents* web_contents,
              PasswordForm::Store to_store,
              ImportResultsCallback results_callback) override {
    password_manager::ImportResults results;
    results.status = import_results_status_;
    // For consistency |results_callback| is always run asynchronously.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(results_callback), results));
  }

  void ContinueImport(const std::vector<int>& selected_ids,
                      ImportResultsCallback results_callback) override {
    password_manager::ImportResults results;
    results.status = import_results_status_;
    // For consistency |results_callback| is always run asynchronously.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(results_callback), results));
  }

  void ResetImporter(bool delete_file) override {}

  void set_import_result_status(
      password_manager::ImportResults::Status status) {
    import_results_status_ = status;
  }

 private:
  password_manager::ImportResults::Status import_results_status_ =
      password_manager::ImportResults::Status::SUCCESS;
};

class MockPasswordManagerClient : public ChromePasswordManagerClient {
 public:
  // Creates the mock and attaches it to |web_contents|.
  static MockPasswordManagerClient* CreateForWebContentsAndGet(
      content::WebContents* web_contents);

  ~MockPasswordManagerClient() override = default;

  // ChromePasswordManagerClient overrides.
  MOCK_METHOD(void,
              TriggerReauthForPrimaryAccount,
              (signin_metrics::ReauthAccessPoint,
               base::OnceCallback<void(ReauthSucceeded)>),
              (override));
  const password_manager::MockPasswordFeatureManager*
  GetPasswordFeatureManager() const override {
    return &mock_password_feature_manager_;
  }

  password_manager::MockPasswordFeatureManager* GetPasswordFeatureManager() {
    return &mock_password_feature_manager_;
  }

 private:
  explicit MockPasswordManagerClient(content::WebContents* web_contents)
      : ChromePasswordManagerClient(web_contents) {}

  password_manager::MockPasswordFeatureManager mock_password_feature_manager_;
};

class MockEnclaveManager : public EnclaveManagerInterface {
 public:
  MockEnclaveManager() = default;
  ~MockEnclaveManager() override = default;
  MockEnclaveManager(const MockEnclaveManager&) = delete;
  MockEnclaveManager& operator=(const MockEnclaveManager&) = delete;

  MOCK_METHOD(void, Unenroll, (Callback), (override));
  MOCK_METHOD(bool, is_registered, (), (const override));
};

// static
MockPasswordManagerClient*
MockPasswordManagerClient::CreateForWebContentsAndGet(
    content::WebContents* web_contents) {
  // Avoid creation of log router.
  password_manager::PasswordManagerLogRouterFactory::GetInstance()
      ->SetTestingFactory(
          web_contents->GetBrowserContext(),
          base::BindRepeating(
              [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                return nullptr;
              }));
  auto* mock_client = new MockPasswordManagerClient(web_contents);
  web_contents->SetUserData(UserDataKey(), base::WrapUnique(mock_client));
  return mock_client;
}

void SetUpSyncInTransportMode(Profile* profile) {
  auto* sync_service = static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile,
          base::BindRepeating(
              [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                return std::make_unique<syncer::TestSyncService>();
              })));
  sync_service->SetSignedIn(signin::ConsentLevel::kSignin);
  ASSERT_FALSE(sync_service->IsSyncFeatureEnabled());
}

class PasswordEventObserver
    : public extensions::TestEventRouter::EventObserver {
 public:
  // The observer will only listen to events with the |event_name|.
  explicit PasswordEventObserver(const std::string& event_name);

  PasswordEventObserver(const PasswordEventObserver&) = delete;
  PasswordEventObserver& operator=(const PasswordEventObserver&) = delete;

  ~PasswordEventObserver() override;

  // Removes |event_args_| from |*this| and returns them.
  base::Value PassEventArgs();

  // extensions::TestEventRouter::EventObserver:
  void OnBroadcastEvent(const extensions::Event& event) override;

 private:
  // The name of the observed event.
  const std::string event_name_;

  // The arguments passed for the last observed event.
  base::Value event_args_;
};

PasswordEventObserver::PasswordEventObserver(const std::string& event_name)
    : event_name_(event_name) {}

PasswordEventObserver::~PasswordEventObserver() = default;

base::Value PasswordEventObserver::PassEventArgs() {
  return std::move(event_args_);
}

void PasswordEventObserver::OnBroadcastEvent(const extensions::Event& event) {
  if (event.event_name != event_name_) {
    return;
  }
  event_args_ = base::Value(event.event_args.Clone());
}

std::unique_ptr<KeyedService> BuildPasswordsPrivateEventRouter(
    content::BrowserContext* context) {
  return std::make_unique<PasswordsPrivateEventRouter>(context);
}

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

}  // namespace

class PasswordsPrivateDelegateImplTest : public WebAppTest {
 public:
  PasswordsPrivateDelegateImplTest()
      : WebAppTest(WebAppTest::WithTestUrlLoaderFactory()) {}

  PasswordsPrivateDelegateImplTest(const PasswordsPrivateDelegateImplTest&) =
      delete;
  PasswordsPrivateDelegateImplTest& operator=(
      const PasswordsPrivateDelegateImplTest&) = delete;

  ~PasswordsPrivateDelegateImplTest() override;

  void SetUp() override;

  // Sets up a testing password store and fills it with |forms|.
  void SetUpPasswordStores(std::vector<PasswordForm> forms);

  // Sets up a testing EventRouter with a production
  // PasswordsPrivateEventRouter.
  void SetUpRouters();

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  scoped_refptr<PasswordsPrivateDelegateImpl> CreateDelegate() {
    return new PasswordsPrivateDelegateImpl(profile());
  }

  // Queries and returns the list of saved credentials, blocking until finished.
  PasswordsPrivateDelegate::UiEntries GetCredentials(
      PasswordsPrivateDelegate& delegate);

  // Returns a test `WebContents` with an initialized Autofill client, which is
  // needed for PasswordManager client to work properly.
  std::unique_ptr<content::WebContents> CreateWebContents() {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile(),
                                                          /*instance=*/nullptr);
    autofill::ChromeAutofillClient::CreateForWebContents(web_contents.get());
    return web_contents;
  }

  syncer::TestSyncService* sync_service();

 protected:
  raw_ptr<extensions::TestEventRouter, DanglingUntriaged> event_router_ =
      nullptr;
  scoped_refptr<TestPasswordStore> profile_store_;
  scoped_refptr<TestPasswordStore> account_store_;
  raw_ptr<ui::TestClipboard, DanglingUntriaged> test_clipboard_;
  MockChangePinController change_pin_controller_;

 private:
  base::HistogramTester histogram_tester_;
};

PasswordsPrivateDelegateImplTest::~PasswordsPrivateDelegateImplTest() {
  ui::Clipboard::DestroyClipboardForCurrentThread();
}

void PasswordsPrivateDelegateImplTest::SetUp() {
  WebAppTest::SetUp();
  profile_store_ = CreateAndUseTestPasswordStore(profile());
  account_store_ = CreateAndUseTestAccountPasswordStore(profile());
  test_clipboard_ = ui::TestClipboard::CreateForCurrentThread();
  AffiliationServiceFactory::GetInstance()->SetTestingSubclassFactoryAndUse(
      profile(), base::BindOnce([](content::BrowserContext*) {
        return std::make_unique<affiliations::FakeAffiliationService>();
      }));
  SetUpRouters();
  SetUpSyncInTransportMode(profile());
  PasskeyModelFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(),
      base::BindRepeating(
          [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return std::make_unique<webauthn::TestPasskeyModel>();
          }));

  PasswordSenderServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), base::BindRepeating([](content::BrowserContext*)
                                         -> std::unique_ptr<KeyedService> {
        return std::make_unique<password_manager::MockPasswordSenderService>();
      }));
  ChangePinController::set_instance_for_testing(&change_pin_controller_);

  EnclaveManagerFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(),
      base::BindRepeating(
          [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return std::make_unique<MockEnclaveManager>();
          }));
}

void PasswordsPrivateDelegateImplTest::SetUpPasswordStores(
    std::vector<PasswordForm> forms) {
  for (const PasswordForm& form : forms) {
    if (form.IsUsingAccountStore()) {
      account_store_->AddLogin(form);
    } else if (form.IsUsingProfileStore()) {
      profile_store_->AddLogin(form);
    } else {
      NOTREACHED_IN_MIGRATION() << "Store not set";
    }
  }
  // Spin the loop to allow PasswordStore tasks being processed.
  base::RunLoop().RunUntilIdle();
}

void PasswordsPrivateDelegateImplTest::SetUpRouters() {
  event_router_ = extensions::CreateAndUseTestEventRouter(profile());
  // Set the production PasswordsPrivateEventRouter::Create as a testing
  // factory, because at some point during the preceding initialization, a null
  // factory is set, resulting in nul PasswordsPrivateEventRouter.
  PasswordsPrivateEventRouterFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildPasswordsPrivateEventRouter));
}

PasswordsPrivateDelegate::UiEntries
PasswordsPrivateDelegateImplTest::GetCredentials(
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

syncer::TestSyncService* PasswordsPrivateDelegateImplTest::sync_service() {
  return static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetForProfile(profile()));
}

TEST_F(PasswordsPrivateDelegateImplTest, GetSavedPasswordsList) {
  auto delegate = CreateDelegate();

  base::MockCallback<PasswordsPrivateDelegate::UiEntriesCallback> callback;
  EXPECT_CALL(callback, Run).Times(0);
  delegate->GetSavedPasswordsList(callback.Get());

  EXPECT_CALL(callback, Run);
  SetUpPasswordStores({});

  EXPECT_CALL(callback, Run);
  delegate->GetSavedPasswordsList(callback.Get());
}

TEST_F(PasswordsPrivateDelegateImplTest,
       PasswordsDuplicatedInStoresAreRepresentedAsSingleEntity) {
  auto delegate = CreateDelegate();

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
  auto delegate = CreateDelegate();

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
  auto delegate = CreateDelegate();
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
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  auto* client =
      MockPasswordManagerClient::CreateForWebContentsAndGet(web_contents.get());
  ON_CALL(*(client->GetPasswordFeatureManager()), IsOptedInForAccountStorage)
      .WillByDefault(Return(false));
  auto delegate = CreateDelegate();
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  // Double check that the contents of the passwords list matches our
  // expectation.
  base::MockCallback<PasswordsPrivateDelegate::UiEntriesCallback> callback;
  EXPECT_CALL(callback, Run(SizeIs(0)));
  delegate->GetSavedPasswordsList(callback.Get());

  EXPECT_TRUE(
      delegate->AddPassword(/*url=*/"example1.com", /*username=*/u"username1",
                            /*password=*/u"password1", /*note=*/u"",
                            /*use_account_store=*/true, web_contents.get()));
  EXPECT_TRUE(delegate->AddPassword(
      /*url=*/"http://example2.com/login?param=value",
      /*username=*/u"", /*password=*/u"password2", /*note=*/u"note",
      /*use_account_store=*/false, web_contents.get()));
  // Spin the loop to allow PasswordStore tasks posted when adding the
  // password to be completed.
  base::RunLoop().RunUntilIdle();

  // Check that adding passwords got reflected in the passwords list.
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

TEST_F(PasswordsPrivateDelegateImplTest, AddPasswordUpdatesDefaultStore) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  auto* client =
      MockPasswordManagerClient::CreateForWebContentsAndGet(web_contents.get());
  auto delegate = CreateDelegate();

  // NOT update default store if not opted-in for account storage.
  ON_CALL(*(client->GetPasswordFeatureManager()), IsOptedInForAccountStorage)
      .WillByDefault(Return(false));
  EXPECT_CALL(*(client->GetPasswordFeatureManager()), SetDefaultPasswordStore)
      .Times(0);
  EXPECT_TRUE(
      delegate->AddPassword("example1.com", u"username1", u"password1", u"",
                            /*use_account_store=*/false, web_contents.get()));

  // Updates the default store if opted-in and operation succeeded.
  ON_CALL(*(client->GetPasswordFeatureManager()), IsOptedInForAccountStorage)
      .WillByDefault(Return(true));
  EXPECT_TRUE(
      delegate->AddPassword("example2.com", u"username2", u"password2", u"",
                            /*use_account_store=*/true, web_contents.get()));

  // NOT update default store if opted-in, but operation failed.
  EXPECT_CALL(*(client->GetPasswordFeatureManager()), SetDefaultPasswordStore)
      .Times(0);
  EXPECT_FALSE(delegate->AddPassword("", u"", u"", u"",
                                     /*use_account_store=*/false,
                                     web_contents.get()));
}

TEST_F(PasswordsPrivateDelegateImplTest, AddPasswordDoesNotUpdateDefaultStore) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  auto* client =
      MockPasswordManagerClient::CreateForWebContentsAndGet(web_contents.get());
  auto delegate = CreateDelegate();

  ON_CALL(*(client->GetPasswordFeatureManager()), IsOptedInForAccountStorage)
      .WillByDefault(Return(true));
  EXPECT_CALL(*(client->GetPasswordFeatureManager()), SetDefaultPasswordStore)
      .Times(0);
  EXPECT_TRUE(
      delegate->AddPassword("example2.com", u"username2", u"password2", u"",
                            /*use_account_store=*/true, web_contents.get()));
}

TEST_F(PasswordsPrivateDelegateImplTest,
       ImportPasswordsDoesNotUpdateDefaultStore) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  auto* client =
      MockPasswordManagerClient::CreateForWebContentsAndGet(web_contents.get());
  auto delegate = CreateDelegate();

  auto mock_porter = std::make_unique<MockPasswordManagerPorter>();
  auto* mock_porter_ptr = mock_porter.get();

  delegate->SetPorterForTesting(std::move(mock_porter));

  ON_CALL(*(client->GetPasswordFeatureManager()), IsOptedInForAccountStorage)
      .WillByDefault(Return(true));
  EXPECT_CALL(*(client->GetPasswordFeatureManager()), SetDefaultPasswordStore)
      .Times(0);
  EXPECT_CALL(*mock_porter_ptr, Import).Times(1);
  delegate->ImportPasswords(api::passwords_private::PasswordStoreSet::kDevice,
                            base::DoNothing(), web_contents.get());
}

TEST_F(PasswordsPrivateDelegateImplTest,
       ImportPasswordsDoesntUpdateDefaultStore) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  auto* client =
      MockPasswordManagerClient::CreateForWebContentsAndGet(web_contents.get());
  auto delegate = CreateDelegate();

  auto mock_porter = std::make_unique<MockPasswordManagerPorter>();
  auto* mock_porter_ptr = mock_porter.get();

  delegate->SetPorterForTesting(std::move(mock_porter));

  // Updates the default store if opted-in and operation succeeded.
  ON_CALL(*(client->GetPasswordFeatureManager()), IsOptedInForAccountStorage)
      .WillByDefault(Return(true));
  EXPECT_CALL(*(client->GetPasswordFeatureManager()),
              SetDefaultPasswordStore(_))
      .Times(0);
  EXPECT_CALL(*mock_porter_ptr, Import).Times(1);
  delegate->ImportPasswords(api::passwords_private::PasswordStoreSet::kAccount,
                            base::DoNothing(), web_contents.get());
}

TEST_F(PasswordsPrivateDelegateImplTest,
       ImportPasswordsLogsImportResultsStatus) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  auto* client =
      MockPasswordManagerClient::CreateForWebContentsAndGet(web_contents.get());
  auto delegate = CreateDelegate();

  auto fake_porter = std::make_unique<FakePasswordManagerPorter>();
  auto* fake_porter_ptr = fake_porter.get();
  delegate->SetPorterForTesting(std::move(fake_porter));

  ON_CALL(*(client->GetPasswordFeatureManager()), IsOptedInForAccountStorage)
      .WillByDefault(Return(false));

  const auto kExpectedStatus =
      password_manager::ImportResults::Status::BAD_FORMAT;
  fake_porter_ptr->set_import_result_status(kExpectedStatus);

  base::MockCallback<PasswordsPrivateDelegate::ImportResultsCallback> callback;
  EXPECT_CALL(callback,
              Run(::testing::Field(
                  &ImportResults::status,
                  api::passwords_private::ImportResultsStatus::kBadFormat)))
      .Times(1);
  delegate->ImportPasswords(api::passwords_private::PasswordStoreSet::kAccount,
                            callback.Get(), web_contents.get());
  task_environment()->RunUntilIdle();

  histogram_tester().ExpectUniqueSample("PasswordManager.ImportResultsStatus2",
                                        kExpectedStatus, 1);
}

TEST_F(PasswordsPrivateDelegateImplTest, TestReauthFailedOnImport) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  auto* client =
      MockPasswordManagerClient::CreateForWebContentsAndGet(web_contents.get());

  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();

  auto fake_porter = std::make_unique<FakePasswordManagerPorter>();
  auto* fake_porter_ptr = fake_porter.get();
  delegate->SetPorterForTesting(std::move(fake_porter));
  ON_CALL(*(client->GetPasswordFeatureManager()), IsOptedInForAccountStorage)
      .WillByDefault(Return(false));

  const auto kExpectedStatus =
      password_manager::ImportResults::Status::DISMISSED;
  fake_porter_ptr->set_import_result_status(kExpectedStatus);

  ExpectAuthentication(delegate, /*successful=*/false);

  base::MockCallback<PasswordsPrivateDelegate::ImportResultsCallback>
      import_callback;
  EXPECT_CALL(import_callback,
              Run(::testing::Field(
                  &ImportResults::status,
                  api::passwords_private::ImportResultsStatus::kDismissed)))
      .Times(1);

  delegate->ContinueImport(/*selected_ids=*/{1}, import_callback.Get(),
                           web_contents.get());
  task_environment()->RunUntilIdle();

  histogram_tester().ExpectUniqueSample("PasswordManager.ImportResultsStatus2",
                                        kExpectedStatus, 1);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       ContinueImportLogsImportResultsStatus) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  auto* client =
      MockPasswordManagerClient::CreateForWebContentsAndGet(web_contents.get());
  scoped_refptr<PasswordsPrivateDelegateImpl> delegate = CreateDelegate();

  auto fake_porter = std::make_unique<FakePasswordManagerPorter>();
  auto* fake_porter_ptr = fake_porter.get();
  delegate->SetPorterForTesting(std::move(fake_porter));

  ON_CALL(*(client->GetPasswordFeatureManager()), IsOptedInForAccountStorage)
      .WillByDefault(Return(false));

  const auto kExpectedStatus =
      password_manager::ImportResults::Status::BAD_FORMAT;
  fake_porter_ptr->set_import_result_status(kExpectedStatus);

  base::MockCallback<PasswordsPrivateDelegate::ImportResultsCallback> callback;
  EXPECT_CALL(callback,
              Run(::testing::Field(
                  &ImportResults::status,
                  api::passwords_private::ImportResultsStatus::kBadFormat)))
      .Times(1);
  delegate->ContinueImport(/*selected_ids=*/{}, callback.Get(),
                           web_contents.get());
  task_environment()->RunUntilIdle();

  histogram_tester().ExpectUniqueSample("PasswordManager.ImportResultsStatus2",
                                        kExpectedStatus, 1);
}

TEST_F(PasswordsPrivateDelegateImplTest, ResetImporter) {
  auto delegate = CreateDelegate();

  auto mock_porter = std::make_unique<MockPasswordManagerPorter>();
  auto* mock_porter_ptr = mock_porter.get();
  delegate->SetPorterForTesting(std::move(mock_porter));

  EXPECT_CALL(*mock_porter_ptr, ResetImporter).Times(1);
  delegate->ResetImporter(/*delete_file=*/false);
}

TEST_F(PasswordsPrivateDelegateImplTest, ChangeCredential_Password) {
  PasswordForm sample_form = CreateSampleForm();
  SetUpPasswordStores({sample_form});
  auto delegate = CreateDelegate();
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  PasswordUiEntry updated_credential = GetCredentials(*delegate).at(0).Clone();
  updated_credential.password = "new_pass";
  updated_credential.username = "new_user";

  EXPECT_TRUE(delegate->ChangeCredential(updated_credential));

  // Spin the loop to allow PasswordStore tasks posted when changing the
  // password to be completed.
  base::RunLoop().RunUntilIdle();

  // Check that the changing the password got reflected in the passwords list.
  // `note` field should not be filled when `GetSavedPasswordsList` is called.
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

  auto delegate = CreateDelegate();
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  PasswordUiEntry updated_credential = GetCredentials(*delegate).at(0).Clone();
  updated_credential.password = "new_pass";
  updated_credential.username = "new_user";

  EXPECT_TRUE(delegate->ChangeCredential(updated_credential));

  // Spin the loop to allow PasswordStore tasks posted when changing the
  // password to be completed.
  base::RunLoop().RunUntilIdle();

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

  auto delegate = CreateDelegate();
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  // Get the account credential.
  const PasswordsPrivateDelegate::UiEntries& credentials =
      GetCredentials(*delegate);
  EXPECT_EQ(credentials.size(), 2u);
  const auto account_credential_it = base::ranges::find(
      credentials, api::passwords_private::PasswordStoreSet::kAccount,
      &PasswordUiEntry::stored_in);
  ASSERT_NE(account_credential_it, credentials.end());

  PasswordUiEntry updated_credential = account_credential_it->Clone();
  updated_credential.password = "new_pass";
  updated_credential.username = "new_user";

  EXPECT_TRUE(delegate->ChangeCredential(updated_credential));

  // Spin the loop to allow PasswordStore tasks posted when changing the
  // password to be completed.
  base::RunLoop().RunUntilIdle();

  const PasswordsPrivateDelegate::UiEntries& updated_credentials =
      GetCredentials(*delegate);
  EXPECT_EQ(updated_credentials.size(), 2u);
  const auto refreshed_credential_it = base::ranges::find(
      updated_credentials, api::passwords_private::PasswordStoreSet::kAccount,
      &PasswordUiEntry::stored_in);
  ASSERT_NE(account_credential_it, updated_credentials.end());
  EXPECT_EQ(refreshed_credential_it->username, "new_user");
}

TEST_F(PasswordsPrivateDelegateImplTest, ChangeCredential_Passkey) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(syncer::kSyncWebauthnCredentials);

  webauthn::PasskeyModel* passkey_model =
      PasskeyModelFactory::GetForProfile(profile());
  ASSERT_EQ(passkey_model, PasskeyModelFactory::GetForProfile(profile()));
  ASSERT_TRUE(passkey_model);
  sync_pb::WebauthnCredentialSpecifics passkey = CreatePasskey();
  passkey_model->AddNewPasskeyForTesting(passkey);

  auto delegate = CreateDelegate();
  // Spin the loop to allow PasskeyModel tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  // Get the passkey credential.
  const PasswordsPrivateDelegate::UiEntries& credentials =
      GetCredentials(*delegate);
  EXPECT_EQ(credentials.size(), 1u);
  const PasswordUiEntry& existing_credential = credentials.at(0);
  EXPECT_TRUE(existing_credential.is_passkey);

  PasswordUiEntry updated_credential = existing_credential.Clone();
  updated_credential.username = "new_user";
  updated_credential.display_name = "new_display_name";

  EXPECT_TRUE(delegate->ChangeCredential(updated_credential));

  // Spin the loop to allow PasskeyModel tasks posted when changing the
  // password to be completed.
  base::RunLoop().RunUntilIdle();

  const PasswordsPrivateDelegate::UiEntries& updated_credentials =
      GetCredentials(*delegate);
  EXPECT_EQ(updated_credentials.size(), 1u);
  EXPECT_EQ(updated_credentials.at(0).username, "new_user");
  EXPECT_EQ(updated_credentials.at(0).display_name, "new_display_name");
}

TEST_F(PasswordsPrivateDelegateImplTest, ChangeCredential_NotFound) {
  SetUpPasswordStores({});
  auto delegate = CreateDelegate();
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(delegate->ChangeCredential(PasswordUiEntry()));
}

TEST_F(PasswordsPrivateDelegateImplTest, ChangeCredential_EmptyPassword) {
  PasswordForm sample_form = CreateSampleForm();
  SetUpPasswordStores({sample_form});
  auto delegate = CreateDelegate();
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  PasswordUiEntry updated_credential = GetCredentials(*delegate).at(0).Clone();
  updated_credential.password = "";
  updated_credential.username = "new_user";

  EXPECT_FALSE(delegate->ChangeCredential(updated_credential));
}

// Checking callback result of RequestPlaintextPassword with reason Copy.
// By implementation for Copy, callback will receive empty string.
TEST_F(PasswordsPrivateDelegateImplTest, TestCopyPasswordCallbackResult) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  PasswordForm form = CreateSampleForm();
  SetUpPasswordStores({form});

  auto delegate = CreateDelegate();
  base::RunLoop().RunUntilIdle();

  ExpectAuthentication(delegate, /*successful=*/true);

  MockPlaintextPasswordCallback password_callback;
  EXPECT_CALL(password_callback, Run(Eq(std::u16string())));
  delegate->RequestPlaintextPassword(
      0, api::passwords_private::PlaintextReason::kCopy,
      password_callback.Get(), web_contents.get());

  std::u16string result;
  test_clipboard_->ReadText(ui::ClipboardBuffer::kCopyPaste,
                            /* data_dst = */ nullptr, &result);
  EXPECT_EQ(form.password_value, result);

  histogram_tester().ExpectUniqueSample(
      kHistogramName, password_manager::metrics_util::ACCESS_PASSWORD_COPIED,
      1);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       TestShouldNotReauthForOptInIfExplicitSigninUIEnabled) {
  base::test::ScopedFeatureList scoped_feature_list(
      switches::kExplicitBrowserSigninUIOnDesktop);
  profile()->GetPrefs()->SetBoolean(prefs::kExplicitBrowserSignin, false);

  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  auto* client =
      MockPasswordManagerClient::CreateForWebContentsAndGet(web_contents.get());
  ON_CALL(*(client->GetPasswordFeatureManager()), IsOptedInForAccountStorage)
      .WillByDefault(Return(false));

  EXPECT_CALL(*client,
              TriggerReauthForPrimaryAccount(
                  signin_metrics::ReauthAccessPoint::kPasswordSettings, _))
      .Times(0);

  auto delegate = CreateDelegate();
  delegate->SetAccountStorageEnabled(true, web_contents.get());

  profile()->GetPrefs()->SetBoolean(prefs::kExplicitBrowserSignin, true);

  // Implicit and explicit sign-ins are treated alike.
  delegate->SetAccountStorageEnabled(true, web_contents.get());
}

TEST_F(PasswordsPrivateDelegateImplTest,
       TestShouldNotReauthForOptOutAndShouldSetPref) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  auto* client =
      MockPasswordManagerClient::CreateForWebContentsAndGet(web_contents.get());
  password_manager::MockPasswordFeatureManager* feature_manager =
      client->GetPasswordFeatureManager();
  ON_CALL(*feature_manager, IsOptedInForAccountStorage)
      .WillByDefault(Return(true));

  EXPECT_CALL(*client,
              TriggerReauthForPrimaryAccount(
                  signin_metrics::ReauthAccessPoint::kPasswordSettings, _))
      .Times(0);
  EXPECT_CALL(*feature_manager, OptOutOfAccountStorage);

  auto delegate = CreateDelegate();
  delegate->SetAccountStorageEnabled(false, web_contents.get());
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
TEST_F(PasswordsPrivateDelegateImplTest, TestCopyPasswordCallbackResultFail) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  SetUpPasswordStores({CreateSampleForm()});

  auto delegate = CreateDelegate();
  base::RunLoop().RunUntilIdle();

  ExpectAuthentication(delegate, /*successful=*/false);

  base::Time before_call = test_clipboard_->GetLastModifiedTime();

  MockPlaintextPasswordCallback password_callback;
  EXPECT_CALL(password_callback, Run(Eq(std::nullopt)));
  delegate->RequestPlaintextPassword(
      0, api::passwords_private::PlaintextReason::kCopy,
      password_callback.Get(), web_contents.get());
  // Clipboard should not be modified in case Reauth failed
  std::u16string result;
  test_clipboard_->ReadText(ui::ClipboardBuffer::kCopyPaste,
                            /* data_dst = */ nullptr, &result);
  EXPECT_EQ(std::u16string(), result);
  EXPECT_EQ(before_call, test_clipboard_->GetLastModifiedTime());

  // Since Reauth had failed password was not copied and metric wasn't recorded
  histogram_tester().ExpectTotalCount(kHistogramName, 0);
}
#endif

TEST_F(PasswordsPrivateDelegateImplTest, TestPassedReauthOnView) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  SetUpPasswordStores({CreateSampleForm()});

  auto delegate = CreateDelegate();
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  ExpectAuthentication(delegate, /*successful=*/true);

  MockPlaintextPasswordCallback password_callback;
  EXPECT_CALL(password_callback, Run(Eq(u"test")));
  delegate->RequestPlaintextPassword(
      0, api::passwords_private::PlaintextReason::kView,
      password_callback.Get(), web_contents.get());

  histogram_tester().ExpectUniqueSample(
      kHistogramName, password_manager::metrics_util::ACCESS_PASSWORD_VIEWED,
      1);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       TestPassedReauthOnRequestCredentialsDetails) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  PasswordForm sample_form = CreateSampleForm();
  sample_form.notes.emplace_back(u"best note ever",
                                 /*date_created=*/base::Time::Now());
  SetUpPasswordStores({sample_form});

  auto delegate = CreateDelegate();
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  ExpectAuthentication(delegate, /*successful=*/true);

  MockRequestCredentialsDetailsCallback password_callback;
  EXPECT_CALL(password_callback, Run)
      .WillOnce([&](const std::vector<PasswordUiEntry>& entries) {
        EXPECT_EQ(1u, entries.size());
        EXPECT_THAT(entries[0].password, Eq("test"));
        EXPECT_THAT(entries[0].username, Eq("test@gmail.com"));
        EXPECT_THAT(entries[0].note, Eq("best note ever"));
      });

  delegate->RequestCredentialsDetails({0}, password_callback.Get(),
                                      web_contents.get());

  histogram_tester().ExpectUniqueSample(
      kHistogramName, password_manager::metrics_util::ACCESS_PASSWORD_VIEWED,
      1);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
TEST_F(PasswordsPrivateDelegateImplTest, TestFailedReauthOnView) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  SetUpPasswordStores({CreateSampleForm()});

  auto delegate = CreateDelegate();
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  ExpectAuthentication(delegate, /*successful=*/false);

  MockPlaintextPasswordCallback password_callback;
  EXPECT_CALL(password_callback, Run(Eq(std::nullopt)));
  delegate->RequestPlaintextPassword(
      0, api::passwords_private::PlaintextReason::kView,
      password_callback.Get(), web_contents.get());

  // Since Reauth had failed password was not viewed and metric wasn't recorded
  histogram_tester().ExpectTotalCount(kHistogramName, 0);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       TestFailedReauthOnRequestCredentialsDetails) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  SetUpPasswordStores({CreateSampleForm()});

  auto delegate = CreateDelegate();
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  ExpectAuthentication(delegate, /*successful=*/false);

  MockRequestCredentialsDetailsCallback password_callback;
  EXPECT_CALL(password_callback, Run(testing::IsEmpty()));
  delegate->RequestCredentialsDetails({0}, password_callback.Get(),
                                      web_contents.get());

  // Since Reauth had failed password was not viewed and metric wasn't recorded
  histogram_tester().ExpectTotalCount(kHistogramName, 0);
}

TEST_F(PasswordsPrivateDelegateImplTest, TestReauthFailedOnExport) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  SetUpPasswordStores({CreateSampleForm()});
  StrictMock<base::MockCallback<base::OnceCallback<void(const std::string&)>>>
      mock_accepted;

  auto delegate = CreateDelegate();
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  ExpectAuthentication(delegate, /*successful=*/false);

  EXPECT_CALL(mock_accepted, Run(std::string("reauth-failed")));
  delegate->ExportPasswords(mock_accepted.Get(), web_contents.get());
}
#endif

TEST_F(PasswordsPrivateDelegateImplTest,
       GetUrlCollectionValueWithSchemeWhenIpAddress) {
  auto delegate = CreateDelegate();
  const std::optional<UrlCollection> urls =
      delegate->GetUrlCollection("127.0.0.1");
  EXPECT_TRUE(urls.has_value());
  EXPECT_EQ("127.0.0.1", urls.value().shown);
  EXPECT_EQ("http://127.0.0.1/", urls.value().signon_realm);
  EXPECT_EQ("http://127.0.0.1/", urls.value().link);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       GetUrlCollectionValueWithSchemeWhenWebAddress) {
  auto delegate = CreateDelegate();
  const std::optional<UrlCollection> urls =
      delegate->GetUrlCollection("example.com/login");
  EXPECT_TRUE(urls.has_value());
  EXPECT_EQ("example.com", urls.value().shown);
  EXPECT_EQ("https://example.com/", urls.value().signon_realm);
  EXPECT_EQ("https://example.com/login", urls.value().link);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       GetUrlCollectionStrippedValueWhenFullUrl) {
  auto delegate = CreateDelegate();
  const std::optional<UrlCollection> urls = delegate->GetUrlCollection(
      "http://username:password@example.com/login?param=value#ref");
  EXPECT_TRUE(urls.has_value());
  EXPECT_EQ("example.com", urls.value().shown);
  EXPECT_EQ("http://example.com/", urls.value().signon_realm);
  EXPECT_EQ("http://example.com/login", urls.value().link);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       GetUrlCollectionNoValueWhenUnsupportedScheme) {
  auto delegate = CreateDelegate();
  const std::optional<UrlCollection> urls =
      delegate->GetUrlCollection("scheme://unsupported");
  EXPECT_FALSE(urls.has_value());
}

TEST_F(PasswordsPrivateDelegateImplTest,
       GetUrlCollectionNoValueWhenInvalidUrl) {
  auto delegate = CreateDelegate();
  const std::optional<UrlCollection> urls =
      delegate->GetUrlCollection("https://^/invalid");
  EXPECT_FALSE(urls.has_value());
}

TEST_F(PasswordsPrivateDelegateImplTest, IsAccountStoreDefault) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  auto* client =
      MockPasswordManagerClient::CreateForWebContentsAndGet(web_contents.get());
  ON_CALL(*(client->GetPasswordFeatureManager()), IsOptedInForAccountStorage)
      .WillByDefault(Return(true));

  auto delegate = CreateDelegate();

  EXPECT_CALL(*(client->GetPasswordFeatureManager()), GetDefaultPasswordStore)
      .WillOnce(Return(PasswordForm::Store::kAccountStore));
  EXPECT_TRUE(delegate->IsAccountStoreDefault(web_contents.get()));

  EXPECT_CALL(*(client->GetPasswordFeatureManager()), GetDefaultPasswordStore)
      .WillOnce(Return(PasswordForm::Store::kProfileStore));
  EXPECT_FALSE(delegate->IsAccountStoreDefault(web_contents.get()));
}

TEST_F(PasswordsPrivateDelegateImplTest, TestMovePasswordsToAccountStore) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  auto* client =
      MockPasswordManagerClient::CreateForWebContentsAndGet(web_contents.get());
  ON_CALL(*(client->GetPasswordFeatureManager()), IsOptedInForAccountStorage)
      .WillByDefault(Return(true));

  auto delegate = CreateDelegate();
  PasswordForm form1 = CreateSampleForm(PasswordForm::Store::kProfileStore);

  SetUpPasswordStores({form1});

  int first_id =
      delegate->GetIdForCredential(password_manager::CredentialUIEntry(form1));

  delegate->MovePasswordsToAccount({first_id}, web_contents.get());
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.AccountStorage.MoveToAccountStoreFlowAccepted2",
      password_manager::metrics_util::MoveToAccountStoreTrigger::
          kExplicitlyTriggeredInSettings,
      1);
}

TEST_F(PasswordsPrivateDelegateImplTest, VerifyCastingOfImportEntryStatus) {
  static_assert(
      base::to_underlying(api::passwords_private::ImportEntryStatus::kNone) ==
      int{password_manager::ImportEntry::Status::NONE});
  static_assert(base::to_underlying(
                    api::passwords_private::ImportEntryStatus::kUnknownError) ==
                int{password_manager::ImportEntry::Status::UNKNOWN_ERROR});
  static_assert(
      base::to_underlying(
          api::passwords_private::ImportEntryStatus::kMissingPassword) ==
      int{password_manager::ImportEntry::Status::MISSING_PASSWORD});
  static_assert(base::to_underlying(
                    api::passwords_private::ImportEntryStatus::kMissingUrl) ==
                int{password_manager::ImportEntry::Status::MISSING_URL});
  static_assert(base::to_underlying(
                    api::passwords_private::ImportEntryStatus::kInvalidUrl) ==
                int{password_manager::ImportEntry::Status::INVALID_URL});
  static_assert(base::to_underlying(
                    api::passwords_private::ImportEntryStatus::kLongUrl) ==
                int{password_manager::ImportEntry::Status::LONG_URL});
  static_assert(base::to_underlying(
                    api::passwords_private::ImportEntryStatus::kLongPassword) ==
                int{password_manager::ImportEntry::Status::LONG_PASSWORD});
  static_assert(base::to_underlying(
                    api::passwords_private::ImportEntryStatus::kLongUsername) ==
                int{password_manager::ImportEntry::Status::LONG_USERNAME});
  static_assert(
      base::to_underlying(
          api::passwords_private::ImportEntryStatus::kConflictProfile) ==
      int{password_manager::ImportEntry::Status::CONFLICT_PROFILE});
  static_assert(
      base::to_underlying(
          api::passwords_private::ImportEntryStatus::kConflictAccount) ==
      int{password_manager::ImportEntry::Status::CONFLICT_ACCOUNT});
  static_assert(base::to_underlying(
                    api::passwords_private::ImportEntryStatus::kLongNote) ==
                int{password_manager::ImportEntry::Status::LONG_NOTE});
  static_assert(
      base::to_underlying(
          api::passwords_private::ImportEntryStatus::kLongConcatenatedNote) ==
      int{password_manager::ImportEntry::Status::LONG_CONCATENATED_NOTE});
  static_assert(
      base::to_underlying(api::passwords_private::ImportEntryStatus::kValid) ==
      int{password_manager::ImportEntry::Status::VALID});
}

TEST_F(PasswordsPrivateDelegateImplTest, VerifyCastingOfImportResultsStatus) {
  static_assert(
      base::to_underlying(api::passwords_private::ImportResultsStatus::kNone) ==
      int{password_manager::ImportResults::Status::NONE});
  static_assert(
      base::to_underlying(
          api::passwords_private::ImportResultsStatus::kUnknownError) ==
      int{password_manager::ImportResults::Status::UNKNOWN_ERROR});
  static_assert(base::to_underlying(
                    api::passwords_private::ImportResultsStatus::kSuccess) ==
                int{password_manager::ImportResults::Status::SUCCESS});
  static_assert(base::to_underlying(
                    api::passwords_private::ImportResultsStatus::kIoError) ==
                int{password_manager::ImportResults::Status::IO_ERROR});
  static_assert(base::to_underlying(
                    api::passwords_private::ImportResultsStatus::kBadFormat) ==
                int{password_manager::ImportResults::Status::BAD_FORMAT});
  static_assert(base::to_underlying(
                    api::passwords_private::ImportResultsStatus::kDismissed) ==
                int{password_manager::ImportResults::Status::DISMISSED});
  static_assert(
      base::to_underlying(
          api::passwords_private::ImportResultsStatus::kMaxFileSize) ==
      int{password_manager::ImportResults::Status::MAX_FILE_SIZE});
  static_assert(
      base::to_underlying(
          api::passwords_private::ImportResultsStatus::kImportAlreadyActive) ==
      int{password_manager::ImportResults::Status::IMPORT_ALREADY_ACTIVE});
  static_assert(
      base::to_underlying(
          api::passwords_private::ImportResultsStatus::kNumPasswordsExceeded) ==
      int{password_manager::ImportResults::Status::NUM_PASSWORDS_EXCEEDED});
  static_assert(base::to_underlying(
                    api::passwords_private::ImportResultsStatus::kConflicts) ==
                int{password_manager::ImportResults::Status::CONFLICTS});
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
// Checks if authentication is triggered.
TEST_F(PasswordsPrivateDelegateImplTest,
       SwitchBiometricAuthBeforeFillingState) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  base::MockCallback<
      extensions::PasswordsPrivateDelegate::AuthenticationCallback>
      result_callback;

  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling, false);

  auto delegate = CreateDelegate();
  ExpectAuthentication(delegate, /*successful=*/true);

  EXPECT_CALL(result_callback, Run(/*auth_succeeded=*/true));
  delegate->SwitchBiometricAuthBeforeFillingState(web_contents.get(),
                                                  result_callback.Get());
  // Expects that the switch value will change.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling));
}

TEST_F(PasswordsPrivateDelegateImplTest,
       SwitchBiometricAuthBeforeFillingStateAuthenticationFailed) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  base::MockCallback<
      extensions::PasswordsPrivateDelegate::AuthenticationCallback>
      result_callback;

  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling, false);

  auto delegate = CreateDelegate();
  ExpectAuthentication(delegate, /*successful=*/false);

  EXPECT_CALL(result_callback, Run(/*auth_succeeded=*/false));
  delegate->SwitchBiometricAuthBeforeFillingState(web_contents.get(),
                                                  result_callback.Get());

  // Expects that the switch value will change.
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling));
}
#endif

#if BUILDFLAG(IS_MAC)
// Checks if authentication is triggered.
TEST_F(PasswordsPrivateDelegateImplTest,
       SwitchBiometricAuthBeforeFillingCancelsLastTry) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  auto biometric_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  auto* biometric_authenticator_ptr = biometric_authenticator.get();

  auto delegate = CreateDelegate();
  EXPECT_CALL(*biometric_authenticator_ptr, AuthenticateWithMessage);
  delegate->SetDeviceAuthenticatorForTesting(
      std::move(biometric_authenticator));

  delegate->SwitchBiometricAuthBeforeFillingState(web_contents.get(),
                                                  base::DoNothing());

  // Invoking authentication again will cancel previous request.
  EXPECT_CALL(*biometric_authenticator_ptr, Cancel);
  ExpectAuthentication(delegate, /*successful=*/true);
  delegate->SwitchBiometricAuthBeforeFillingState(web_contents.get(),
                                                  base::DoNothing());
}
#endif

#if BUILDFLAG(IS_WIN)
// Checks if authentication is triggered.
TEST_F(PasswordsPrivateDelegateImplTest,
       SwitchBiometricAuthBeforeFillingDoesntCancelLastTry) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  base::MockCallback<
      extensions::PasswordsPrivateDelegate::AuthenticationCallback>
      result_callback;

  auto biometric_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  auto* biometric_authenticator_ptr = biometric_authenticator.get();

  auto delegate = CreateDelegate();
  EXPECT_CALL(*biometric_authenticator_ptr, AuthenticateWithMessage);
  delegate->SetDeviceAuthenticatorForTesting(
      std::move(biometric_authenticator));

  delegate->SwitchBiometricAuthBeforeFillingState(web_contents.get(),
                                                  result_callback.Get());

  // Invoking authentication again should not cancel previous request.
  EXPECT_CALL(*biometric_authenticator_ptr, Cancel).Times(0);
  EXPECT_CALL(result_callback, Run(false));
  delegate->SwitchBiometricAuthBeforeFillingState(web_contents.get(),
                                                  result_callback.Get());
}
#endif

// TODO(http://crbug.com/1455574) Re-enable.
TEST_F(PasswordsPrivateDelegateImplTest, DISABLED_ShowAddShortcutDialog) {
  base::HistogramTester histogram_tester;
  // Set up a browser instance and simulate a navigation.
  Browser::CreateParams params(profile(), /*user_gesture=*/true);
  params.type = Browser::TYPE_NORMAL;
  auto window = std::make_unique<TestBrowserWindow>();
  params.window = window.get();
  auto browser = std::unique_ptr<Browser>(Browser::Create(params));
  NavigateParams nav_params(browser.get(), GURL("chrome://password-manager"),
                            ui::PAGE_TRANSITION_TYPED);
  nav_params.tabstrip_index = 0;
  nav_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&nav_params);
  content::RenderFrameHostTester::CommitPendingLoad(
      &nav_params.navigated_or_inserted_contents->GetController());

  webapps::WebappsClientDesktop::CreateSingleton();
  auto* provider = web_app::FakeWebAppProvider::Get(profile());
  // This test harness is handling web contents loading, so use the real web
  // contents manager.
  provider->SetWebContentsManager(
      std::make_unique<web_app::WebContentsManager>());
  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  task_environment()->RunUntilIdle();

  // Check that no web app installation is happening at the moment.
  ASSERT_EQ(provider->command_manager().GetCommandCountForTesting(), 0u);
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_FALSE(
      provider->command_manager().IsInstallingForWebContents(web_contents));

  auto delegate = CreateDelegate();
  delegate->ShowAddShortcutDialog(web_contents);
  task_environment()->RunUntilIdle();

  // Check that app installation was triggered.
  EXPECT_EQ(provider->command_manager().GetCommandCountForTesting(), 1u);
  EXPECT_TRUE(
      provider->command_manager().IsInstallingForWebContents(web_contents));

  // Close the browser prior to TearDown.
  browser->tab_strip_model()->CloseAllTabs();

  histogram_tester.ExpectUniqueSample("PasswordManager.ShortcutMetric", 0, 1);
}

TEST_F(PasswordsPrivateDelegateImplTest, GetCredentialGroups) {
  auto delegate = CreateDelegate();

  PasswordForm password1 =
      CreateSampleForm(PasswordForm::Store::kProfileStore, u"username1");
  PasswordForm password2 =
      CreateSampleForm(PasswordForm::Store::kProfileStore, u"username2");

  SetUpPasswordStores({password1, password2});

  auto groups = delegate->GetCredentialGroups();
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
  EXPECT_THAT(groups[0].entries,
              testing::UnorderedElementsAre(
                  PasswordUiEntryDataEquals(testing::ByRef(expected_entry1)),
                  PasswordUiEntryDataEquals(testing::ByRef(expected_entry2))));
}

TEST_F(PasswordsPrivateDelegateImplTest, PasswordManagerAppInstalled) {
  base::HistogramTester histogram_tester;
  auto delegate = CreateDelegate();
  static_cast<web_app::WebAppInstallManagerObserver*>(delegate.get())
      ->OnWebAppInstalledWithOsHooks(web_app::kPasswordManagerAppId);

  EXPECT_THAT(histogram_tester.GetAllSamples("PasswordManager.ShortcutMetric"),
              base::BucketsAre(base::Bucket(1, 1)));

  // Check that installing other app doesn't get recorded.
  static_cast<web_app::WebAppInstallManagerObserver*>(delegate.get())
      ->OnWebAppInstalledWithOsHooks(web_app::kYoutubeMusicAppId);

  histogram_tester.ExpectUniqueSample("PasswordManager.ShortcutMetric", 1, 1);
}

TEST_F(PasswordsPrivateDelegateImplTest, GetPasskeyInGroups) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(syncer::kSyncWebauthnCredentials);

  auto delegate = CreateDelegate();

  webauthn::PasskeyModel* passkey_model =
      PasskeyModelFactory::GetForProfile(profile());
  ASSERT_EQ(passkey_model, PasskeyModelFactory::GetForProfile(profile()));
  ASSERT_TRUE(passkey_model);
  sync_pb::WebauthnCredentialSpecifics passkey = CreatePasskey();
  passkey_model->AddNewPasskeyForTesting(passkey);

  PasswordForm password =
      CreateSampleForm(PasswordForm::Store::kProfileStore, u"username1");
  SetUpPasswordStores({password});

  auto groups = delegate->GetCredentialGroups();
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
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(syncer::kSyncWebauthnCredentials);

  auto delegate = CreateDelegate();

  webauthn::PasskeyModel* passkey_model =
      PasskeyModelFactory::GetForProfile(profile());
  ASSERT_EQ(passkey_model, PasskeyModelFactory::GetForProfile(profile()));
  ASSERT_TRUE(passkey_model);
  sync_pb::WebauthnCredentialSpecifics passkey = CreatePasskey();
  passkey_model->AddNewPasskeyForTesting(std::move(passkey));
  SetUpPasswordStores({});

  auto groups = delegate->GetCredentialGroups();
  PasswordUiEntry& passkey_entry = groups.at(0).entries.at(0);
  ASSERT_TRUE(passkey_entry.is_passkey);
  EXPECT_EQ(user_action_tester.GetActionCount("PasswordManager_RemovePasskey"),
            0);

  delegate->RemoveCredential(passkey_entry.id, passkey_entry.stored_in);
  groups = delegate->GetCredentialGroups();
  EXPECT_TRUE(groups.empty());
  EXPECT_EQ(user_action_tester.GetActionCount("PasswordManager_RemovePasskey"),
            1);

  // Attempt removing a non existent entry.
  delegate->RemoveCredential(
      /*id=*/42, api::passwords_private::PasswordStoreSet::kAccount);
  EXPECT_EQ(user_action_tester.GetActionCount("PasswordManager_RemovePasskey"),
            1);
}

// Ensures that if a password is deleted from the account store via the settings
// UI, password removal reason is recorded in the pref.
TEST_F(PasswordsPrivateDelegateImplTest,
       RemovePasswordFromAccountStoreTracksRemovalReason) {
  auto delegate = CreateDelegate();

  PasswordForm password =
      CreateSampleForm(PasswordForm::Store::kAccountStore, u"username");
  password.signon_realm = "https://facebook.com";
  password.url = GURL("https://facebook.com");

  SetUpPasswordStores({password});

  auto groups = delegate->GetCredentialGroups();
  PasswordUiEntry& password_entry = groups.at(0).entries.at(0);

  delegate->RemoveCredential(password_entry.id, password_entry.stored_in);

  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                password_manager::prefs::kPasswordRemovalReasonForAccount),
            1 << static_cast<int>(
                password_manager::metrics_util::
                    PasswordManagerCredentialRemovalReason::kSettings));
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                password_manager::prefs::kPasswordRemovalReasonForProfile),
            0);
}

// Ensures that if a password is deleted from the profile store via the settings
// UI, password removal reason is recorded in the pref.
TEST_F(PasswordsPrivateDelegateImplTest,
       RemovePasswordFromProfileStoreTracksRemovalReason) {
  auto delegate = CreateDelegate();

  PasswordForm password =
      CreateSampleForm(PasswordForm::Store::kProfileStore, u"username");
  password.signon_realm = "https://facebook.com";
  password.url = GURL("https://facebook.com");

  SetUpPasswordStores({password});

  auto groups = delegate->GetCredentialGroups();
  PasswordUiEntry& password_entry = groups.at(0).entries.at(0);

  delegate->RemoveCredential(password_entry.id, password_entry.stored_in);

  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                password_manager::prefs::kPasswordRemovalReasonForAccount),
            0);
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                password_manager::prefs::kPasswordRemovalReasonForProfile),
            1 << static_cast<int>(
                password_manager::metrics_util::
                    PasswordManagerCredentialRemovalReason::kSettings));
}

TEST_F(PasswordsPrivateDelegateImplTest, SharePasswordWithTwoRecipients) {
  auto delegate = CreateDelegate();
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

  password_manager::MockPasswordSenderService* password_sender_service =
      static_cast<password_manager::MockPasswordSenderService*>(
          PasswordSenderServiceFactory::GetForProfile(profile()));

  password_manager::PublicKey expected_public_key1, expected_public_key2;
  expected_public_key1.key = kSharingRecipientKeyValue1;
  expected_public_key2.key = kSharingRecipientKeyValue2;
  // There are two recipients and hence, SendPasswords() should be called twice
  // with the same credentials for each recipient.
  EXPECT_CALL(
      *password_sender_service,
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
      *password_sender_service,
      SendPasswords(
          ElementsAre(AllOf(
              Field(&PasswordForm::username_value, password.username_value),
              Field(&PasswordForm::password_value, password.password_value),
              Field(&PasswordForm::signon_realm, password.signon_realm))),
          AllOf(Field("user id", &PasswordRecipient::user_id,
                      kSharingRecipientId2),
                Field("public key", &PasswordRecipient::public_key,
                      expected_public_key2)))

  );

  delegate->SharePassword(/*id=*/0, recipients);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       ShareAllPasswordsRepresentedByUiEntry) {
  auto delegate = CreateDelegate();
  // `password1` and `password2` share the same username and password and their
  // origins are PSL matches. They should be represented by the same ui entry.
  // `password3` has a different username and hence is represented by a
  // different ui entry.
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

  // Credentials should have been grouped in two groups.
  PasswordsPrivateDelegate::CredentialsGroups groups =
      delegate->GetCredentialGroups();
  ASSERT_EQ(groups.size(), 2U);
  // Find the id of the ui entry that represents both facebook.com and
  // m.facebook.com
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

  password_manager::MockPasswordSenderService* password_sender_service =
      static_cast<password_manager::MockPasswordSenderService*>(
          PasswordSenderServiceFactory::GetForProfile(profile()));

  password_manager::PublicKey expected_public_key;
  expected_public_key.key = kSharingRecipientKeyValue1;
  // There is one recipient and hence, SendPasswords() should be called only
  // once with the two credentials represented by this ui entry.
  EXPECT_CALL(
      *password_sender_service,
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
  auto delegate = CreateDelegate();

  PasswordsPrivateDelegate::ShareRecipients recipients;
  RecipientInfo recipient;
  recipient.user_id = kSharingRecipientId1;
  recipients.push_back(std::move(recipient));

  password_manager::MockPasswordSenderService* password_sender_service =
      static_cast<password_manager::MockPasswordSenderService*>(
          PasswordSenderServiceFactory::GetForProfile(profile()));
  EXPECT_CALL(*password_sender_service, SendPasswords).Times(0);

  delegate->SharePassword(/*id=*/100, recipients);
}

TEST_F(PasswordsPrivateDelegateImplTest, IsChangePinFlowAvailable) {
  auto delegate = CreateDelegate();
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;

  EXPECT_CALL(mock_callback, Run(Eq(true)));
  EXPECT_CALL(change_pin_controller_, IsChangePinFlowAvailable)
      .WillOnce([&](auto callback) { std::move(callback).Run(true); });
  delegate->IsPasswordManagerPinAvailable(web_contents.get(),
                                          mock_callback.Get());
}

TEST_F(PasswordsPrivateDelegateImplTest, DisconnectCloudAuthenticator) {
  auto delegate = CreateDelegate();
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  MockEnclaveManager* enclave_manager_mock = static_cast<MockEnclaveManager*>(
      EnclaveManagerFactory::GetForProfile(profile()));
  EXPECT_CALL(*enclave_manager_mock, Unenroll).Times(1);

  delegate->DisconnectCloudAuthenticator(
      web_contents.get(),
      base::BindLambdaForTesting([](bool success) { EXPECT_TRUE(success); }));
}

TEST_F(PasswordsPrivateDelegateImplTest, IsConnecetdToCloudAuthenticator) {
  auto delegate = CreateDelegate();
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  MockEnclaveManager* enclave_manager_mock = static_cast<MockEnclaveManager*>(
      EnclaveManagerFactory::GetForProfile(profile()));
  EXPECT_CALL(*enclave_manager_mock, is_registered).Times(1);

  delegate->IsConnectedToCloudAuthenticator(web_contents.get());
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
class PasswordsPrivateDelegateImplMockTaskEnvironmentTest
    : public testing::Test {
 public:
  PasswordsPrivateDelegateImplMockTaskEnvironmentTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    profile_ = profile_manager_.CreateTestingProfile("test_profile");
    web_contents_ = web_contents_factory_.CreateWebContents(profile_);

    profile_store_ = CreateAndUseTestPasswordStore(profile_);
    account_store_ = CreateAndUseTestAccountPasswordStore(profile_);
  }

  void TearDown() override { profile_manager_.DeleteAllTestingProfiles(); }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  content::WebContents* web_contents() { return web_contents_; }

  content::TestWebContentsFactory& web_contents_factory() {
    return web_contents_factory_;
  }

  TestingProfile* profile() { return profile_; }

  scoped_refptr<PasswordsPrivateDelegateImpl> CreateDelegate() {
    return new PasswordsPrivateDelegateImpl(profile_);
  }

  content::BrowserTaskEnvironment& GetTaskEnvironment() {
    return task_environment_;
  }

 private:
  TestingProfileManager profile_manager_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  content::TestWebContentsFactory web_contents_factory_;
  scoped_refptr<TestPasswordStore> profile_store_;
  scoped_refptr<TestPasswordStore> account_store_;
  // Owned by |web_contents_factory_|
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<TestingProfile> profile_;
};

TEST_F(PasswordsPrivateDelegateImplMockTaskEnvironmentTest,
       AuthenticationTimeMetric) {
  content::WebContents* web_contents_ptr = web_contents();
  auto delegate = CreateDelegate();

  auto biometric_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  EXPECT_CALL(*biometric_authenticator, AuthenticateWithMessage)
      .WillOnce(testing::WithArg<1>(
          [this](PasswordsPrivateDelegateImpl::AuthResultCallback callback) {
            // Waiting for 10 seconds to simulate a long authentication process.
            GetTaskEnvironment().FastForwardBy(base::Seconds(10));
            std::move(callback).Run(/*successful=*/true);
          }));

  delegate->SetDeviceAuthenticatorForTesting(
      std::move(biometric_authenticator));

  MockRequestCredentialsDetailsCallback callback;
  EXPECT_CALL(callback, Run(testing::IsEmpty()));
  delegate->RequestCredentialsDetails({0}, callback.Get(), web_contents_ptr);

  histogram_tester().ExpectUniqueTimeSample(
      "PasswordManager.Settings.AuthenticationTime2", base::Seconds(10), 1);
}

TEST_F(PasswordsPrivateDelegateImplMockTaskEnvironmentTest,
       ClosingTabDuringExportDoesNotCrashChrome) {
  content::WebContents* web_contents_ptr =
      web_contents_factory().CreateWebContents(profile());
  auto delegate = CreateDelegate();

  auto biometric_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  device_reauth::DeviceAuthenticator::AuthenticateCallback auth_result_callback;
  EXPECT_CALL(*biometric_authenticator, AuthenticateWithMessage)
      .WillOnce(MoveArg<1>(&auth_result_callback));

  delegate->SetDeviceAuthenticatorForTesting(
      std::move(biometric_authenticator));

  base::MockCallback<base::OnceCallback<void(const std::string&)>> callback;
  delegate->ExportPasswords(callback.Get(), web_contents_ptr);

  // Simulate closing tab while authentication is still ongoing.
  web_contents_factory().DestroyWebContents(web_contents_ptr);

  // Now simulate auth is finished with success. Expect export to fail because
  // the tab is closed.
  EXPECT_CALL(callback, Run("reauth-failed"));
  std::move(auth_result_callback).Run(true);
}

#if !BUILDFLAG(IS_WIN)
TEST_F(PasswordsPrivateDelegateImplMockTaskEnvironmentTest,
       DestroyingDelegateWhileExportOngoing) {
  content::WebContents* web_contents_ptr =
      web_contents_factory().CreateWebContents(profile());
  auto delegate = CreateDelegate();

  auto biometric_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  auto* biometric_authenticator_ptr = biometric_authenticator.get();

  device_reauth::DeviceAuthenticator::AuthenticateCallback auth_result_callback;

  delegate->SetDeviceAuthenticatorForTesting(
      std::move(biometric_authenticator));

  EXPECT_CALL(*biometric_authenticator_ptr, AuthenticateWithMessage);
  base::MockCallback<base::OnceCallback<void(const std::string&)>> callback;
  delegate->ExportPasswords(callback.Get(), web_contents_ptr);

  // Simulate destroying delegate while authentication is still ongoing. It
  // should trigger cancelation of ongoing authentication.
  EXPECT_CALL(*biometric_authenticator_ptr, Cancel);
  delegate.reset();
}
#endif  // !BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)

class PasswordsPrivateDelegateImplFetchFamilyMembersTest
    : public PasswordsPrivateDelegateImplTest {
 public:
  PasswordsPrivateDelegateImplFetchFamilyMembersTest() = default;

  void SetUp() override {
    PasswordsPrivateDelegateImplTest::SetUp();
    delegate_ = CreateDelegate();
    delegate_->SetRecipientsFetcherForTesting(
        std::make_unique<password_manager::RecipientsFetcherImpl>(
            version_info::Channel::DEFAULT,
            profile_url_loader_factory().GetSafeWeakWrapper(),
            identity_test_env_.identity_manager()));
    identity_test_env_.MakePrimaryAccountAvailable("test@email.com",
                                                   signin::ConsentLevel::kSync);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  void TearDown() override {
    delegate_ = nullptr;
    PasswordsPrivateDelegateImplTest::TearDown();
  }

 protected:
  const std::string kTestUserId = "12345";
  const std::string kTestUserName = "Theo Tester";
  const std::string kTestEmail = "theo@example.com";
  const std::string kTestProfileImageUrl =
      "https://3837fjsdjaka.image.example.com";
  const std::string kTestPublicKeyBase64 =
      "MDEyMzQ1Njc4OTAxMjM0NTY3ODkwMTIzNDU2Nzg5MTI=";
  const uint32_t kTestPublicKeyVersion = 42;

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
    profile_url_loader_factory().AddResponse(
        password_manager::PasswordSharingRecipientsDownloader::
            GetPasswordSharingRecipientsURL(version_info::Channel::DEFAULT)
                .spec(),
        response.SerializeAsString(), status);
  }

  PasswordsPrivateDelegateImpl* delegate() { return delegate_.get(); }

 private:
  signin::IdentityTestEnvironment identity_test_env_;
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
  task_environment()->RunUntilIdle();
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
  task_environment()->RunUntilIdle();
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
  task_environment()->RunUntilIdle();
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
  task_environment()->RunUntilIdle();
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

  task_environment()->RunUntilIdle();
}

TEST_F(PasswordsPrivateDelegateImplFetchFamilyMembersTest,
       FetchFamilyMembersFailsWithNetworkError) {
  profile_url_loader_factory().AddResponse(
      password_manager::PasswordSharingRecipientsDownloader::
          GetPasswordSharingRecipientsURL(version_info::Channel::DEFAULT)
              .spec(),
      /*content=*/std::string(), net::HTTP_INTERNAL_SERVER_ERROR);

  base::MockCallback<PasswordsPrivateDelegate::FetchFamilyResultsCallback>
      callback;
  FamilyFetchResults family_fetch_results;
  EXPECT_CALL(
      callback,
      Run(AllOf(Field(&FamilyFetchResults::status,
                      api::passwords_private::FamilyFetchStatus::kUnknownError),
                Field(&FamilyFetchResults::family_members, IsEmpty()))));

  delegate()->FetchFamilyMembers(callback.Get());
  task_environment()->RunUntilIdle();
}

TEST_F(PasswordsPrivateDelegateImplTest, GetCredentialGroups_SyncOn) {
  sync_service()->SetSignedIn(signin::ConsentLevel::kSync);

  auto delegate = CreateDelegate();

  PasswordForm password =
      CreateSampleForm(PasswordForm::Store::kProfileStore, u"username2");

  SetUpPasswordStores({password});

  auto groups = delegate->GetCredentialGroups();
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
  auto delegate = CreateDelegate();

  PasswordForm password =
      CreateSampleForm(PasswordForm::Store::kProfileStore, u"username2");

  SetUpPasswordStores({password});

  auto groups = delegate->GetCredentialGroups();
  EXPECT_EQ(1u, groups.size());
  EXPECT_EQ(1u, groups[0].entries.size());
  EXPECT_EQ("abc1.com", groups[0].name);
  EXPECT_EQ("https://abc1.com/favicon.ico", groups[0].icon_url);
}

TEST_F(PasswordsPrivateDelegateImplTest, GetCredentialGroups_Butter) {
  signin::IdentityTestEnvironment identity_test_env_;
  identity_test_env_.MakePrimaryAccountAvailable("test@email.com",
                                                 signin::ConsentLevel::kSignin);
  identity_test_env_.SetAutomaticIssueOfAccessTokens(true);

  auto delegate = CreateDelegate();

  PasswordForm password =
      CreateSampleForm(PasswordForm::Store::kAccountStore, u"username2");

  SetUpPasswordStores({password});

  auto groups = delegate->GetCredentialGroups();
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
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  auto delegate = CreateDelegate();
  PasswordForm form_profile =
      CreateSampleForm(PasswordForm::Store::kProfileStore);
  PasswordForm form_account =
      CreateSampleForm(PasswordForm::Store::kAccountStore);
  SetUpPasswordStores({form_profile, form_account});
  task_environment()->RunUntilIdle();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(syncer::kSyncWebauthnCredentials);
  webauthn::PasskeyModel* passkey_model =
      PasskeyModelFactory::GetForProfile(profile());
  ASSERT_EQ(passkey_model, PasskeyModelFactory::GetForProfile(profile()));
  ASSERT_TRUE(passkey_model);
  sync_pb::WebauthnCredentialSpecifics passkey = CreatePasskey();
  passkey_model->AddNewPasskeyForTesting(passkey);

  EXPECT_THAT(profile_store_->stored_passwords(), testing::SizeIs(1));
  EXPECT_THAT(account_store_->stored_passwords(), testing::SizeIs(1));
  EXPECT_THAT(passkey_model->GetAllPasskeys(), SizeIs(1));

  ExpectAuthentication(delegate, /*successful=*/true);
  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(true));
  delegate->DeleteAllPasswordManagerData(web_contents.get(), callback.Get());
  task_environment()->RunUntilIdle();
  EXPECT_THAT(profile_store_->stored_passwords(), testing::IsEmpty());
  EXPECT_THAT(account_store_->stored_passwords(), testing::IsEmpty());
  EXPECT_THAT(passkey_model->GetAllPasskeys(), testing::IsEmpty());
}

TEST_F(PasswordsPrivateDelegateImplTest,
       DeleteAllDataRecordsPasswordRemovalReason) {
  base::test::ScopedFeatureList scoped_feature_list{
      syncer::kSyncWebauthnCredentials};
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  auto delegate = CreateDelegate();

  ExpectAuthentication(delegate, /*successful=*/true);
  base::test::TestFuture<bool> completion_future;
  delegate->DeleteAllPasswordManagerData(web_contents.get(),
                                         completion_future.GetCallback());
  ASSERT_TRUE(completion_future.Take());

  int expected_reason =
      1 << static_cast<int>(password_manager::metrics_util::
                                PasswordManagerCredentialRemovalReason::
                                    kDeleteAllPasswordManagerData);
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                password_manager::prefs::kPasswordRemovalReasonForAccount),
            expected_reason);
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                password_manager::prefs::kPasswordRemovalReasonForProfile),
            expected_reason);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
TEST_F(PasswordsPrivateDelegateImplTest, DeleteAllDataWithReauthFailed) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  auto delegate = CreateDelegate();
  PasswordForm form_profile =
      CreateSampleForm(PasswordForm::Store::kProfileStore);
  PasswordForm form_account =
      CreateSampleForm(PasswordForm::Store::kAccountStore);
  SetUpPasswordStores({form_profile, form_account});
  task_environment()->RunUntilIdle();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(syncer::kSyncWebauthnCredentials);
  webauthn::PasskeyModel* passkey_model =
      PasskeyModelFactory::GetForProfile(profile());
  ASSERT_EQ(passkey_model, PasskeyModelFactory::GetForProfile(profile()));
  ASSERT_TRUE(passkey_model);
  sync_pb::WebauthnCredentialSpecifics passkey = CreatePasskey();
  passkey_model->AddNewPasskeyForTesting(passkey);
  task_environment()->RunUntilIdle();

  ExpectAuthentication(delegate, /*successful=*/false);
  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(false));
  delegate->DeleteAllPasswordManagerData(web_contents.get(), callback.Get());
  task_environment()->RunUntilIdle();
  EXPECT_THAT(profile_store_->stored_passwords(), testing::SizeIs(1));
  EXPECT_THAT(account_store_->stored_passwords(), testing::SizeIs(1));
  EXPECT_THAT(passkey_model->GetAllPasskeys(), SizeIs(1));
}
#endif

}  // namespace extensions
