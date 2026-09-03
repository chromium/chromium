// Copyright 2016 The Chromium Authors
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
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_impl.h"
#include "chrome/browser/password_manager/factories/account_password_store_factory.h"
#include "chrome/browser/password_manager/factories/bulk_leak_check_service_factory.h"
#include "chrome/browser/password_manager/factories/password_sender_service_factory.h"
#include "chrome/browser/password_manager/factories/profile_password_store_factory.h"
#include "chrome/browser/password_manager/password_change_service_factory.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/passwords/settings/fake_password_import_controller.h"
#include "chrome/browser/ui/passwords/settings/mock_password_import_controller.h"
#include "chrome/browser/ui/passwords/settings/password_import_controller_interface.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service_factory.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/browser/webapps/webapps_client_desktop.h"
#include "chrome/browser/webauthn/change_pin_controller.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/enclave_manager_interface.h"
#include "chrome/browser/webauthn/mock_enclave_manager.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/device_reauth/device_reauth_metrics_util.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/content/browser/password_manager_log_router_factory.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/import/import_results.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/password_string.h"
#include "components/password_manager/core/browser/sharing/mock_password_sender_service.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/test_event_router_observer.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/test/clipboard_test_util.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

using device_reauth::ReauthResult;
using password_manager::PasswordForm;
using password_manager::PasswordRecipient;
using password_manager::PasswordString;
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

using MockPlaintextPasswordCallback =
    base::MockCallback<PasswordsPrivateDelegate::PlaintextPasswordCallback>;
using MockRequestCredentialsDetailsCallback =
    base::MockCallback<PasswordsPrivateDelegate::UiEntriesCallback>;

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

class TestEnclaveManager : public MockEnclaveManager {
 public:
  TestEnclaveManager() = default;
  ~TestEnclaveManager() override = default;
  EnclaveManager* GetEnclaveManager() override { return nullptr; }
};

std::unique_ptr<KeyedService> BuildPasswordsPrivateEventRouter(
    content::BrowserContext* context) {
  return std::make_unique<PasswordsPrivateEventRouterImpl>(context);
}

PasswordForm CreateSampleForm(
    PasswordForm::Store store = PasswordForm::Store::kProfileStore,
    const std::u16string& username = u"test@gmail.com") {
  PasswordForm form;
  form.signon_realm = "https://abc1.com";
  form.url = GURL("https://abc1.com");
  form.username_value = username;
  form.password_value = PasswordString(u"test");
  form.in_store = store;
  return form;
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

class PasswordsPrivateDelegateImplTest : public InProcessBrowserTest {
 public:
  PasswordsPrivateDelegateImplTest() = default;

  PasswordsPrivateDelegateImplTest(const PasswordsPrivateDelegateImplTest&) =
      delete;
  PasswordsPrivateDelegateImplTest& operator=(
      const PasswordsPrivateDelegateImplTest&) = delete;

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override;

  void SetUpOnMainThread() override;

  // Sets up a testing password store and fills it with |forms|.
  void SetUpPasswordStores(std::vector<PasswordForm> forms);

  // Sets up a testing EventRouter with a production
  // PasswordsPrivateEventRouter.
  void SetUpRouters();

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  scoped_refptr<PasswordsPrivateDelegateImpl> CreateDelegate() {
    Profile* profile = GetProfile();
    return base::MakeRefCounted<PasswordsPrivateDelegateImpl>(
        profile->GetPrefs(), IdentityManagerFactory::GetForProfile(profile),
        profile->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess(),
        PasswordSenderServiceFactory::GetForProfile(profile),
        SyncServiceFactory::GetForProfile(profile),
        TrustSafetySentimentServiceFactory::GetForProfile(profile),
        AffiliationServiceFactory::GetForProfile(profile),
        ProfilePasswordStoreFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS),
        AccountPasswordStoreFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS),
        PasskeyModelFactory::GetInstance()->GetForProfile(profile),
        BulkLeakCheckServiceFactory::GetForProfile(profile),
        PasswordsPrivateEventRouterFactory::GetForProfile(profile),
        &web_app::WebAppProvider::GetForWebApps(profile)->install_manager(),
        EnclaveManagerFactory::GetForProfile(profile), base::NullCallback(),
        base::DoNothing());
  }

  // Queries and returns the list of saved credentials, blocking until finished.
  PasswordsPrivateDelegate::UiEntries GetCredentials(
      PasswordsPrivateDelegate& delegate);

  // Returns a test `WebContents` with an initialized Autofill client, which is
  // needed for PasswordManager client to work properly.
  std::unique_ptr<content::WebContents> CreateWebContents() {
    content::WebContents::CreateParams params(GetProfile());
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContents::Create(params);
    autofill::ChromeAutofillClient::CreateForWebContents(web_contents.get());
    return web_contents;
  }

  syncer::TestSyncService* sync_service();

 protected:
  raw_ptr<extensions::EventRouter> event_router_ = nullptr;
  scoped_refptr<TestPasswordStore> profile_store_;
  scoped_refptr<TestPasswordStore> account_store_;
  MockChangePinController change_pin_controller_;

  void TearDownOnMainThread() override;

 private:
  base::HistogramTester histogram_tester_;
};

void PasswordsPrivateDelegateImplTest::TearDownOnMainThread() {
  event_router_ = nullptr;
  ui::Clipboard::DestroyClipboardForCurrentThread();
  InProcessBrowserTest::TearDownOnMainThread();
}

void PasswordsPrivateDelegateImplTest::SetUpBrowserContextKeyedServices(
    content::BrowserContext* context) {
  InProcessBrowserTest::SetUpBrowserContextKeyedServices(context);

  PasswordStatusCheckServiceFactory::GetInstance()->SetTestingFactory(
      context,
      base::BindRepeating(
          [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return nullptr;
          }));

  password_manager::PasswordManagerLogRouterFactory::GetInstance()
      ->SetTestingFactory(
          context,
          base::BindRepeating(
              [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                return nullptr;
              }));

  ProfilePasswordStoreFactory::GetInstance()->SetTestingFactory(
      context,
      base::BindRepeating(
          &password_manager::BuildPasswordStore<
              content::BrowserContext, password_manager::TestPasswordStore>));
  AccountPasswordStoreFactory::GetInstance()->SetTestingFactory(
      context,
      base::BindRepeating(
          &password_manager::BuildPasswordStoreWithArgs<
              content::BrowserContext, password_manager::TestPasswordStore,
              password_manager::IsAccountStore>,
          password_manager::IsAccountStore(true)));

  SyncServiceFactory::GetInstance()->SetTestingFactory(
      context,
      base::BindRepeating(
          [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return std::make_unique<syncer::TestSyncService>();
          }));

  AffiliationServiceFactory::GetInstance()->SetTestingFactory(
      context,
      base::BindOnce(
          [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return std::make_unique<affiliations::FakeAffiliationService>();
          }));

  PasskeyModelFactory::GetInstance()->SetTestingFactory(
      context,
      base::BindRepeating(
          [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return std::make_unique<webauthn::TestPasskeyModel>();
          }));

  PasswordSenderServiceFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating([](content::BrowserContext*)
                                       -> std::unique_ptr<KeyedService> {
        return std::make_unique<password_manager::MockPasswordSenderService>();
      }));

  EnclaveManagerFactory::GetInstance()->SetTestingFactory(
      context,
      base::BindRepeating(
          [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return std::make_unique<TestEnclaveManager>();
          }));
}

void PasswordsPrivateDelegateImplTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
  profile_store_ = static_cast<password_manager::TestPasswordStore*>(
      ProfilePasswordStoreFactory::GetForProfile(
          GetProfile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get());
  account_store_ = static_cast<password_manager::TestPasswordStore*>(
      AccountPasswordStoreFactory::GetForProfile(
          GetProfile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get());
  ui::TestClipboard::CreateForCurrentThread();
  SetUpRouters();
  sync_service()->SetSignedIn(signin::ConsentLevel::kSignin);
  ChangePinController::set_instance_for_testing(&change_pin_controller_);
}

void PasswordsPrivateDelegateImplTest::SetUpPasswordStores(
    std::vector<PasswordForm> forms) {
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
  base::RunLoop().RunUntilIdle();
}

void PasswordsPrivateDelegateImplTest::SetUpRouters() {
  event_router_ = extensions::EventRouter::Get(GetProfile());
  // Set the production PasswordsPrivateEventRouter::Create as a testing
  // factory, because at some point during the preceding initialization, a null
  // factory is set, resulting in nul PasswordsPrivateEventRouter.
  PasswordsPrivateEventRouterFactory::GetInstance()->SetTestingFactory(
      GetProfile(), base::BindRepeating(&BuildPasswordsPrivateEventRouter));
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
      SyncServiceFactory::GetForProfile(GetProfile()));
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateDelegateImplTest,
                       ActionableErrorChanged) {
  auto delegate = CreateDelegate();

  extensions::TestEventRouterObserver observer(
      extensions::EventRouter::Get(GetProfile()));

  profile_store_->SetError(
      password_manager::ActionableError::kTrustedVaultKeyNeeded);
  profile_store_->NotifyAboutError();

  observer.WaitForEventWithName(
      api::passwords_private::OnPasswordManagerActionableErrorChanged::
          kEventName);

  auto& events = observer.events();
  auto it =
      events.find(api::passwords_private::
                      OnPasswordManagerActionableErrorChanged::kEventName);
  ASSERT_TRUE(it != events.end());
  base::Value args = base::Value(it->second->args().Clone());

  ASSERT_TRUE(args.is_list());
  ASSERT_EQ(1u, args.GetList().size());
  EXPECT_EQ("TRUSTED_VAULT_KEY_NEEDED", args.GetList()[0].GetString());
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateDelegateImplTest,
                       ImportPasswordsLogsImportResultsStatus) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  auto delegate = CreateDelegate();

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
  delegate->ImportPasswords(api::passwords_private::PasswordStoreSet::kAccount,
                            callback.Get(), web_contents.get());
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectUniqueSample("PasswordManager.ImportResultsStatus2",
                                        kExpectedStatus, 1);
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateDelegateImplTest,
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
      "PasswordManager.AccessPasswordInSettings",
      password_manager::metrics_util::ACCESS_PASSWORD_VIEWED, 1);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PasswordsPrivateDelegateImplTest,
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
  histogram_tester().ExpectTotalCount(
      "PasswordManager.AccessPasswordInSettings", 0);
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateDelegateImplTest,
                       TestReauthFailedOnExport) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  SetUpPasswordStores({CreateSampleForm()});
  StrictMock<base::MockCallback<base::OnceCallback<void(
      PasswordsPrivateDelegate::ExportPasswordsResult)>>>
      mock_accepted;

  auto delegate = CreateDelegate();
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  ExpectAuthentication(delegate, /*successful=*/false);

  EXPECT_CALL(
      mock_accepted,
      Run(PasswordsPrivateDelegate::ExportPasswordsResult::kReauthFailed));
  delegate->ExportPasswords(mock_accepted.Get(), web_contents.get());
}
#endif

// TODO(http://crbug.com/40272850) Re-enable.
IN_PROC_BROWSER_TEST_F(PasswordsPrivateDelegateImplTest,
                       ShowAddShortcutDialog) {
  base::HistogramTester histogram_tester;
  // Simulate a navigation.
  browser()->GetWindow()->Show();
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateParams nav_params(browser(),
                            embedded_test_server()->GetURL("/empty.html"),
                            ui::PAGE_TRANSITION_TYPED);
  nav_params.tabstrip_index = 0;
  nav_params.disposition = WindowOpenDisposition::CURRENT_TAB;
  Navigate(&nav_params);

  webapps::WebappsClientDesktop::CreateSingleton();
  auto* provider = web_app::WebAppProvider::GetForTest(GetProfile());
  web_app::test::WaitUntilWebAppProviderAndSubsystemsReady(provider);
  base::RunLoop().RunUntilIdle();

  // Check that no web app installation is happening at the moment.
  ASSERT_EQ(provider->command_manager().GetCommandCountForTesting(), 0u);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_FALSE(
      provider->command_manager().IsInstallingForWebContents(web_contents));

  auto delegate = CreateDelegate();
  delegate->ShowAddShortcutDialog(web_contents);
  base::RunLoop().RunUntilIdle();

  // Check that app installation was triggered.
  EXPECT_EQ(provider->command_manager().GetCommandCountForTesting(), 1u);
  EXPECT_TRUE(
      provider->command_manager().IsInstallingForWebContents(web_contents));

  histogram_tester.ExpectUniqueSample("PasswordManager.ShortcutMetric", 0, 1);
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateDelegateImplTest,
                       IsChangePinFlowAvailable) {
  auto delegate = CreateDelegate();
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;

  EXPECT_CALL(mock_callback, Run(Eq(true)));
  EXPECT_CALL(change_pin_controller_, IsChangePinFlowAvailable)
      .WillOnce([&](auto callback) { std::move(callback).Run(true); });
  delegate->IsPasswordManagerPinAvailable(web_contents.get(),
                                          mock_callback.Get());
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
class PasswordsPrivateDelegateImplMockTaskEnvironmentTest
    : public PasswordsPrivateDelegateImplTest {
 public:
  PasswordsPrivateDelegateImplMockTaskEnvironmentTest() = default;

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(PasswordsPrivateDelegateImplMockTaskEnvironmentTest,
                       AuthenticationTimeMetric) {
  content::WebContents* web_contents_ptr =
      browser()->GetTabStripModel()->GetActiveWebContents();
  auto delegate = CreateDelegate();

  auto biometric_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  EXPECT_CALL(*biometric_authenticator, AuthenticateWithMessage)
      .WillOnce(testing::WithArg<1>(
          [](PasswordsPrivateDelegateImpl::AuthResultCallback callback) {
            std::move(callback).Run(/*successful=*/true);
          }));

  delegate->SetDeviceAuthenticatorForTesting(
      std::move(biometric_authenticator));

  MockRequestCredentialsDetailsCallback callback;
  EXPECT_CALL(callback, Run(testing::IsEmpty()));
  delegate->RequestCredentialsDetails({0}, callback.Get(), web_contents_ptr);

  histogram_tester().ExpectUniqueTimeSample(
      "PasswordManager.Settings.AuthenticationTime2", base::Seconds(0), 1);
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateDelegateImplMockTaskEnvironmentTest,
                       ClosingTabDuringExportDoesNotCrashChrome) {
  chrome::AddSelectedTabWithURL(browser(), GURL(url::kAboutBlankURL),
                                ui::PAGE_TRANSITION_LINK);
  content::WebContents* web_contents_ptr =
      browser()->GetTabStripModel()->GetActiveWebContents();
  auto delegate = CreateDelegate();

  auto biometric_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  device_reauth::DeviceAuthenticator::AuthenticateCallback auth_result_callback;
  EXPECT_CALL(*biometric_authenticator, AuthenticateWithMessage)
      .WillOnce(MoveArg<1>(&auth_result_callback));

  delegate->SetDeviceAuthenticatorForTesting(
      std::move(biometric_authenticator));

  base::MockCallback<
      base::OnceCallback<void(PasswordsPrivateDelegate::ExportPasswordsResult)>>
      callback;
  delegate->ExportPasswords(callback.Get(), web_contents_ptr);

  // Simulate closing tab while authentication is still ongoing.
  browser()->GetTabStripModel()->CloseWebContentsAt(
      browser()->GetTabStripModel()->active_index(), CLOSE_NONE);

  // Now simulate auth is finished with success. Expect export to fail because
  // the tab is closed.
  EXPECT_CALL(
      callback,
      Run(PasswordsPrivateDelegate::ExportPasswordsResult::kReauthFailed));
  std::move(auth_result_callback).Run(true);
}

#if !BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(PasswordsPrivateDelegateImplMockTaskEnvironmentTest,
                       DestroyingDelegateWhileExportOngoing) {
  chrome::AddSelectedTabWithURL(browser(), GURL(url::kAboutBlankURL),
                                ui::PAGE_TRANSITION_LINK);
  content::WebContents* web_contents_ptr =
      browser()->GetTabStripModel()->GetActiveWebContents();
  auto delegate = CreateDelegate();

  auto biometric_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  auto* biometric_authenticator_ptr = biometric_authenticator.get();

  device_reauth::DeviceAuthenticator::AuthenticateCallback auth_result_callback;

  delegate->SetDeviceAuthenticatorForTesting(
      std::move(biometric_authenticator));

  EXPECT_CALL(*biometric_authenticator_ptr, AuthenticateWithMessage);
  base::MockCallback<
      base::OnceCallback<void(PasswordsPrivateDelegate::ExportPasswordsResult)>>
      callback;
  delegate->ExportPasswords(callback.Get(), web_contents_ptr);

  // Simulate destroying delegate while authentication is still ongoing. It
  // should trigger cancelation of ongoing authentication.
  EXPECT_CALL(*biometric_authenticator_ptr, Cancel);
  delegate.reset();
}
#endif  // !BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)

}  // namespace extensions
