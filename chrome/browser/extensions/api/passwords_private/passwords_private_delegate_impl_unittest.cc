// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_impl.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/affiliation_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/passwords/settings/password_manager_porter_interface.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/webapps/chrome_webapps_client.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/password_manager/content/browser/password_manager_log_router_factory.h"
#include "components/password_manager/core/browser/affiliation/fake_affiliation_service.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/reauth_purpose.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/browser/ui/import_results.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/test_event_router.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/clipboard/test/test_clipboard.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#endif

using MockReauthCallback = base::MockCallback<
    password_manager::PasswordAccessAuthenticator::ReauthCallback>;
using password_manager::ReauthPurpose;
using password_manager::TestPasswordStore;
using ::testing::_;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Ne;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::StrictMock;
namespace extensions {

namespace {

constexpr char kHistogramName[] = "PasswordManager.AccessPasswordInSettings";

using MockPlaintextPasswordCallback =
    base::MockCallback<PasswordsPrivateDelegate::PlaintextPasswordCallback>;
using MockRequestCredentialsDetailsCallback =
    base::MockCallback<PasswordsPrivateDelegate::UiEntriesCallback>;

class MockPasswordManagerPorter : public PasswordManagerPorterInterface {
 public:
  MOCK_METHOD(bool, Export, (content::WebContents * web_contents), (override));
  MOCK_METHOD(void, CancelExport, (), (override));
  MOCK_METHOD(password_manager::ExportProgressStatus,
              GetExportProgressStatus,
              (),
              (override));
  MOCK_METHOD(void,
              Import,
              (content::WebContents * web_contents,
               password_manager::PasswordForm::Store to_store,
               ImportResultsCallback results_callback),
              (override));
  MOCK_METHOD(void,
              ContinueImport,
              (const std::vector<int>& selected_ids,
               ImportResultsCallback results_callback),
              (override));
  MOCK_METHOD(void, ResetImporter, (bool delete_file), (override));
};

class FakePasswordManagerPorter : public PasswordManagerPorterInterface {
 public:
  bool Export(content::WebContents* web_contents) override { return true; }

  void CancelExport() override {}

  password_manager::ExportProgressStatus GetExportProgressStatus() override {
    return password_manager::ExportProgressStatus::SUCCEEDED;
  }

  void Import(content::WebContents* web_contents,
              password_manager::PasswordForm::Store to_store,
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

  scoped_refptr<device_reauth::DeviceAuthenticator> GetDeviceAuthenticator()
      override {
    return biometric_authenticator_;
  }

  void SetDeviceAuthenticator(
      scoped_refptr<device_reauth::MockDeviceAuthenticator>
          biometric_authenticator) {
    biometric_authenticator_ = std::move(biometric_authenticator);
  }

 private:
  explicit MockPasswordManagerClient(content::WebContents* web_contents)
      : ChromePasswordManagerClient(web_contents, nullptr) {}

  password_manager::MockPasswordFeatureManager mock_password_feature_manager_;
  scoped_refptr<device_reauth::MockDeviceAuthenticator>
      biometric_authenticator_ = nullptr;
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
  CoreAccountInfo account;
  account.email = "foo@gmail.com";
  account.gaia = "foo";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);
  sync_service->SetAccountInfo(account);
  sync_service->SetDisableReasons({});
  sync_service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  sync_service->SetHasSyncConsent(false);
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
  return std::unique_ptr<KeyedService>(
      PasswordsPrivateEventRouter::Create(context));
}

password_manager::PasswordForm CreateSampleForm(
    password_manager::PasswordForm::Store store =
        password_manager::PasswordForm::Store::kProfileStore,
    const std::u16string& username = u"test@gmail.com") {
  password_manager::PasswordForm form;
  form.signon_realm = "https://abc1.com";
  form.url = GURL("https://abc1.com");
  form.username_value = username;
  form.password_value = u"test";
  form.in_store = store;
  return form;
}

MATCHER_P(PasswordUiEntryDataEquals, expected, "") {
  return testing::Value(expected.get().urls.link, arg.urls.link) &&
         testing::Value(expected.get().username, arg.username) &&
         testing::Value(expected.get().stored_in, arg.stored_in) &&
         testing::Value(expected.get().is_android_credential,
                        arg.is_android_credential);
}

}  // namespace

class PasswordsPrivateDelegateImplTest : public WebAppTest {
 public:
  PasswordsPrivateDelegateImplTest() = default;

  PasswordsPrivateDelegateImplTest(const PasswordsPrivateDelegateImplTest&) =
      delete;
  PasswordsPrivateDelegateImplTest& operator=(
      const PasswordsPrivateDelegateImplTest&) = delete;

  ~PasswordsPrivateDelegateImplTest() override;

  void SetUp() override;

  // Sets up a testing password store and fills it with |forms|.
  void SetUpPasswordStores(std::vector<password_manager::PasswordForm> forms);

  // Sets up a testing EventRouter with a production
  // PasswordsPrivateEventRouter.
  void SetUpRouters();

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  scoped_refptr<PasswordsPrivateDelegateImpl> CreateDelegate() {
    return new PasswordsPrivateDelegateImpl(profile());
  }

 protected:
  raw_ptr<extensions::TestEventRouter> event_router_ = nullptr;
  scoped_refptr<TestPasswordStore> profile_store_;
  scoped_refptr<TestPasswordStore> account_store_;
  raw_ptr<ui::TestClipboard> test_clipboard_;
  scoped_refptr<device_reauth::MockDeviceAuthenticator>
      biometric_authenticator_;

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
  biometric_authenticator_ =
      base::MakeRefCounted<device_reauth::MockDeviceAuthenticator>();
  AffiliationServiceFactory::GetInstance()->SetTestingSubclassFactoryAndUse(
      profile(), base::BindRepeating([](content::BrowserContext*) {
        return std::make_unique<password_manager::FakeAffiliationService>();
      }));
  SetUpRouters();
  SetUpSyncInTransportMode(profile());
}

void PasswordsPrivateDelegateImplTest::SetUpPasswordStores(
    std::vector<password_manager::PasswordForm> forms) {
  for (const password_manager::PasswordForm& form : forms) {
    if (form.IsUsingAccountStore())
      account_store_->AddLogin(form);
    else if (form.IsUsingProfileStore())
      profile_store_->AddLogin(form);
    else
      NOTREACHED() << "Store not set";
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

  password_manager::PasswordForm account_password =
      CreateSampleForm(password_manager::PasswordForm::Store::kAccountStore);
  password_manager::PasswordForm profile_password =
      CreateSampleForm(password_manager::PasswordForm::Store::kProfileStore);

  SetUpPasswordStores({account_password, profile_password});

  base::MockCallback<PasswordsPrivateDelegate::UiEntriesCallback> callback;
  EXPECT_CALL(callback, Run(SizeIs(1)))
      .WillOnce([&](const PasswordsPrivateDelegate::UiEntries& passwords) {
        EXPECT_EQ(api::passwords_private::PASSWORD_STORE_SET_DEVICE_AND_ACCOUNT,
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
  password_manager::PasswordForm account_exception;
  account_exception.blocked_by_user = true;
  account_exception.url = GURL("https://test.com");
  account_exception.in_store =
      password_manager::PasswordForm::Store::kAccountStore;
  password_manager::PasswordForm profile_exception;
  profile_exception.url = GURL("https://test.com");
  profile_exception.blocked_by_user = true;
  profile_exception.in_store =
      password_manager::PasswordForm::Store::kProfileStore;

  SetUpPasswordStores({account_exception, profile_exception});

  base::MockCallback<PasswordsPrivateDelegate::ExceptionEntriesCallback>
      callback;

  EXPECT_CALL(callback, Run(SizeIs(1)));
  delegate->GetPasswordExceptionsList(callback.Get());
}

TEST_F(PasswordsPrivateDelegateImplTest, AddPassword) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
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
  api::passwords_private::PasswordUiEntry expected_entry1;
  expected_entry1.urls.link = "https://example1.com/";
  expected_entry1.username = "username1";
  expected_entry1.note.emplace();
  expected_entry1.stored_in =
      api::passwords_private::PASSWORD_STORE_SET_ACCOUNT;
  api::passwords_private::PasswordUiEntry expected_entry2;
  expected_entry2.urls.link = "http://example2.com/login";
  expected_entry2.username = "";
  expected_entry2.note = "note";
  expected_entry2.stored_in = api::passwords_private::PASSWORD_STORE_SET_DEVICE;
  EXPECT_CALL(callback,
              Run(testing::UnorderedElementsAre(
                  PasswordUiEntryDataEquals(testing::ByRef(expected_entry1)),
                  PasswordUiEntryDataEquals(testing::ByRef(expected_entry2)))));
  delegate->GetSavedPasswordsList(callback.Get());
}

TEST_F(PasswordsPrivateDelegateImplTest, AddPasswordUpdatesDefaultStore) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
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
  EXPECT_CALL(*(client->GetPasswordFeatureManager()),
              SetDefaultPasswordStore(
                  password_manager::PasswordForm::Store::kAccountStore));
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

TEST_F(PasswordsPrivateDelegateImplTest,
       ImportPasswordsDoesNotUpdateDefaultStore) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  auto* client =
      MockPasswordManagerClient::CreateForWebContentsAndGet(web_contents.get());
  auto delegate = CreateDelegate();

  auto mock_porter = std::make_unique<MockPasswordManagerPorter>();
  auto* mock_porter_ptr = mock_porter.get();

  delegate->SetPorterForTesting(std::move(mock_porter));

  // NOT update default store if not opted-in for account storage.
  ON_CALL(*(client->GetPasswordFeatureManager()), IsOptedInForAccountStorage)
      .WillByDefault(Return(false));
  EXPECT_CALL(*(client->GetPasswordFeatureManager()), SetDefaultPasswordStore)
      .Times(0);
  EXPECT_CALL(*mock_porter_ptr, Import).Times(1);
  delegate->ImportPasswords(
      api::passwords_private::PasswordStoreSet::PASSWORD_STORE_SET_DEVICE,
      base::DoNothing(), web_contents.get());
}

TEST_F(PasswordsPrivateDelegateImplTest, ImportPasswordsUpdatesDefaultStore) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
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
              SetDefaultPasswordStore(
                  password_manager::PasswordForm::Store::kAccountStore));
  EXPECT_CALL(*mock_porter_ptr, Import).Times(1);
  delegate->ImportPasswords(
      api::passwords_private::PasswordStoreSet::PASSWORD_STORE_SET_ACCOUNT,
      base::DoNothing(), web_contents.get());
}

TEST_F(PasswordsPrivateDelegateImplTest,
       ImportPasswordsLogsImportResultsStatus) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(),
                                                        /*instance=*/nullptr);
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
  EXPECT_CALL(callback, Run(::testing::Field(
                            &api::passwords_private::ImportResults::status,
                            api::passwords_private::ImportResultsStatus::
                                IMPORT_RESULTS_STATUS_BAD_FORMAT)))
      .Times(1);
  delegate->ImportPasswords(
      api::passwords_private::PasswordStoreSet::PASSWORD_STORE_SET_ACCOUNT,
      callback.Get(), web_contents.get());
  task_environment()->RunUntilIdle();

  histogram_tester().ExpectUniqueSample("PasswordManager.ImportResultsStatus2",
                                        kExpectedStatus, 1);
}

TEST_F(PasswordsPrivateDelegateImplTest, TestReauthFailedOnImport) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(),
                                                        /*instance=*/nullptr);
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

  MockReauthCallback reauth_callback;
  delegate->set_os_reauth_call(reauth_callback.Get());

  EXPECT_CALL(reauth_callback, Run(ReauthPurpose::IMPORT, _))
      .WillOnce(testing::WithArg<1>(
          [](password_manager::PasswordAccessAuthenticator::AuthResultCallback
                 callback) { std::move(callback).Run(false); }));

  base::MockCallback<PasswordsPrivateDelegate::ImportResultsCallback>
      import_callback;
  EXPECT_CALL(
      import_callback,
      Run(::testing::Field(&api::passwords_private::ImportResults::status,
                           api::passwords_private::ImportResultsStatus::
                               IMPORT_RESULTS_STATUS_DISMISSED)))
      .Times(1);

  delegate->ContinueImport(/*selected_ids=*/{1}, import_callback.Get(),
                           web_contents.get());
  task_environment()->RunUntilIdle();

  histogram_tester().ExpectUniqueSample("PasswordManager.ImportResultsStatus2",
                                        kExpectedStatus, 1);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       ContinueImportLogsImportResultsStatus) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(),
                                                        /*instance=*/nullptr);
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
  EXPECT_CALL(callback, Run(::testing::Field(
                            &api::passwords_private::ImportResults::status,
                            api::passwords_private::ImportResultsStatus::
                                IMPORT_RESULTS_STATUS_BAD_FORMAT)))
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

TEST_F(PasswordsPrivateDelegateImplTest, ChangeSavedPassword) {
  password_manager::PasswordForm sample_form = CreateSampleForm();
  SetUpPasswordStores({sample_form});
  auto delegate = CreateDelegate();
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  // Double check that the contents of the passwords list matches our
  // expectation.
  base::MockCallback<PasswordsPrivateDelegate::UiEntriesCallback> callback;
  EXPECT_CALL(callback, Run(SizeIs(1)))
      .WillOnce([&](const PasswordsPrivateDelegate::UiEntries& passwords) {
        EXPECT_EQ(sample_form.username_value,
                  base::UTF8ToUTF16(passwords[0].username));
      });
  delegate->GetSavedPasswordsList(callback.Get());
  int sample_form_id = delegate->GetIdForCredential(
      password_manager::CredentialUIEntry(sample_form));

  api::passwords_private::ChangeSavedPasswordParams params;
  params.password = "new_pass";
  params.username = "new_user";
  params.note = "new note";

  sample_form.username_value = u"new_user";
  sample_form.password_value = u"new_pass";
  int new_form_id = delegate->GetIdForCredential(
      password_manager::CredentialUIEntry(sample_form));

  auto result = delegate->ChangeSavedPassword(sample_form_id, params);
  EXPECT_EQ(result, new_form_id);

  // Spin the loop to allow PasswordStore tasks posted when changing the
  // password to be completed.
  base::RunLoop().RunUntilIdle();

  // Check that the changing the password got reflected in the passwords list.
  // `note` field should not be filled when `GetSavedPasswordsList` is called.
  EXPECT_CALL(callback, Run(SizeIs(1)))
      .WillOnce([](const PasswordsPrivateDelegate::UiEntries& passwords) {
        EXPECT_THAT(passwords[0].username, Eq("new_user"));
        EXPECT_THAT(passwords[0].note, Eq(absl::nullopt));
      });
  delegate->GetSavedPasswordsList(callback.Get());
}

TEST_F(PasswordsPrivateDelegateImplTest, ChangeSavedPasswordInBothStores) {
  password_manager::PasswordForm profile_form = CreateSampleForm();
  password_manager::PasswordForm account_form = profile_form;
  account_form.in_store = password_manager::PasswordForm::Store::kAccountStore;
  SetUpPasswordStores({profile_form, account_form});

  auto delegate = CreateDelegate();
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  int profile_form_id = delegate->GetIdForCredential(
      password_manager::CredentialUIEntry(profile_form));
  int account_form_id = delegate->GetIdForCredential(
      password_manager::CredentialUIEntry(account_form));

  ASSERT_EQ(profile_form_id, account_form_id);

  api::passwords_private::ChangeSavedPasswordParams params;
  params.password = "new_pass";
  params.username = "new_user";

  profile_form.username_value = u"new_user";
  profile_form.password_value = u"new_pass";
  int new_profile_form_id = delegate->GetIdForCredential(
      password_manager::CredentialUIEntry(profile_form));
  account_form.username_value = u"new_user";
  account_form.password_value = u"new_pass";
  int new_account_form_id = delegate->GetIdForCredential(
      password_manager::CredentialUIEntry(account_form));

  ASSERT_EQ(new_profile_form_id, new_account_form_id);

  EXPECT_EQ(new_profile_form_id,
            delegate->ChangeSavedPassword(profile_form_id, params));
}

TEST_F(PasswordsPrivateDelegateImplTest, ChangeSavedPasswordInAccountStore) {
  password_manager::PasswordForm profile_form = CreateSampleForm();
  profile_form.password_value = u"different_pass";
  password_manager::PasswordForm account_form = CreateSampleForm();
  account_form.in_store = password_manager::PasswordForm::Store::kAccountStore;
  SetUpPasswordStores({profile_form, account_form});

  auto delegate = CreateDelegate();
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  int account_form_id = delegate->GetIdForCredential(
      password_manager::CredentialUIEntry(account_form));

  api::passwords_private::ChangeSavedPasswordParams params;
  params.password = "new_pass";
  params.username = "new_user";

  account_form.username_value = u"new_user";
  account_form.password_value = u"new_pass";
  int new_account_form_id = delegate->GetIdForCredential(
      password_manager::CredentialUIEntry(account_form));

  auto result = delegate->ChangeSavedPassword(account_form_id, params);
  EXPECT_THAT(result, new_account_form_id);
}

// Checking callback result of RequestPlaintextPassword with reason Copy.
// By implementation for Copy, callback will receive empty string.
TEST_F(PasswordsPrivateDelegateImplTest, TestCopyPasswordCallbackResult) {
  password_manager::PasswordForm form = CreateSampleForm();
  SetUpPasswordStores({form});

  auto delegate = CreateDelegate();
  base::RunLoop().RunUntilIdle();

  MockReauthCallback callback;
  delegate->set_os_reauth_call(callback.Get());

  EXPECT_CALL(callback, Run(ReauthPurpose::COPY_PASSWORD, _))
      .WillOnce(testing::WithArg<1>(
          [&](password_manager::PasswordAccessAuthenticator::AuthResultCallback
                  callback) { std::move(callback).Run(true); }));

  MockPlaintextPasswordCallback password_callback;
  EXPECT_CALL(password_callback, Run(Eq(std::u16string())));
  delegate->RequestPlaintextPassword(
      0, api::passwords_private::PLAINTEXT_REASON_COPY, password_callback.Get(),
      nullptr);

  std::u16string result;
  test_clipboard_->ReadText(ui::ClipboardBuffer::kCopyPaste,
                            /* data_dst = */ nullptr, &result);
  EXPECT_EQ(form.password_value, result);

  histogram_tester().ExpectUniqueSample(
      kHistogramName, password_manager::metrics_util::ACCESS_PASSWORD_COPIED,
      1);
}

TEST_F(PasswordsPrivateDelegateImplTest, TestShouldReauthForOptIn) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  auto* client =
      MockPasswordManagerClient::CreateForWebContentsAndGet(web_contents.get());
  ON_CALL(*(client->GetPasswordFeatureManager()), IsOptedInForAccountStorage)
      .WillByDefault(Return(false));

  EXPECT_CALL(*client,
              TriggerReauthForPrimaryAccount(
                  signin_metrics::ReauthAccessPoint::kPasswordSettings, _));

  auto delegate = CreateDelegate();
  delegate->SetAccountStorageOptIn(true, web_contents.get());
}

TEST_F(PasswordsPrivateDelegateImplTest,
       TestShouldNotReauthForOptOutAndShouldSetPref) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
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
  EXPECT_CALL(*feature_manager, OptOutOfAccountStorageAndClearSettings);

  auto delegate = CreateDelegate();
  delegate->SetAccountStorageOptIn(false, web_contents.get());
}

TEST_F(PasswordsPrivateDelegateImplTest, TestCopyPasswordCallbackResultFail) {
  SetUpPasswordStores({CreateSampleForm()});

  auto delegate = CreateDelegate();
  base::RunLoop().RunUntilIdle();

  MockReauthCallback callback;
  delegate->set_os_reauth_call(callback.Get());

  EXPECT_CALL(callback, Run(ReauthPurpose::COPY_PASSWORD, _))
      .WillOnce(testing::WithArg<1>(
          [&](password_manager::PasswordAccessAuthenticator::AuthResultCallback
                  callback) { std::move(callback).Run(false); }));

  base::Time before_call = test_clipboard_->GetLastModifiedTime();

  MockPlaintextPasswordCallback password_callback;
  EXPECT_CALL(password_callback, Run(Eq(absl::nullopt)));
  delegate->RequestPlaintextPassword(
      0, api::passwords_private::PLAINTEXT_REASON_COPY, password_callback.Get(),
      nullptr);
  // Clipboard should not be modified in case Reauth failed
  std::u16string result;
  test_clipboard_->ReadText(ui::ClipboardBuffer::kCopyPaste,
                            /* data_dst = */ nullptr, &result);
  EXPECT_EQ(std::u16string(), result);
  EXPECT_EQ(before_call, test_clipboard_->GetLastModifiedTime());

  // Since Reauth had failed password was not copied and metric wasn't recorded
  histogram_tester().ExpectTotalCount(kHistogramName, 0);
}

TEST_F(PasswordsPrivateDelegateImplTest, TestPassedReauthOnView) {
  SetUpPasswordStores({CreateSampleForm()});

  auto delegate = CreateDelegate();
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  MockReauthCallback callback;
  delegate->set_os_reauth_call(callback.Get());

  EXPECT_CALL(callback, Run(ReauthPurpose::VIEW_PASSWORD, _))
      .WillOnce(testing::WithArg<1>(
          [&](password_manager::PasswordAccessAuthenticator::AuthResultCallback
                  callback) { std::move(callback).Run(true); }));

  MockPlaintextPasswordCallback password_callback;
  EXPECT_CALL(password_callback, Run(Eq(u"test")));
  delegate->RequestPlaintextPassword(
      0, api::passwords_private::PLAINTEXT_REASON_VIEW, password_callback.Get(),
      nullptr);

  histogram_tester().ExpectUniqueSample(
      kHistogramName, password_manager::metrics_util::ACCESS_PASSWORD_VIEWED,
      1);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       TestPassedReauthOnRequestCredentialsDetails) {
  password_manager::PasswordForm sample_form = CreateSampleForm();
  sample_form.notes.emplace_back(u"best note ever",
                                 /*date_created=*/base::Time::Now());
  SetUpPasswordStores({sample_form});

  auto delegate = CreateDelegate();
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  MockReauthCallback callback;
  delegate->set_os_reauth_call(callback.Get());

  EXPECT_CALL(callback, Run(ReauthPurpose::VIEW_PASSWORD, _))
      .WillOnce(testing::WithArg<1>(
          [&](password_manager::PasswordAccessAuthenticator::AuthResultCallback
                  callback) { std::move(callback).Run(true); }));

  MockRequestCredentialsDetailsCallback password_callback;
  EXPECT_CALL(password_callback, Run)
      .WillOnce([&](const std::vector<api::passwords_private::PasswordUiEntry>&
                        entries) {
        EXPECT_EQ(1u, entries.size());
        EXPECT_THAT(entries[0].password, Eq("test"));
        EXPECT_THAT(entries[0].username, Eq("test@gmail.com"));
        EXPECT_THAT(entries[0].note, Eq("best note ever"));
      });

  delegate->RequestCredentialsDetails({0}, password_callback.Get(), nullptr);

  histogram_tester().ExpectUniqueSample(
      kHistogramName, password_manager::metrics_util::ACCESS_PASSWORD_VIEWED,
      1);
}

TEST_F(PasswordsPrivateDelegateImplTest, TestFailedReauthOnView) {
  SetUpPasswordStores({CreateSampleForm()});

  auto delegate = CreateDelegate();
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  MockReauthCallback callback;
  delegate->set_os_reauth_call(callback.Get());

  EXPECT_CALL(callback, Run(ReauthPurpose::VIEW_PASSWORD, _))
      .WillOnce(testing::WithArg<1>(
          [&](password_manager::PasswordAccessAuthenticator::AuthResultCallback
                  callback) { std::move(callback).Run(false); }));

  MockPlaintextPasswordCallback password_callback;
  EXPECT_CALL(password_callback, Run(Eq(absl::nullopt)));
  delegate->RequestPlaintextPassword(
      0, api::passwords_private::PLAINTEXT_REASON_VIEW, password_callback.Get(),
      nullptr);

  // Since Reauth had failed password was not viewed and metric wasn't recorded
  histogram_tester().ExpectTotalCount(kHistogramName, 0);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       TestFailedReauthOnRequestCredentialsDetails) {
  SetUpPasswordStores({CreateSampleForm()});

  auto delegate = CreateDelegate();
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  MockReauthCallback callback;
  delegate->set_os_reauth_call(callback.Get());

  EXPECT_CALL(callback, Run(ReauthPurpose::VIEW_PASSWORD, _))
      .WillOnce(testing::WithArg<1>(
          [&](password_manager::PasswordAccessAuthenticator::AuthResultCallback
                  callback) { std::move(callback).Run(false); }));

  MockRequestCredentialsDetailsCallback password_callback;
  EXPECT_CALL(password_callback, Run(testing::IsEmpty()));
  delegate->RequestCredentialsDetails({0}, password_callback.Get(), nullptr);

  // Since Reauth had failed password was not viewed and metric wasn't recorded
  histogram_tester().ExpectTotalCount(kHistogramName, 0);
}

TEST_F(PasswordsPrivateDelegateImplTest, TestReauthFailedOnExport) {
  SetUpPasswordStores({CreateSampleForm()});
  StrictMock<base::MockCallback<base::OnceCallback<void(const std::string&)>>>
      mock_accepted;

  auto delegate = CreateDelegate();
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_accepted, Run(std::string("reauth-failed")));

  MockReauthCallback callback;
  delegate->set_os_reauth_call(callback.Get());

  EXPECT_CALL(callback, Run(ReauthPurpose::EXPORT, _))
      .WillOnce(testing::WithArg<1>(
          [&](password_manager::PasswordAccessAuthenticator::AuthResultCallback
                  callback) { std::move(callback).Run(false); }));
  delegate->ExportPasswords(mock_accepted.Get(), nullptr);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       GetUrlCollectionValueWithSchemeWhenIpAddress) {
  auto delegate = CreateDelegate();
  const absl::optional<api::passwords_private::UrlCollection> urls =
      delegate->GetUrlCollection("127.0.0.1");
  EXPECT_TRUE(urls.has_value());
  EXPECT_EQ("127.0.0.1", urls.value().shown);
  EXPECT_EQ("http://127.0.0.1/", urls.value().signon_realm);
  EXPECT_EQ("http://127.0.0.1/", urls.value().link);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       GetUrlCollectionValueWithSchemeWhenWebAddress) {
  auto delegate = CreateDelegate();
  const absl::optional<api::passwords_private::UrlCollection> urls =
      delegate->GetUrlCollection("example.com/login");
  EXPECT_TRUE(urls.has_value());
  EXPECT_EQ("example.com", urls.value().shown);
  EXPECT_EQ("https://example.com/", urls.value().signon_realm);
  EXPECT_EQ("https://example.com/login", urls.value().link);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       GetUrlCollectionStrippedValueWhenFullUrl) {
  auto delegate = CreateDelegate();
  const absl::optional<api::passwords_private::UrlCollection> urls =
      delegate->GetUrlCollection(
          "http://username:password@example.com/login?param=value#ref");
  EXPECT_TRUE(urls.has_value());
  EXPECT_EQ("example.com", urls.value().shown);
  EXPECT_EQ("http://example.com/", urls.value().signon_realm);
  EXPECT_EQ("http://example.com/login", urls.value().link);
}

TEST_F(PasswordsPrivateDelegateImplTest,
       GetUrlCollectionNoValueWhenUnsupportedScheme) {
  auto delegate = CreateDelegate();
  const absl::optional<api::passwords_private::UrlCollection> urls =
      delegate->GetUrlCollection("scheme://unsupported");
  EXPECT_FALSE(urls.has_value());
}

TEST_F(PasswordsPrivateDelegateImplTest,
       GetUrlCollectionNoValueWhenInvalidUrl) {
  auto delegate = CreateDelegate();
  const absl::optional<api::passwords_private::UrlCollection> urls =
      delegate->GetUrlCollection("https://;/invalid");
  EXPECT_FALSE(urls.has_value());
}

TEST_F(PasswordsPrivateDelegateImplTest, IsAccountStoreDefault) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  auto* client =
      MockPasswordManagerClient::CreateForWebContentsAndGet(web_contents.get());
  ON_CALL(*(client->GetPasswordFeatureManager()), IsOptedInForAccountStorage)
      .WillByDefault(Return(true));

  auto delegate = CreateDelegate();

  EXPECT_CALL(*(client->GetPasswordFeatureManager()), GetDefaultPasswordStore)
      .WillOnce(Return(password_manager::PasswordForm::Store::kAccountStore));
  EXPECT_TRUE(delegate->IsAccountStoreDefault(web_contents.get()));

  EXPECT_CALL(*(client->GetPasswordFeatureManager()), GetDefaultPasswordStore)
      .WillOnce(Return(password_manager::PasswordForm::Store::kProfileStore));
  EXPECT_FALSE(delegate->IsAccountStoreDefault(web_contents.get()));
}

TEST_F(PasswordsPrivateDelegateImplTest, TestMovePasswordsToAccountStore) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  auto* client =
      MockPasswordManagerClient::CreateForWebContentsAndGet(web_contents.get());
  ON_CALL(*(client->GetPasswordFeatureManager()), IsOptedInForAccountStorage)
      .WillByDefault(Return(true));

  auto delegate = CreateDelegate();
  password_manager::PasswordForm form1 =
      CreateSampleForm(password_manager::PasswordForm::Store::kProfileStore);
  password_manager::PasswordForm form2 = form1;
  form2.username_value = u"different_username";

  SetUpPasswordStores({form1, form2});

  int first_id =
      delegate->GetIdForCredential(password_manager::CredentialUIEntry(form1));
  int second_id =
      delegate->GetIdForCredential(password_manager::CredentialUIEntry(form2));

  delegate->MovePasswordsToAccount({first_id, second_id}, web_contents.get());
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.AccountStorage.MoveToAccountStoreFlowAccepted2",
      password_manager::metrics_util::MoveToAccountStoreTrigger::
          kExplicitlyTriggeredForMultiplePasswordsInSettings,
      1);
}

TEST_F(PasswordsPrivateDelegateImplTest, AndroidCredential) {
  auto delegate = CreateDelegate();

  password_manager::PasswordForm android_form;
  android_form.signon_realm = "android://hash@example.com";
  android_form.username_value = u"test@gmail.com";
  android_form.in_store = password_manager::PasswordForm::Store::kProfileStore;
  SetUpPasswordStores({android_form});

  base::MockCallback<PasswordsPrivateDelegate::UiEntriesCallback> callback;

  api::passwords_private::PasswordUiEntry expected_entry;
  expected_entry.urls.link =
      "https://play.google.com/store/apps/details?id=example.com";
  expected_entry.username = "test@gmail.com";
  expected_entry.is_android_credential = true;
  expected_entry.stored_in = api::passwords_private::PASSWORD_STORE_SET_DEVICE;
  EXPECT_CALL(callback, Run(testing::ElementsAre(PasswordUiEntryDataEquals(
                            testing::ByRef(expected_entry)))));
  delegate->GetSavedPasswordsList(callback.Get());
}

TEST_F(PasswordsPrivateDelegateImplTest, VerifyCastingOfImportEntryStatus) {
  static_assert(
      static_cast<int>(api::passwords_private::ImportEntryStatus::
                           IMPORT_ENTRY_STATUS_NONE) ==
          static_cast<int>(password_manager::ImportEntry::Status::NONE),
      "");
  static_assert(static_cast<int>(api::passwords_private::ImportEntryStatus::
                                     IMPORT_ENTRY_STATUS_UNKNOWN_ERROR) ==
                    static_cast<int>(
                        password_manager::ImportEntry::Status::UNKNOWN_ERROR),
                "");
  static_assert(
      static_cast<int>(api::passwords_private::ImportEntryStatus::
                           IMPORT_ENTRY_STATUS_MISSING_PASSWORD) ==
          static_cast<int>(
              password_manager::ImportEntry::Status::MISSING_PASSWORD),
      "");
  static_assert(
      static_cast<int>(api::passwords_private::ImportEntryStatus::
                           IMPORT_ENTRY_STATUS_MISSING_URL) ==
          static_cast<int>(password_manager::ImportEntry::Status::MISSING_URL),
      "");
  static_assert(
      static_cast<int>(api::passwords_private::ImportEntryStatus::
                           IMPORT_ENTRY_STATUS_INVALID_URL) ==
          static_cast<int>(password_manager::ImportEntry::Status::INVALID_URL),
      "");
  static_assert(
      static_cast<int>(api::passwords_private::ImportEntryStatus::
                           IMPORT_ENTRY_STATUS_LONG_URL) ==
          static_cast<int>(password_manager::ImportEntry::Status::LONG_URL),
      "");
  static_assert(static_cast<int>(api::passwords_private::ImportEntryStatus::
                                     IMPORT_ENTRY_STATUS_LONG_PASSWORD) ==
                    static_cast<int>(
                        password_manager::ImportEntry::Status::LONG_PASSWORD),
                "");
  static_assert(static_cast<int>(api::passwords_private::ImportEntryStatus::
                                     IMPORT_ENTRY_STATUS_LONG_USERNAME) ==
                    static_cast<int>(
                        password_manager::ImportEntry::Status::LONG_USERNAME),
                "");
  static_assert(
      static_cast<int>(api::passwords_private::ImportEntryStatus::
                           IMPORT_ENTRY_STATUS_CONFLICT_PROFILE) ==
          static_cast<int>(
              password_manager::ImportEntry::Status::CONFLICT_PROFILE),
      "");
  static_assert(
      static_cast<int>(api::passwords_private::ImportEntryStatus::
                           IMPORT_ENTRY_STATUS_CONFLICT_ACCOUNT) ==
          static_cast<int>(
              password_manager::ImportEntry::Status::CONFLICT_ACCOUNT),
      "");
  static_assert(
      static_cast<int>(api::passwords_private::ImportEntryStatus::
                           IMPORT_ENTRY_STATUS_LONG_NOTE) ==
          static_cast<int>(password_manager::ImportEntry::Status::LONG_NOTE),
      "");
  static_assert(
      static_cast<int>(api::passwords_private::ImportEntryStatus::
                           IMPORT_ENTRY_STATUS_LONG_CONCATENATED_NOTE) ==
          static_cast<int>(
              password_manager::ImportEntry::Status::LONG_CONCATENATED_NOTE),
      "");
  static_assert(
      static_cast<int>(api::passwords_private::ImportEntryStatus::
                           IMPORT_ENTRY_STATUS_VALID) ==
          static_cast<int>(password_manager::ImportEntry::Status::VALID),
      "");
}

TEST_F(PasswordsPrivateDelegateImplTest, VerifyCastingOfImportResultsStatus) {
  static_assert(
      static_cast<int>(api::passwords_private::ImportResultsStatus::
                           IMPORT_RESULTS_STATUS_NONE) ==
          static_cast<int>(password_manager::ImportResults::Status::NONE),
      "");
  static_assert(static_cast<int>(api::passwords_private::ImportResultsStatus::
                                     IMPORT_RESULTS_STATUS_UNKNOWN_ERROR) ==
                    static_cast<int>(
                        password_manager::ImportResults::Status::UNKNOWN_ERROR),
                "");
  static_assert(
      static_cast<int>(api::passwords_private::ImportResultsStatus::
                           IMPORT_RESULTS_STATUS_SUCCESS) ==
          static_cast<int>(password_manager::ImportResults::Status::SUCCESS),
      "");
  static_assert(
      static_cast<int>(api::passwords_private::ImportResultsStatus::
                           IMPORT_RESULTS_STATUS_IO_ERROR) ==
          static_cast<int>(password_manager::ImportResults::Status::IO_ERROR),
      "");
  static_assert(
      static_cast<int>(api::passwords_private::ImportResultsStatus::
                           IMPORT_RESULTS_STATUS_BAD_FORMAT) ==
          static_cast<int>(password_manager::ImportResults::Status::BAD_FORMAT),
      "");
  static_assert(
      static_cast<int>(api::passwords_private::ImportResultsStatus::
                           IMPORT_RESULTS_STATUS_DISMISSED) ==
          static_cast<int>(password_manager::ImportResults::Status::DISMISSED),
      "");
  static_assert(static_cast<int>(api::passwords_private::ImportResultsStatus::
                                     IMPORT_RESULTS_STATUS_MAX_FILE_SIZE) ==
                    static_cast<int>(
                        password_manager::ImportResults::Status::MAX_FILE_SIZE),
                "");
  static_assert(
      static_cast<int>(api::passwords_private::ImportResultsStatus::
                           IMPORT_RESULTS_STATUS_IMPORT_ALREADY_ACTIVE) ==
          static_cast<int>(
              password_manager::ImportResults::Status::IMPORT_ALREADY_ACTIVE),
      "");
  static_assert(
      static_cast<int>(api::passwords_private::ImportResultsStatus::
                           IMPORT_RESULTS_STATUS_NUM_PASSWORDS_EXCEEDED) ==
          static_cast<int>(
              password_manager::ImportResults::Status::NUM_PASSWORDS_EXCEEDED),
      "");
  static_assert(
      static_cast<int>(api::passwords_private::ImportResultsStatus::
                           IMPORT_RESULTS_STATUS_CONFLICTS) ==
          static_cast<int>(password_manager::ImportResults::Status::CONFLICTS),
      "");
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// Checks if authentication is triggered.
TEST_F(PasswordsPrivateDelegateImplTest,
       SwitchBiometricAuthBeforeFillingState) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kBiometricAuthenticationForFilling);
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  auto* client =
      MockPasswordManagerClient::CreateForWebContentsAndGet(web_contents.get());
  client->SetDeviceAuthenticator(biometric_authenticator_);
  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling, false);
  EXPECT_CALL(*biometric_authenticator_.get(), AuthenticateWithMessage)
      .WillOnce(base::test::RunOnceCallback<1>(/*successful=*/true));
  auto delegate = CreateDelegate();
  delegate->SwitchBiometricAuthBeforeFillingState(web_contents.get());
  // Expects that the switch value will change.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling));
}

// Checks if authentication is triggered.
TEST_F(PasswordsPrivateDelegateImplTest,
       SwitchBiometricAuthBeforeFillingCancelsLastTry) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kBiometricAuthenticationForFilling);
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  auto* client =
      MockPasswordManagerClient::CreateForWebContentsAndGet(web_contents.get());
  client->SetDeviceAuthenticator(biometric_authenticator_);

  auto delegate = CreateDelegate();
  EXPECT_CALL(*biometric_authenticator_.get(), AuthenticateWithMessage);
  delegate->SwitchBiometricAuthBeforeFillingState(web_contents.get());

  // Invoking authentication again will cancel previous request.
  EXPECT_CALL(*biometric_authenticator_.get(), Cancel);
  EXPECT_CALL(*biometric_authenticator_.get(), AuthenticateWithMessage);
  delegate->SwitchBiometricAuthBeforeFillingState(web_contents.get());
}

#endif

TEST_F(PasswordsPrivateDelegateImplTest, ShowAddShortcutDialog) {
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

  webapps::ChromeWebappsClient::GetInstance();
  auto* provider = web_app::FakeWebAppProvider::Get(profile());
  provider->SetDefaultFakeSubsystems();
  provider->StartWithSubsystems();
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
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordsGrouping);

  auto delegate = CreateDelegate();

  password_manager::PasswordForm password1 = CreateSampleForm(
      password_manager::PasswordForm::Store::kProfileStore, u"username1");
  password_manager::PasswordForm password2 = CreateSampleForm(
      password_manager::PasswordForm::Store::kProfileStore, u"username2");

  SetUpPasswordStores({password1, password2});

  auto groups = delegate->GetCredentialGroups();
  EXPECT_EQ(1u, groups.size());
  EXPECT_EQ(2u, groups[0].entries.size());
  EXPECT_EQ("abc1.com", groups[0].name);
  EXPECT_EQ("https://abc1.com/favicon.ico", groups[0].icon_url);

  api::passwords_private::PasswordUiEntry expected_entry1;
  expected_entry1.urls.link = "https://abc1.com/";
  expected_entry1.username = "username1";
  expected_entry1.stored_in = api::passwords_private::PASSWORD_STORE_SET_DEVICE;
  api::passwords_private::PasswordUiEntry expected_entry2;
  expected_entry2.urls.link = "https://abc1.com/";
  expected_entry2.username = "username2";
  expected_entry2.stored_in = api::passwords_private::PASSWORD_STORE_SET_DEVICE;
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

}  // namespace extensions
