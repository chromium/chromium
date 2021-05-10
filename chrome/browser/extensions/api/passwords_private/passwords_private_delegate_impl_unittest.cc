// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_impl.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/content/browser/password_manager_log_router_factory.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/reauth_purpose.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/test_event_router.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/test/test_clipboard.h"

using MockReauthCallback = base::MockCallback<
    password_manager::PasswordAccessAuthenticator::ReauthCallback>;
using PasswordFormList =
    std::vector<std::unique_ptr<password_manager::PasswordForm>>;
using password_manager::ReauthPurpose;
using password_manager::TestPasswordStore;
using ::testing::_;
using ::testing::Eq;
using ::testing::Ne;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::StrictMock;
namespace extensions {

namespace {

constexpr char kHistogramName[] = "PasswordManager.AccessPasswordInSettings";

using MockPlaintextPasswordCallback =
    base::MockCallback<PasswordsPrivateDelegate::PlaintextPasswordCallback>;

scoped_refptr<TestPasswordStore> CreateAndUseTestAccountPasswordStore(
    Profile* profile) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kEnablePasswordsAccountStorage)) {
    return nullptr;
  }
  return base::WrapRefCounted(static_cast<TestPasswordStore*>(
      AccountPasswordStoreFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              profile,
              base::BindRepeating(&password_manager::BuildPasswordStore<
                                  content::BrowserContext, TestPasswordStore>))
          .get()));
}

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
      : ChromePasswordManagerClient(web_contents, nullptr) {}

  password_manager::MockPasswordFeatureManager mock_password_feature_manager_;
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

class PasswordEventObserver
    : public extensions::TestEventRouter::EventObserver {
 public:
  // The observer will only listen to events with the |event_name|.
  explicit PasswordEventObserver(const std::string& event_name);

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

  DISALLOW_COPY_AND_ASSIGN(PasswordEventObserver);
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
  event_args_ = event.event_args->Clone();
}

std::unique_ptr<KeyedService> BuildPasswordsPrivateEventRouter(
    content::BrowserContext* context) {
  return std::unique_ptr<KeyedService>(
      PasswordsPrivateEventRouter::Create(context));
}

password_manager::PasswordForm CreateSampleForm() {
  password_manager::PasswordForm form;
  form.signon_realm = "http://abc1.com";
  form.url = GURL("http://abc1.com");
  form.username_value = u"test@gmail.com";
  form.password_value = u"test";
  return form;
}

}  // namespace

class PasswordsPrivateDelegateImplTest : public testing::Test {
 public:
  PasswordsPrivateDelegateImplTest();
  ~PasswordsPrivateDelegateImplTest() override;

  // Sets up a testing password store and fills it with |forms|.
  void SetUpPasswordStore(std::vector<password_manager::PasswordForm> forms);

  // Sets up a testing EventRouter with a production
  // PasswordsPrivateEventRouter.
  void SetUpRouters();

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  extensions::TestEventRouter* event_router_ = nullptr;
  scoped_refptr<TestPasswordStore> store_ =
      CreateAndUseTestPasswordStore(&profile_);
  scoped_refptr<TestPasswordStore> account_store_ =
      CreateAndUseTestAccountPasswordStore(&profile_);
  ui::TestClipboard* test_clipboard_ =
      ui::TestClipboard::CreateForCurrentThread();

 private:
  base::HistogramTester histogram_tester_;
  DISALLOW_COPY_AND_ASSIGN(PasswordsPrivateDelegateImplTest);
};

PasswordsPrivateDelegateImplTest::PasswordsPrivateDelegateImplTest() {
  SetUpRouters();
}

PasswordsPrivateDelegateImplTest::~PasswordsPrivateDelegateImplTest() {
  ui::Clipboard::DestroyClipboardForCurrentThread();
}

void PasswordsPrivateDelegateImplTest::SetUpPasswordStore(
    std::vector<password_manager::PasswordForm> forms) {
  for (const password_manager::PasswordForm& form : forms) {
    store_->AddLogin(form);
  }
  // Spin the loop to allow PasswordStore tasks being processed.
  base::RunLoop().RunUntilIdle();
}

void PasswordsPrivateDelegateImplTest::SetUpRouters() {
  event_router_ = extensions::CreateAndUseTestEventRouter(&profile_);
  // Set the production PasswordsPrivateEventRouter::Create as a testing
  // factory, because at some point during the preceding initialization, a null
  // factory is set, resulting in nul PasswordsPrivateEventRouter.
  PasswordsPrivateEventRouterFactory::GetInstance()->SetTestingFactory(
      &profile_, base::BindRepeating(&BuildPasswordsPrivateEventRouter));
}

TEST_F(PasswordsPrivateDelegateImplTest, GetSavedPasswordsList) {
  PasswordsPrivateDelegateImpl delegate(&profile_);

  base::MockCallback<PasswordsPrivateDelegate::UiEntriesCallback> callback;
  EXPECT_CALL(callback, Run).Times(0);
  delegate.GetSavedPasswordsList(callback.Get());

  PasswordFormList list;
  list.push_back(std::make_unique<password_manager::PasswordForm>());

  EXPECT_CALL(callback, Run);
  delegate.SetPasswordList(list);

  EXPECT_CALL(callback, Run);
  delegate.GetSavedPasswordsList(callback.Get());
}

TEST_F(PasswordsPrivateDelegateImplTest,
       PasswordsDuplicatedInStoresHaveSameFrontendId) {
  PasswordsPrivateDelegateImpl delegate(&profile_);

  auto account_password = std::make_unique<password_manager::PasswordForm>();
  account_password->in_store =
      password_manager::PasswordForm::Store::kAccountStore;
  auto profile_password = std::make_unique<password_manager::PasswordForm>();
  profile_password->in_store =
      password_manager::PasswordForm::Store::kProfileStore;

  PasswordFormList list;
  list.push_back(std::move(account_password));
  list.push_back(std::move(profile_password));

  delegate.SetPasswordList(list);

  base::MockCallback<PasswordsPrivateDelegate::UiEntriesCallback> callback;
  int first_frontend_id, second_frontend_id;
  EXPECT_CALL(callback, Run(SizeIs(2)))
      .WillOnce([&](const PasswordsPrivateDelegate::UiEntries& passwords) {
        first_frontend_id = passwords[0].frontend_id;
        second_frontend_id = passwords[1].frontend_id;
      });

  delegate.GetSavedPasswordsList(callback.Get());

  EXPECT_EQ(first_frontend_id, second_frontend_id);
}

TEST_F(PasswordsPrivateDelegateImplTest, GetPasswordExceptionsList) {
  PasswordsPrivateDelegateImpl delegate(&profile_);

  base::MockCallback<PasswordsPrivateDelegate::ExceptionEntriesCallback>
      callback;
  EXPECT_CALL(callback, Run).Times(0);
  delegate.GetPasswordExceptionsList(callback.Get());

  PasswordFormList list;
  list.push_back(std::make_unique<password_manager::PasswordForm>());

  EXPECT_CALL(callback, Run);
  delegate.SetPasswordExceptionList(list);

  EXPECT_CALL(callback, Run);
  delegate.GetPasswordExceptionsList(callback.Get());
}

TEST_F(PasswordsPrivateDelegateImplTest,
       ExceptionsDuplicatedInStoresHaveSameFrontendId) {
  PasswordsPrivateDelegateImpl delegate(&profile_);

  auto account_exception = std::make_unique<password_manager::PasswordForm>();
  account_exception->blocked_by_user = true;
  account_exception->in_store =
      password_manager::PasswordForm::Store::kAccountStore;
  auto profile_exception = std::make_unique<password_manager::PasswordForm>();
  profile_exception->blocked_by_user = true;
  profile_exception->in_store =
      password_manager::PasswordForm::Store::kProfileStore;

  PasswordFormList list;
  list.push_back(std::move(account_exception));
  list.push_back(std::move(profile_exception));

  delegate.SetPasswordExceptionList(list);

  base::MockCallback<PasswordsPrivateDelegate::ExceptionEntriesCallback>
      callback;
  int first_frontend_id, second_frontend_id;
  EXPECT_CALL(callback, Run(SizeIs(2)))
      .WillOnce(
          [&](const PasswordsPrivateDelegate::ExceptionEntries& exceptions) {
            first_frontend_id = exceptions[0].frontend_id;
            second_frontend_id = exceptions[1].frontend_id;
          });

  delegate.GetPasswordExceptionsList(callback.Get());

  EXPECT_EQ(first_frontend_id, second_frontend_id);
}

TEST_F(PasswordsPrivateDelegateImplTest, ChangeSavedPassword) {
  password_manager::PasswordForm sample_form = CreateSampleForm();
  SetUpPasswordStore({sample_form});

  PasswordsPrivateDelegateImpl delegate(&profile_);
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  // Double check that the contents of the passwords list matches our
  // expectation.
  bool got_passwords = false;
  delegate.GetSavedPasswordsList(base::BindLambdaForTesting(
      [&](const PasswordsPrivateDelegate::UiEntries& password_list) {
        got_passwords = true;
        ASSERT_EQ(1u, password_list.size());
        EXPECT_EQ(sample_form.username_value,
                  base::UTF8ToUTF16(password_list[0].username));
      }));
  EXPECT_TRUE(got_passwords);
  int sample_form_id = delegate.GetPasswordIdGeneratorForTesting().GenerateId(
      password_manager::CreateSortKey(sample_form));

  EXPECT_TRUE(
      delegate.ChangeSavedPassword({sample_form_id}, u"new_user", u"new_pass"));

  // Spin the loop to allow PasswordStore tasks posted when changing the
  // password to be completed.
  base::RunLoop().RunUntilIdle();

  // Check that the changing the password got reflected in the passwords list.
  got_passwords = false;
  delegate.GetSavedPasswordsList(base::BindLambdaForTesting(
      [&](const PasswordsPrivateDelegate::UiEntries& password_list) {
        got_passwords = true;
        ASSERT_EQ(1u, password_list.size());
        EXPECT_EQ("new_user", password_list[0].username);
      }));
  EXPECT_TRUE(got_passwords);
}

// Checking callback result of RequestPlaintextPassword with reason Copy.
// By implementation for Copy, callback will receive empty string.
TEST_F(PasswordsPrivateDelegateImplTest, TestCopyPasswordCallbackResult) {
  password_manager::PasswordForm form = CreateSampleForm();
  SetUpPasswordStore({form});

  PasswordsPrivateDelegateImpl delegate(&profile_);
  base::RunLoop().RunUntilIdle();

  MockReauthCallback callback;
  delegate.set_os_reauth_call(callback.Get());

  EXPECT_CALL(callback, Run(ReauthPurpose::COPY_PASSWORD))
      .WillOnce(Return(true));

  MockPlaintextPasswordCallback password_callback;
  EXPECT_CALL(password_callback, Run(Eq(std::u16string())));
  delegate.RequestPlaintextPassword(
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
  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories;

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  auto* client =
      MockPasswordManagerClient::CreateForWebContentsAndGet(web_contents.get());
  ON_CALL(*(client->GetPasswordFeatureManager()), IsOptedInForAccountStorage)
      .WillByDefault(Return(false));

  EXPECT_CALL(*client,
              TriggerReauthForPrimaryAccount(
                  signin_metrics::ReauthAccessPoint::kPasswordSettings, _));

  PasswordsPrivateDelegateImpl delegate(&profile_);
  delegate.SetAccountStorageOptIn(true, web_contents.get());
}

TEST_F(PasswordsPrivateDelegateImplTest,
       TestShouldNotReauthForOptOutAndShouldSetPref) {
  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories;

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
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

  PasswordsPrivateDelegateImpl delegate(&profile_);
  delegate.SetAccountStorageOptIn(false, web_contents.get());
}

TEST_F(PasswordsPrivateDelegateImplTest, TestCopyPasswordCallbackResultFail) {
  SetUpPasswordStore({CreateSampleForm()});

  PasswordsPrivateDelegateImpl delegate(&profile_);
  base::RunLoop().RunUntilIdle();

  MockReauthCallback callback;
  delegate.set_os_reauth_call(callback.Get());

  EXPECT_CALL(callback, Run(ReauthPurpose::COPY_PASSWORD))
      .WillOnce(Return(false));

  base::Time before_call = test_clipboard_->GetLastModifiedTime();

  MockPlaintextPasswordCallback password_callback;
  EXPECT_CALL(password_callback, Run(Eq(base::nullopt)));
  delegate.RequestPlaintextPassword(
      0, api::passwords_private::PLAINTEXT_REASON_COPY, password_callback.Get(),
      nullptr);
  // Clipboard should not be modifiend in case Reauth failed
  std::u16string result;
  test_clipboard_->ReadText(ui::ClipboardBuffer::kCopyPaste,
                            /* data_dst = */ nullptr, &result);
  EXPECT_EQ(std::u16string(), result);
  EXPECT_EQ(before_call, test_clipboard_->GetLastModifiedTime());

  // Since Reauth had failed password was not copied and metric wasn't recorded
  histogram_tester().ExpectTotalCount(kHistogramName, 0);
}

TEST_F(PasswordsPrivateDelegateImplTest, TestPassedReauthOnView) {
  SetUpPasswordStore({CreateSampleForm()});

  PasswordsPrivateDelegateImpl delegate(&profile_);
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  MockReauthCallback callback;
  delegate.set_os_reauth_call(callback.Get());

  EXPECT_CALL(callback, Run(ReauthPurpose::VIEW_PASSWORD))
      .WillOnce(Return(true));

  MockPlaintextPasswordCallback password_callback;
  EXPECT_CALL(password_callback, Run(Eq(u"test")));
  delegate.RequestPlaintextPassword(
      0, api::passwords_private::PLAINTEXT_REASON_VIEW, password_callback.Get(),
      nullptr);

  histogram_tester().ExpectUniqueSample(
      kHistogramName, password_manager::metrics_util::ACCESS_PASSWORD_VIEWED,
      1);
}

TEST_F(PasswordsPrivateDelegateImplTest, TestFailedReauthOnView) {
  SetUpPasswordStore({CreateSampleForm()});

  PasswordsPrivateDelegateImpl delegate(&profile_);
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  MockReauthCallback callback;
  delegate.set_os_reauth_call(callback.Get());

  EXPECT_CALL(callback, Run(ReauthPurpose::VIEW_PASSWORD))
      .WillOnce(Return(false));

  MockPlaintextPasswordCallback password_callback;
  EXPECT_CALL(password_callback, Run(Eq(base::nullopt)));
  delegate.RequestPlaintextPassword(
      0, api::passwords_private::PLAINTEXT_REASON_VIEW, password_callback.Get(),
      nullptr);

  // Since Reauth had failed password was not viewed and metric wasn't recorded
  histogram_tester().ExpectTotalCount(kHistogramName, 0);
}

TEST_F(PasswordsPrivateDelegateImplTest, TestReauthOnExport) {
  SetUpPasswordStore({CreateSampleForm()});
  StrictMock<base::MockCallback<base::OnceCallback<void(const std::string&)>>>
      mock_accepted;

  PasswordsPrivateDelegateImpl delegate(&profile_);
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  MockReauthCallback callback;
  delegate.set_os_reauth_call(callback.Get());

  EXPECT_CALL(mock_accepted, Run(std::string())).Times(2);

  EXPECT_CALL(callback, Run(ReauthPurpose::EXPORT)).WillOnce(Return(true));
  delegate.ExportPasswords(mock_accepted.Get(), nullptr);

  // Export should ignore previous reauthentication results.
  EXPECT_CALL(callback, Run(ReauthPurpose::EXPORT)).WillOnce(Return(true));
  delegate.ExportPasswords(mock_accepted.Get(), nullptr);
}

TEST_F(PasswordsPrivateDelegateImplTest, TestReauthFailedOnExport) {
  SetUpPasswordStore({CreateSampleForm()});
  StrictMock<base::MockCallback<base::OnceCallback<void(const std::string&)>>>
      mock_accepted;

  PasswordsPrivateDelegateImpl delegate(&profile_);
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_accepted, Run(std::string("reauth-failed")));

  MockReauthCallback callback;
  delegate.set_os_reauth_call(callback.Get());

  EXPECT_CALL(callback, Run(ReauthPurpose::EXPORT)).WillOnce(Return(false));
  delegate.ExportPasswords(mock_accepted.Get(), nullptr);
}

// Verifies that PasswordsPrivateDelegateImpl::GetPlaintextInsecurePassword
// fails if the re-auth fails.
TEST_F(PasswordsPrivateDelegateImplTest,
       TestReauthOnGetPlaintextInsecurePasswordFails) {
  PasswordsPrivateDelegateImpl delegate(&profile_);

  MockReauthCallback reauth_callback;
  delegate.set_os_reauth_call(reauth_callback.Get());

  base::MockCallback<
      PasswordsPrivateDelegate::PlaintextInsecurePasswordCallback>
      credential_callback;

  EXPECT_CALL(reauth_callback, Run(ReauthPurpose::VIEW_PASSWORD))
      .WillOnce(Return(false));
  EXPECT_CALL(credential_callback, Run(Eq(base::nullopt)));

  delegate.GetPlaintextInsecurePassword(
      api::passwords_private::InsecureCredential(),
      api::passwords_private::PLAINTEXT_REASON_VIEW, nullptr,
      credential_callback.Get());
}

// Verifies that PasswordsPrivateDelegateImpl::GetPlaintextInsecurePassword
// succeeds if the re-auth succeeds and there is a matching compromised
// credential in the store.
TEST_F(PasswordsPrivateDelegateImplTest, TestReauthOnGetPlaintextCompPassword) {
  PasswordsPrivateDelegateImpl delegate(&profile_);

  password_manager::PasswordForm form = CreateSampleForm();
  password_manager::InsecureCredential compromised_credentials;
  compromised_credentials.signon_realm = form.signon_realm;
  compromised_credentials.username = form.username_value;
  store_->AddLogin(form);
  store_->AddInsecureCredential(compromised_credentials);
  base::RunLoop().RunUntilIdle();

  api::passwords_private::InsecureCredential credential =
      std::move(delegate.GetCompromisedCredentials().at(0));

  MockReauthCallback reauth_callback;
  delegate.set_os_reauth_call(reauth_callback.Get());

  base::MockCallback<
      PasswordsPrivateDelegate::PlaintextInsecurePasswordCallback>
      credential_callback;

  base::Optional<api::passwords_private::InsecureCredential> opt_credential;
  EXPECT_CALL(reauth_callback, Run(ReauthPurpose::VIEW_PASSWORD))
      .WillOnce(Return(true));
  EXPECT_CALL(credential_callback, Run).WillOnce(MoveArg(&opt_credential));

  delegate.GetPlaintextInsecurePassword(
      std::move(credential), api::passwords_private::PLAINTEXT_REASON_VIEW,
      nullptr, credential_callback.Get());

  ASSERT_TRUE(opt_credential.has_value());
  EXPECT_EQ(form.signon_realm, opt_credential->signon_realm);
  EXPECT_EQ(form.username_value, base::UTF8ToUTF16(opt_credential->username));
  EXPECT_EQ(form.password_value, base::UTF8ToUTF16(*opt_credential->password));
}

}  // namespace extensions
