// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/passwords_navigation_observer.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/password_store_results_observer.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/fake_server_nigori_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::ElementsAre;
using testing::IsEmpty;
using testing::NiceMock;
using testing::UnorderedElementsAre;

MATCHER_P2(MatchesLogin, username, password, "") {
  return arg->username_value == base::UTF8ToUTF16(username) &&
         arg->password_value == base::UTF8ToUTF16(password);
}

MATCHER_P3(MatchesLoginAndRealm, username, password, signon_realm, "") {
  return arg->username_value == base::UTF8ToUTF16(username) &&
         arg->password_value == base::UTF8ToUTF16(password) &&
         arg->signon_realm == signon_realm;
}

const char kTestUserEmail[] = "user@email.com";
const char kExampleHostname[] = "www.example.com";
const char kExamplePslHostname[] = "psl.example.com";

// Waits for SavedPasswordsPresenter to have a certain number of
// credentials.
class SavedPasswordsPresenterWaiter
    : public StatusChangeChecker,
      public password_manager::SavedPasswordsPresenter::Observer {
 public:
  SavedPasswordsPresenterWaiter(
      password_manager::SavedPasswordsPresenter* presenter,
      size_t n_passwords)
      : presenter_(presenter), n_passwords_(n_passwords) {
    presenter_->AddObserver(this);
  }

  ~SavedPasswordsPresenterWaiter() override {
    presenter_->RemoveObserver(this);
  }

 private:
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for the SavedPasswordsPresenter to have " << n_passwords_
        << " passwords";
    return presenter_->GetSavedCredentials().size() == n_passwords_;
  }

  void OnSavedPasswordsChanged(
      const password_manager::PasswordStoreChangeList& changes) override {
    CheckExitCondition();
  }

  const raw_ptr<password_manager::SavedPasswordsPresenter> presenter_;
  const size_t n_passwords_;
};

class SyncActiveWithoutPasswordsChecker
    : public SingleClientStatusChangeChecker {
 public:
  explicit SyncActiveWithoutPasswordsChecker(syncer::SyncServiceImpl* service)
      : SingleClientStatusChangeChecker(service) {}

  ~SyncActiveWithoutPasswordsChecker() override = default;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    // DEVICE_INFO is another arbitrary type supported for signed-in users,
    // just so we rule out no type at all is active.
    return !service()->GetActiveDataTypes().Has(syncer::PASSWORDS) &&
           service()->GetActiveDataTypes().Has(syncer::DEVICE_INFO);
  }
};

// Note: This helper applies to ChromeOS too, but is currently unused there. So
// define it out to prevent a compile error due to the unused function.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
content::WebContents* GetNewTab(Browser* browser) {
  return PasswordManagerBrowserTestBase::GetNewTab(browser);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// This test fixture is similar to SingleClientPasswordsSyncTest, but it also
// sets up all the necessary test hooks etc for PasswordManager code (like
// PasswordManagerBrowserTestBase), to allow for integration tests covering
// both Sync and the PasswordManager.
class PasswordManagerSyncTest : public SyncTest {
 public:
  PasswordManagerSyncTest() : SyncTest(SINGLE_CLIENT) {
    // Note: Enabling kFillOnAccountSelect effectively *disables* autofilling on
    // page load. This is important because if a password is autofilled, then
    // all Javascript changes to it are discarded, and thus any tests that cover
    // updating a password become flaky.
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{password_manager::features::kFillOnAccountSelect,
                              switches::kExplicitBrowserSigninUIOnDesktop},
        /*disabled_features=*/{});
  }

  ~PasswordManagerSyncTest() override = default;

  void SetUp() override {
    // Setup HTTPS server serving files from standard test directory.
    // This needs to happen here (really early) because the test server must
    // already be running in SetUpCommandLine();
    static constexpr base::FilePath::CharType kDocRoot[] =
        FILE_PATH_LITERAL("chrome/test/data");
    https_test_server()->ServeFilesFromSourceDirectory(
        base::FilePath(kDocRoot));
    ASSERT_TRUE(https_test_server()->Start());

    SyncTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SyncTest::SetUpCommandLine(command_line);

    // Make sure that fake Gaia pages served by the test server match the Gaia
    // URL (as specified by GaiaUrls::gaia_url()). Note that even though the
    // hostname is the same, it's necessary to override the port to the one used
    // by the test server.
    command_line->AppendSwitchASCII(
        switches::kGaiaUrl,
        https_test_server()->GetURL("accounts.google.com", "/").spec());

    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    SyncTest::SetUpInProcessBrowserTestFixture();

    test_signin_client_subscription_ =
        secondary_account_helper::SetUpSigninClient(&test_url_loader_factory_);
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    host_resolver()->AddRule("*", "127.0.0.1");

    // Whitelist all certs for the HTTPS server.
    scoped_refptr<net::X509Certificate> cert =
        https_test_server()->GetCertificate();
    net::CertVerifyResult verify_result;
    verify_result.cert_status = 0;
    verify_result.verified_cert = cert;
    mock_cert_verifier_.mock_cert_verifier()->AddResultForCert(
        cert.get(), verify_result, net::OK);

    fake_server::SetKeystoreNigoriInFakeServer(GetFakeServer());
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(https_test_server()->ShutdownAndWaitUntilComplete());
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    SyncTest::TearDownOnMainThread();
  }

  // Also stores the AccountInfo for the signed-in account in
  // |signed_in_account_| as a side effect.
  // Implicit browser signin, disables passwords account storage by default.
  void SignIn(const std::string& email = kTestUserEmail,
              bool explicit_signin = true) {
    ASSERT_TRUE(signed_in_account_.IsEmpty());
    // Setup Sync for an unconsented account (i.e. in transport mode).
    signed_in_account_ =
        explicit_signin
            ? secondary_account_helper::SignInUnconsentedAccount(
                  GetProfile(0), &test_url_loader_factory_, email)
            : secondary_account_helper::ImplicitSignInUnconsentedAccount(
                  GetProfile(0), &test_url_loader_factory_, email);
    ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
    ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  }

  void SetupSyncTransportWithPasswordAccountStorage(
      bool explicit_signin = true) {
    SignIn(kTestUserEmail, explicit_signin);

    if (!explicit_signin) {
      // Let the user opt in to the account-scoped password storage, and wait
      // for it to become active.
      password_manager::features_util::OptInToAccountStorage(
          GetProfile(0)->GetPrefs(), GetSyncService(0));
    }
    PasswordSyncActiveChecker(GetSyncService(0)).Wait();
    ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  }

  // Should only be called after SetupSyncTransportWithPasswordAccountStorage().
  void SignOut() {
    ASSERT_FALSE(signed_in_account_.IsEmpty());
    secondary_account_helper::SignOut(GetProfile(0), &test_url_loader_factory_);
    signed_in_account_ = AccountInfo();
  }

  GURL GetWWWOrigin() {
    return embedded_test_server()->GetURL(kExampleHostname, "/");
  }
  GURL GetPSLOrigin() {
    return embedded_test_server()->GetURL(kExamplePslHostname, "/");
  }
  GURL GetHttpsOrigin(const std::string& hostname) {
    return https_test_server()->GetURL(hostname, "/");
  }

  // Returns a credential for the origin returned by GetWWWOrigin().
  password_manager::PasswordForm CreateTestPasswordForm(
      const std::string& username,
      const std::string& password) {
    return CreateTestPasswordForm(username, password, GetWWWOrigin());
  }

  password_manager::PasswordForm CreateTestPasswordForm(
      const std::string& username,
      const std::string& password,
      const GURL& origin) {
    password_manager::PasswordForm form;
    form.signon_realm = origin.spec();
    form.url = origin;
    form.username_value = base::UTF8ToUTF16(username);
    form.password_value = base::UTF8ToUTF16(password);
    form.date_created = base::Time::Now();
    return form;
  }

  // Returns a credential for the origin returned by GetPSLOrigin().
  password_manager::PasswordForm CreateTestPSLPasswordForm(
      const std::string& username,
      const std::string& password) {
    return CreateTestPasswordForm(username, password, GetPSLOrigin());
  }

  // Adds a credential to the Sync fake server for the origin returned by
  // GetWWWOrigin().
  void AddCredentialToFakeServer(const std::string& username,
                                 const std::string& password) {
    AddCredentialToFakeServer(CreateTestPasswordForm(username, password));
  }

  // Adds a credential to the Sync fake server.
  void AddCredentialToFakeServer(const password_manager::PasswordForm& form) {
    passwords_helper::InjectKeystoreEncryptedServerPassword(form,
                                                            GetFakeServer());
  }

  // Adds a credential to the local store for the origin returned by
  // GetWWWOrigin().
  void AddLocalCredential(const std::string& username,
                          const std::string& password) {
    AddLocalCredential(CreateTestPasswordForm(username, password));
  }

  // Adds a credential to the local store.
  void AddLocalCredential(const password_manager::PasswordForm& form) {
    scoped_refptr<password_manager::PasswordStoreInterface> password_store =
        passwords_helper::GetProfilePasswordStoreInterface(0);
    password_store->AddLogin(form);
    // Do a roundtrip to the DB thread, to make sure the new password is stored
    // before doing anything else that might depend on it.
    GetAllLoginsFromProfilePasswordStore();
  }

  // Synchronously reads all credentials from the profile password store and
  // returns them.
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
  GetAllLoginsFromProfilePasswordStore() {
    scoped_refptr<password_manager::PasswordStoreInterface> password_store =
        passwords_helper::GetProfilePasswordStoreInterface(0);
    password_manager::PasswordStoreResultsObserver syncer;
    password_store->GetAllLoginsWithAffiliationAndBrandingInformation(
        syncer.GetWeakPtr());
    return syncer.WaitForResults();
  }

  // Synchronously reads all credentials from the account password store and
  // returns them.
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
  GetAllLoginsFromAccountPasswordStore() {
    scoped_refptr<password_manager::PasswordStoreInterface> password_store =
        passwords_helper::GetAccountPasswordStoreInterface(0);
    password_manager::PasswordStoreResultsObserver syncer;
    password_store->GetAllLoginsWithAffiliationAndBrandingInformation(
        syncer.GetWeakPtr());
    return syncer.WaitForResults();
  }

  void NavigateToFile(content::WebContents* web_contents,
                      const std::string& hostname,
                      const std::string& path) {
    NavigateToFileImpl(web_contents,
                       embedded_test_server()->GetURL(hostname, path));
  }

  void NavigateToFileHttps(content::WebContents* web_contents,
                           const std::string& hostname,
                           const std::string& path) {
    NavigateToFileImpl(web_contents,
                       https_test_server()->GetURL(hostname, path));
  }

  void FillAndSubmitPasswordForm(content::WebContents* web_contents,
                                 const std::string& username,
                                 const std::string& password) {
    PasswordsNavigationObserver observer(web_contents);
    std::string fill_and_submit = base::StringPrintf(
        "document.getElementById('username_field').value = '%s';"
        "document.getElementById('password_field').value = '%s';"
        "document.getElementById('input_submit_button').click()",
        username.c_str(), password.c_str());
    ASSERT_TRUE(content::ExecJs(web_contents, fill_and_submit));
    ASSERT_TRUE(observer.Wait());
  }

  net::EmbeddedTestServer* https_test_server() { return &https_test_server_; }

 private:
  void NavigateToFileImpl(content::WebContents* web_contents, const GURL& url) {
    ASSERT_EQ(web_contents,
              GetBrowser(0)->tab_strip_model()->GetActiveWebContents());
    PasswordsNavigationObserver observer(web_contents);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowser(0), url));
    ASSERT_TRUE(observer.Wait());
    // After navigation, the password manager retrieves any matching credentials
    // from the store(s). So before doing anything else (like filling and
    // submitting a form), do a roundtrip to the stores to make sure the
    // credentials have finished loading.
    GetAllLoginsFromProfilePasswordStore();
    GetAllLoginsFromAccountPasswordStore();
  }

  base::test::ScopedFeatureList feature_list_;

  base::CallbackListSubscription test_signin_client_subscription_;

  // A test server instance that runs on HTTPS (as opposed to the default
  // |embedded_test_server()|). This is necessary to simulate Gaia pages, which
  // must be on a secure (cryptographic) scheme.
  net::EmbeddedTestServer https_test_server_{
      net::EmbeddedTestServer::TYPE_HTTPS};
  content::ContentMockCertVerifier mock_cert_verifier_;

  AccountInfo signed_in_account_;
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class PasswordManagerSyncExplicitParamTest
    : public PasswordManagerSyncTest,
      public base::test::WithFeatureOverride {
 public:
  PasswordManagerSyncExplicitParamTest()
      : base::test::WithFeatureOverride(
            switches::kExplicitBrowserSigninUIOnDesktop) {}
};
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest, ChooseDestinationStore) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  content::WebContents* web_contents = GetNewTab(GetBrowser(0));

  SetupSyncTransportWithPasswordAccountStorage();

  // Part 1: Save a password; it should go into the account store by default.
  {
    // Navigate to a page with a password form, fill it out, and submit it.
    NavigateToFile(web_contents, kExampleHostname,
                   "/password/password_form.html");
    FillAndSubmitPasswordForm(web_contents, "accountuser", "accountpass");

    // Save the password and check the store.
    BubbleObserver bubble_observer(web_contents);
    ASSERT_TRUE(bubble_observer.IsSavePromptShownAutomatically());
    bubble_observer.AcceptSavePrompt();

    std::vector<std::unique_ptr<password_manager::PasswordForm>>
        account_credentials = GetAllLoginsFromAccountPasswordStore();
    EXPECT_THAT(account_credentials,
                ElementsAre(MatchesLogin("accountuser", "accountpass")));
  }

  // Part 2: Mimic the user choosing to save locally; now a newly saved password
  // should end up in the profile store.
  password_manager::features_util::SetDefaultPasswordStore(
      GetProfile(0)->GetPrefs(), GetSyncService(0),
      password_manager::PasswordForm::Store::kProfileStore);
  {
    // Navigate to a page with a password form, fill it out, and submit it.
    // TODO(crbug.com/40121096): If we use the same URL as in part 1 here, then
    // the test fails because the *account* data gets filled and submitted
    // again. This is because the password manager is "smart" and prefers
    // user-typed values (including autofilled-on-pageload ones) over
    // script-provided values, see
    // https://cs.chromium.org/chromium/src/components/autofill/content/renderer/form_autofill_util.cc?rcl=e38f0c99fe45ef81bd09d97f235c3dee64e2bd9f&l=1749
    // and
    // https://cs.chromium.org/chromium/src/components/autofill/content/renderer/password_autofill_agent.cc?rcl=63830d3f4b7f5fceec9609d83cf909d0cad04bb2&l=1855
    // Some PasswordManager browser tests work around this by disabling
    // autofill on pageload, see PasswordManagerBrowserTestWithAutofillDisabled.
    // NavigateToFile(web_contents, "/password/password_form.html");
    NavigateToFile(web_contents, kExampleHostname,
                   "/password/simple_password.html");
    FillAndSubmitPasswordForm(web_contents, "localuser", "localpass");

    // Save the password and check the store.
    BubbleObserver bubble_observer(web_contents);

    EXPECT_TRUE(
        bubble_observer.IsDefaultStoreChangedPromptShownAutomatically());
    bubble_observer.AcknowledgeDefaultStoreChange();

    ASSERT_TRUE(bubble_observer.IsSavePromptShownAutomatically());
    bubble_observer.AcceptSavePrompt();

    EXPECT_THAT(GetAllLoginsFromAccountPasswordStore(),
                testing::Contains(MatchesLogin("localuser", "localpass")));
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest, UpdateInProfileStore) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  AddLocalCredential("user", "localpass");

  SetupSyncTransportWithPasswordAccountStorage();

  content::WebContents* web_contents = GetNewTab(GetBrowser(0));

  // Go to a form and submit a different password.
  NavigateToFile(web_contents, kExampleHostname,
                 "/password/simple_password.html");
  FillAndSubmitPasswordForm(web_contents, "user", "newpass");

  // There should be an update bubble; accept it.
  BubbleObserver bubble_observer(web_contents);
  ASSERT_TRUE(bubble_observer.IsUpdatePromptShownAutomatically());
  bubble_observer.AcceptUpdatePrompt();

  // The updated password should be in the profile store, while the account
  // store should still be empty.
  EXPECT_THAT(GetAllLoginsFromProfilePasswordStore(),
              ElementsAre(MatchesLogin("user", "newpass")));
  EXPECT_THAT(GetAllLoginsFromAccountPasswordStore(), IsEmpty());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest, UpdateInAccountStore) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  AddCredentialToFakeServer("user", "accountpass");

  SetupSyncTransportWithPasswordAccountStorage();

  content::WebContents* web_contents = GetNewTab(GetBrowser(0));

  // Go to a form and submit a different password.
  NavigateToFile(web_contents, kExampleHostname,
                 "/password/simple_password.html");
  FillAndSubmitPasswordForm(web_contents, "user", "newpass");

  // There should be an update bubble; accept it.
  BubbleObserver bubble_observer(web_contents);
  ASSERT_TRUE(bubble_observer.IsUpdatePromptShownAutomatically());
  bubble_observer.AcceptUpdatePrompt();

  // The updated password should be in the account store, while the profile
  // store should still be empty.
  EXPECT_THAT(GetAllLoginsFromAccountPasswordStore(),
              ElementsAre(MatchesLogin("user", "newpass")));
  EXPECT_THAT(GetAllLoginsFromProfilePasswordStore(), IsEmpty());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       UpdateMatchingCredentialInBothStores) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  AddCredentialToFakeServer("user", "pass");
  AddLocalCredential("user", "pass");

  SetupSyncTransportWithPasswordAccountStorage();

  content::WebContents* web_contents = GetNewTab(GetBrowser(0));

  // Go to a form and submit a different password.
  NavigateToFile(web_contents, kExampleHostname,
                 "/password/simple_password.html");
  FillAndSubmitPasswordForm(web_contents, "user", "newpass");

  // There should be an update bubble; accept it.
  BubbleObserver bubble_observer(web_contents);
  // TODO(crbug.com/40121096): Remove this temporary logging once the test
  // flakiness is diagnosed.
  if (!bubble_observer.IsUpdatePromptShownAutomatically()) {
    LOG(ERROR) << "ManagePasswordsUIController state: "
               << ManagePasswordsUIController::FromWebContents(web_contents)
                      ->GetState();
  }
  ASSERT_TRUE(bubble_observer.IsUpdatePromptShownAutomatically());
  bubble_observer.AcceptUpdatePrompt();

  // The updated password should be in both stores.
  EXPECT_THAT(GetAllLoginsFromAccountPasswordStore(),
              ElementsAre(MatchesLogin("user", "newpass")));
  EXPECT_THAT(GetAllLoginsFromProfilePasswordStore(),
              ElementsAre(MatchesLogin("user", "newpass")));
}

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       UpdateMismatchingCredentialInBothStores) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  AddCredentialToFakeServer("user", "accountpass");
  AddLocalCredential("user", "localpass");

  SetupSyncTransportWithPasswordAccountStorage();

  content::WebContents* web_contents = GetNewTab(GetBrowser(0));

  // Go to a form and submit a different password.
  NavigateToFile(web_contents, kExampleHostname,
                 "/password/simple_password.html");
  FillAndSubmitPasswordForm(web_contents, "user", "newpass");

  // There should be an update bubble; accept it.
  BubbleObserver bubble_observer(web_contents);
  // TODO(crbug.com/40121096): Remove this temporary logging once the test
  // flakiness is diagnosed.
  if (!bubble_observer.IsUpdatePromptShownAutomatically()) {
    LOG(ERROR) << "ManagePasswordsUIController state: "
               << ManagePasswordsUIController::FromWebContents(web_contents)
                      ->GetState();
  }
  ASSERT_TRUE(bubble_observer.IsUpdatePromptShownAutomatically());
  bubble_observer.AcceptUpdatePrompt();

  // The updated password should be in both stores.
  EXPECT_THAT(GetAllLoginsFromAccountPasswordStore(),
              ElementsAre(MatchesLogin("user", "newpass")));
  EXPECT_THAT(GetAllLoginsFromProfilePasswordStore(),
              ElementsAre(MatchesLogin("user", "newpass")));
}

// Tests that if credentials for the same username, but with different passwords
// exist in the two stores, and one of them is used to successfully log in, the
// other one is silently updated to match.
IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       AutoUpdateFromAccountToProfileOnSuccessfulUse) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Add credentials for the same username, but with different passwords, to the
  // two stores.
  AddCredentialToFakeServer("user", "accountpass");
  AddLocalCredential("user", "localpass");

  SetupSyncTransportWithPasswordAccountStorage();

  // Now we have credentials for the same user, but with different passwords, in
  // the two stores.
  ASSERT_THAT(GetAllLoginsFromProfilePasswordStore(),
              ElementsAre(MatchesLogin("user", "localpass")));
  ASSERT_THAT(GetAllLoginsFromAccountPasswordStore(),
              ElementsAre(MatchesLogin("user", "accountpass")));

  content::WebContents* web_contents = GetNewTab(GetBrowser(0));

  // Go to a form and submit the version of the credentials from the profile
  // store.
  NavigateToFile(web_contents, kExampleHostname,
                 "/password/simple_password.html");
  FillAndSubmitPasswordForm(web_contents, "user", "localpass");

  // Now the credential should of course still be in the profile store...
  ASSERT_THAT(GetAllLoginsFromProfilePasswordStore(),
              ElementsAre(MatchesLogin("user", "localpass")));
  // ...but also the one in the account store should have been silently updated
  // to match.
  EXPECT_THAT(GetAllLoginsFromAccountPasswordStore(),
              ElementsAre(MatchesLogin("user", "localpass")));
}

// Tests that if credentials for the same username, but with different passwords
// exist in the two stores, and one of them is used to successfully log in, the
// other one is silently updated to match.
IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       AutoUpdateFromProfileToAccountOnSuccessfulUse) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Add credentials for the same username, but with different passwords, to the
  // two stores.
  AddCredentialToFakeServer("user", "accountpass");
  AddLocalCredential("user", "localpass");

  SetupSyncTransportWithPasswordAccountStorage();

  // Now we have credentials for the same user, but with different passwords, in
  // the two stores.
  ASSERT_THAT(GetAllLoginsFromProfilePasswordStore(),
              ElementsAre(MatchesLogin("user", "localpass")));
  ASSERT_THAT(GetAllLoginsFromAccountPasswordStore(),
              ElementsAre(MatchesLogin("user", "accountpass")));

  content::WebContents* web_contents = GetNewTab(GetBrowser(0));

  // Go to a form and submit the version of the credentials from the account
  // store.
  NavigateToFile(web_contents, kExampleHostname,
                 "/password/simple_password.html");
  FillAndSubmitPasswordForm(web_contents, "user", "accountpass");

  // Now the credential should of course still be in the account store...
  ASSERT_THAT(GetAllLoginsFromAccountPasswordStore(),
              ElementsAre(MatchesLogin("user", "accountpass")));
  // ...but also the one in the profile store should have been updated to match.
  EXPECT_THAT(GetAllLoginsFromProfilePasswordStore(),
              ElementsAre(MatchesLogin("user", "accountpass")));
}

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       AutoUpdatePSLMatchInAccountStoreOnSuccessfulUse) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Add the same credential to both stores, but the account one is a PSL match
  // (i.e. it's stored for psl.example.com instead of www.example.com).
  AddCredentialToFakeServer(CreateTestPSLPasswordForm("user", "pass"));
  AddLocalCredential(CreateTestPasswordForm("user", "pass"));

  SetupSyncTransportWithPasswordAccountStorage();

  content::WebContents* web_contents = GetNewTab(GetBrowser(0));

  // Go to a form (on www.) and submit it with the saved credentials.
  NavigateToFile(web_contents, kExampleHostname,
                 "/password/simple_password.html");
  FillAndSubmitPasswordForm(web_contents, "user", "pass");

  // Now the PSL-matched credential should have been automatically saved for
  // www. as well (in the account store).
  EXPECT_THAT(GetAllLoginsFromAccountPasswordStore(),
              UnorderedElementsAre(
                  MatchesLoginAndRealm("user", "pass", GetWWWOrigin()),
                  MatchesLoginAndRealm("user", "pass", GetPSLOrigin())));
  // In the profile store, there was already a credential for www. so nothing
  // should have changed.
  ASSERT_THAT(GetAllLoginsFromProfilePasswordStore(),
              ElementsAre(MatchesLogin("user", "pass")));
}

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       AutoUpdatePSLMatchInProfileStoreOnSuccessfulUse) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Add the same credential to both stores, but the local one is a PSL match
  // (i.e. it's stored for psl.example.com instead of www.example.com).
  AddCredentialToFakeServer(CreateTestPasswordForm("user", "pass"));
  AddLocalCredential(CreateTestPSLPasswordForm("user", "pass"));

  SetupSyncTransportWithPasswordAccountStorage();

  content::WebContents* web_contents = GetNewTab(GetBrowser(0));

  // Go to a form (on www.) and submit it with the saved credentials.
  NavigateToFile(web_contents, kExampleHostname,
                 "/password/simple_password.html");
  FillAndSubmitPasswordForm(web_contents, "user", "pass");

  // Now the PSL-matched credential should have been automatically saved for
  // www. as well (in the profile store).
  EXPECT_THAT(GetAllLoginsFromProfilePasswordStore(),
              UnorderedElementsAre(
                  MatchesLoginAndRealm("user", "pass", GetWWWOrigin()),
                  MatchesLoginAndRealm("user", "pass", GetPSLOrigin())));
  // In the account store, there was already a credential for www. so nothing
  // should have changed.
  ASSERT_THAT(GetAllLoginsFromAccountPasswordStore(),
              ElementsAre(MatchesLogin("user", "pass")));
}

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       AutoUpdatePSLMatchInBothStoresOnSuccessfulUse) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Add the same PSL-matched credential to both stores (i.e. it's stored for
  // psl.example.com instead of www.example.com).
  AddCredentialToFakeServer(CreateTestPSLPasswordForm("user", "pass"));
  AddLocalCredential(CreateTestPSLPasswordForm("user", "pass"));

  SetupSyncTransportWithPasswordAccountStorage();

  content::WebContents* web_contents = GetNewTab(GetBrowser(0));

  // Go to a form (on www.) and submit it with the saved credentials.
  NavigateToFile(web_contents, kExampleHostname,
                 "/password/simple_password.html");
  FillAndSubmitPasswordForm(web_contents, "user", "pass");

  // Now the PSL-matched credential should have been automatically saved for
  // www. as well, in both stores.
  EXPECT_THAT(GetAllLoginsFromAccountPasswordStore(),
              UnorderedElementsAre(
                  MatchesLoginAndRealm("user", "pass", GetWWWOrigin()),
                  MatchesLoginAndRealm("user", "pass", GetPSLOrigin())));
  EXPECT_THAT(GetAllLoginsFromProfilePasswordStore(),
              UnorderedElementsAre(
                  MatchesLoginAndRealm("user", "pass", GetWWWOrigin()),
                  MatchesLoginAndRealm("user", "pass", GetPSLOrigin())));
}

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       DontOfferToSavePrimaryAccountCredential) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  SetupSyncTransportWithPasswordAccountStorage();

  content::WebContents* web_contents = GetNewTab(GetBrowser(0));

  // Navigate to a Gaia signin form and submit a credential for the primary
  // account.
  NavigateToFileHttps(web_contents, "accounts.google.com",
                      "/password/simple_password.html");
  FillAndSubmitPasswordForm(web_contents, kTestUserEmail, "pass");

  // Since the submitted credential is for the primary account, Chrome should
  // not offer to save it.
  BubbleObserver bubble_observer(web_contents);
  EXPECT_FALSE(bubble_observer.IsSavePromptAvailable());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       OfferToSaveNonPrimaryAccountCredential) {
  // Disable signin interception, because it suppresses the password bubble.
  // See PasswordManagerBrowserTestWithSigninInterception for tests with
  // interception enabled.
  g_browser_process->local_state()->SetBoolean(prefs::kBrowserAddPersonEnabled,
                                               false);

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  SetupSyncTransportWithPasswordAccountStorage();

  content::WebContents* web_contents = GetNewTab(GetBrowser(0));

  // Navigate to a Gaia signin form and submit a credential for an account that
  // is *not* the primary one.
  NavigateToFileHttps(web_contents, "accounts.google.com",
                      "/password/simple_password.html");
  // The call ensures that the form wasn't submitted too quickly before the
  // password store returned something. Otherwise, the password prompt won't be
  // shown.
  GetAllLoginsFromProfilePasswordStore();
  GetAllLoginsFromAccountPasswordStore();
  FillAndSubmitPasswordForm(web_contents, "different-user@gmail.com", "pass");

  // Since the submitted credential is *not* for the primary account, Chrome
  // should offer to save it normally.
  BubbleObserver bubble_observer(web_contents);
  bubble_observer.WaitForAutomaticSavePrompt();
}

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       OfferToUpdatePrimaryAccountCredential) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // The password for the primary account is already saved.
  AddLocalCredential(CreateTestPasswordForm(
      kTestUserEmail, "pass", GetHttpsOrigin("accounts.google.com")));

  SetupSyncTransportWithPasswordAccountStorage();

  content::WebContents* web_contents = GetNewTab(GetBrowser(0));

  // Navigate to a Gaia signin form and submit a new password for the primary
  // account.
  NavigateToFileHttps(web_contents, "accounts.google.com",
                      "/password/simple_password.html");
  // The call ensures that the form wasn't submitted too quickly before the
  // password store returned something. Otherwise, the password prompt won't be
  // shown.
  GetAllLoginsFromProfilePasswordStore();
  GetAllLoginsFromAccountPasswordStore();
  FillAndSubmitPasswordForm(web_contents, kTestUserEmail, "newpass");

  // Since (an outdated version of) the credential is already saved, Chrome
  // should offer to update it, even though it otherwise does *not* offer to
  // save this credential.
  BubbleObserver bubble_observer(web_contents);
  bubble_observer.WaitForAutomaticUpdatePrompt();
}

// Signing out on Lacros is not possible,
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       SignOutWithUnsyncedPasswordsOpensBubble) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  content::WebContents* web_contents = GetNewTab(GetBrowser(0));

  SetupSyncTransportWithPasswordAccountStorage();

  // Force credentials saved to the account to be unsynced.
  GetFakeServer()->SetHttpError(net::HTTP_BAD_REQUEST);

  NavigateToFile(web_contents, kExampleHostname,
                 "/password/password_form.html");
  FillAndSubmitPasswordForm(web_contents, "accountuser", "accountpass");

  // Save the password in the account store.
  BubbleObserver bubble_observer(web_contents);
  bubble_observer.AcceptSavePrompt();
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      account_credentials = GetAllLoginsFromAccountPasswordStore();
  ASSERT_THAT(account_credentials,
              ElementsAre(MatchesLogin("accountuser", "accountpass")));

  SignOut();
  bubble_observer.WaitForSaveUnsyncedCredentialsPrompt();
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

// TODO(b/327118794): Delete this test once implicit signin no longer exists.
IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest, OptInSurvivesSignout) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  SignIn(kTestUserEmail, /*explicit_signin=*/false);
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));

  password_manager::features_util::OptInToAccountStorage(
      GetProfile(0)->GetPrefs(), GetSyncService(0));
  PasswordSyncActiveChecker(GetSyncService(0)).Wait();

  SignOut();
  PasswordSyncInactiveChecker(GetSyncService(0)).Wait();

  // The opt-in should be remembered.
  SignIn(kTestUserEmail, /*explicit_signin=*/false);
  PasswordSyncActiveChecker(GetSyncService(0)).Wait();
}

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest, OptOutSurvivesSignout) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  SignIn(kTestUserEmail);
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));

  password_manager::features_util::OptOutOfAccountStorage(
      GetProfile(0)->GetPrefs(), GetSyncService(0));
  PasswordSyncInactiveChecker(GetSyncService(0)).Wait();

  SignOut();

  // The opt-out should be remembered.
  SignIn(kTestUserEmail);
  PasswordSyncInactiveChecker(GetSyncService(0)).Wait();
}

IN_PROC_BROWSER_TEST_P(PasswordManagerSyncExplicitParamTest, Resignin) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  // Re-signin should be offered if the user is signed out now but in the past
  // some account opted in. No opt-in yet, so no re-signin.
  EXPECT_FALSE(
      password_manager::features_util::ShouldShowAccountStorageReSignin(
          GetProfile(0)->GetPrefs(), GetSyncService(0), GURL()));

  SignIn(kTestUserEmail);

  // Still no opt-in. Plus, the user is signed-in already.
  EXPECT_FALSE(
      password_manager::features_util::ShouldShowAccountStorageReSignin(
          GetProfile(0)->GetPrefs(), GetSyncService(0), GURL()));

  // If the user isn't already opted-in, do it.
  password_manager::features_util::OptInToAccountStorage(
      GetProfile(0)->GetPrefs(), GetSyncService(0));

  // Now there's an opt-in but the user is signed-in already.
  EXPECT_FALSE(
      password_manager::features_util::ShouldShowAccountStorageReSignin(
          GetProfile(0)->GetPrefs(), GetSyncService(0), GURL()));

  SignOut();

  // If kExplicitBrowserSigninUIOnDesktop is enabled, re-signin should never be
  // offered. Otherwise, the preconditions are now met and re-signin should be
  // offered for all pages, except the Gaia sign-in page where it's useless.
  // Native UI can offer re-signin too, in that case the GURL is empty.
  EXPECT_EQ(password_manager::features_util::ShouldShowAccountStorageReSignin(
                GetProfile(0)->GetPrefs(), GetSyncService(0),
                GURL("http://www.example.com")),
            !IsParamFeatureEnabled());
  EXPECT_EQ(password_manager::features_util::ShouldShowAccountStorageReSignin(
                GetProfile(0)->GetPrefs(), GetSyncService(0),
                GURL("https://www.example.com")),
            !IsParamFeatureEnabled());
  EXPECT_EQ(password_manager::features_util::ShouldShowAccountStorageReSignin(
                GetProfile(0)->GetPrefs(), GetSyncService(0), GURL()),
            !IsParamFeatureEnabled());
  EXPECT_FALSE(
      password_manager::features_util::ShouldShowAccountStorageReSignin(
          GetProfile(0)->GetPrefs(), GetSyncService(0),
          GaiaUrls::GetInstance()->gaia_url()));
  EXPECT_FALSE(
      password_manager::features_util::ShouldShowAccountStorageReSignin(
          GetProfile(0)->GetPrefs(), GetSyncService(0),
          GaiaUrls::GetInstance()->gaia_url().Resolve("path")));

  SignIn(kTestUserEmail);

  // Once the user signs in, no re-signin offered anymore.
  EXPECT_FALSE(
      password_manager::features_util::ShouldShowAccountStorageReSignin(
          GetProfile(0)->GetPrefs(), GetSyncService(0), GURL()));
}

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       KeepOptInAccountStorageSettingsOnlyForUsers) {
  ASSERT_TRUE(SetupClients());
  SignIn("first@gmail.com", /*explicit_signin=*/false);
  password_manager::features_util::OptInToAccountStorage(
      GetProfile(0)->GetPrefs(), GetSyncService(0));
  auto first_gaia_id_hash =
      signin::GaiaIdHash::FromGaiaId(GetSyncService(0)->GetAccountInfo().gaia);
  SignOut();
  SignIn("second@gmail.com", /*explicit_signin=*/false);
  password_manager::features_util::OptInToAccountStorage(
      GetProfile(0)->GetPrefs(), GetSyncService(0));
  SignOut();

  GetSyncService(0)->GetUserSettings()->KeepAccountSettingsPrefsOnlyForUsers(
      {first_gaia_id_hash});

  SignIn("first@gmail.com", /*explicit_signin=*/false);
  EXPECT_TRUE(password_manager::features_util::IsOptedInForAccountStorage(
      GetProfile(0)->GetPrefs(), GetSyncService(0)));
  SignOut();
  SignIn("second@gmail.com", /*explicit_signin=*/false);
  EXPECT_FALSE(password_manager::features_util::IsOptedInForAccountStorage(
      GetProfile(0)->GetPrefs(), GetSyncService(0)));
}

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       KeepOptOutAccountStorageSettingsOnlyForUsers) {
  ASSERT_TRUE(SetupClients());
  SignIn("first@gmail.com");
  password_manager::features_util::OptOutOfAccountStorage(
      GetProfile(0)->GetPrefs(), GetSyncService(0));
  auto first_gaia_id_hash =
      signin::GaiaIdHash::FromGaiaId(GetSyncService(0)->GetAccountInfo().gaia);
  SignOut();
  SignIn("second@gmail.com");
  password_manager::features_util::OptOutOfAccountStorage(
      GetProfile(0)->GetPrefs(), GetSyncService(0));
  SignOut();

  GetSyncService(0)->GetUserSettings()->KeepAccountSettingsPrefsOnlyForUsers(
      {first_gaia_id_hash});

  SignIn("first@gmail.com");
  EXPECT_FALSE(password_manager::features_util::IsOptedInForAccountStorage(
      GetProfile(0)->GetPrefs(), GetSyncService(0)));
  SignOut();
  SignIn("second@gmail.com");
  EXPECT_TRUE(password_manager::features_util::IsOptedInForAccountStorage(
      GetProfile(0)->GetPrefs(), GetSyncService(0)));
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PasswordManagerSyncExplicitParamTest);

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// This test verifies that if such users delete passwords along with cookies,
// the password deletions are uploaded before the server connection is cut.
IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       PasswordDeletionsPropagateToServer) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Add credential to server.
  AddCredentialToFakeServer(CreateTestPasswordForm("user", "pass"));

  // Do implicit sign-in, so the opt-in is false by default.
  SetupSyncTransportWithPasswordAccountStorage(/*explicit_signin=*/false);
  password_manager::PasswordStoreInterface* account_store =
      passwords_helper::GetAccountPasswordStoreInterface(0);

  // Make sure the password show up in the account store and on the server.
  ASSERT_EQ(passwords_helper::GetAllLogins(account_store).size(), 1u);
  ASSERT_EQ(fake_server_->GetSyncEntitiesByDataType(syncer::PASSWORDS).size(),
            1u);
  EXPECT_TRUE(password_manager::features_util::IsOptedInForAccountStorage(
      GetProfile(0)->GetPrefs(), GetSyncService(0)));

  // Clear cookies and account passwords.
  content::BrowsingDataRemover* remover =
      GetProfile(0)->GetBrowsingDataRemover();
  content::BrowsingDataRemoverCompletionObserver observer(remover);
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(),
      chrome_browsing_data_remover::DATA_TYPE_SITE_DATA |
          chrome_browsing_data_remover::DATA_TYPE_ACCOUNT_PASSWORDS,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, &observer);
  observer.BlockUntilCompletion();

  // Now passwords should be removed from the client and server.
  EXPECT_EQ(passwords_helper::GetAllLogins(account_store).size(), 0u);
  EXPECT_EQ(fake_server_->GetSyncEntitiesByDataType(syncer::PASSWORDS).size(),
            0u);

  // The user is still opted in (because they are still signed in).
  EXPECT_TRUE(password_manager::features_util::IsOptedInForAccountStorage(
      GetProfile(0)->GetPrefs(), GetSyncService(0)));

  // The preference is reset as the account is removed from Chrome.
  SignOut();
  EXPECT_FALSE(password_manager::features_util::IsOptedInForAccountStorage(
      GetProfile(0)->GetPrefs(), GetSyncService(0)));
}

// TODO(b/327118794): Delete this test once implicit signin no longer exists.
IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       PRE_ClearAccountStoreOnStartup) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Add a credential to the server.
  AddCredentialToFakeServer(
      CreateTestPasswordForm("accountuser", "accountpass"));

  SetupSyncTransportWithPasswordAccountStorage(/*explicit_signin=*/false);

  // Also add a credential to the profile store.
  AddLocalCredential("localuser", "localpass");

  ASSERT_THAT(GetAllLoginsFromProfilePasswordStore(),
              ElementsAre(MatchesLogin("localuser", "localpass")));
  ASSERT_THAT(GetAllLoginsFromAccountPasswordStore(),
              ElementsAre(MatchesLogin("accountuser", "accountpass")));
}

// TODO(b/327118794): Delete this test once implicit signin no longer exists.
IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest, ClearAccountStoreOnStartup) {
  // Before setting up the client (aka profile), manually remove the opt-in pref
  // from the profile's prefs file. This simulates the case where the user
  // revoked their opt-in, but the account store was not cleared correctly, e.g.
  // due to a poorly-timed crash.
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::FilePath profile_path = user_data_dir.Append(GetProfileBaseName(0));
  base::FilePath json_path = profile_path.Append(chrome::kPreferencesFilename);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string json;
    ASSERT_TRUE(base::ReadFileToString(json_path, &json));
    std::optional<base::Value> prefs = base::JSONReader::Read(json);
    ASSERT_TRUE(prefs.has_value());
    ASSERT_TRUE(prefs->is_dict());
    ASSERT_TRUE(prefs->GetDict().RemoveByDottedPath(
        syncer::prefs::internal::kSelectedTypesPerAccount));
    ASSERT_TRUE(base::JSONWriter::Write(*prefs, &json));
    ASSERT_TRUE(base::WriteFile(json_path, json));
  }

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  // Since we mangled the prefs file, the opt-in should be gone.
  ASSERT_FALSE(password_manager::features_util::IsOptedInForAccountStorage(
      GetProfile(0)->GetPrefs(), GetSyncService(0)));
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));

  // Since there's no opt-in, the account-scoped store should have been cleared
  // during startup, and the credential added by the PRE_ test should be gone.
  EXPECT_THAT(GetAllLoginsFromAccountPasswordStore(), IsEmpty());

  // Just as a sanity check: The credential in the profile-scoped store should
  // still be there.
  ASSERT_THAT(GetAllLoginsFromProfilePasswordStore(),
              ElementsAre(MatchesLogin("localuser", "localpass")));
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest, SyncUtilApis) {
  ASSERT_TRUE(SetupSync());

  EXPECT_TRUE(
      password_manager::sync_util::IsSyncFeatureEnabledIncludingPasswords(
          GetSyncService(0)));
  EXPECT_TRUE(
      password_manager::sync_util::IsSyncFeatureActiveIncludingPasswords(
          GetSyncService(0)));
  EXPECT_EQ(password_manager::sync_util::
                GetAccountEmailIfSyncFeatureEnabledIncludingPasswords(
                    GetSyncService(0)),
            SyncTest::kDefaultUserEmail);
  EXPECT_EQ(
      password_manager::sync_util::GetPasswordSyncState(GetSyncService(0)),
      password_manager::sync_util::SyncState::kActiveWithNormalEncryption);

  // Enter a persistent auth error state.
  GetClient(0)->EnterSyncPausedStateForPrimaryAccount();

  // Passwords are not sync-ing actively while sync is paused (any persistent
  // auth error).
  EXPECT_FALSE(
      password_manager::sync_util::IsSyncFeatureActiveIncludingPasswords(
          GetSyncService(0)));
  EXPECT_EQ(
      password_manager::sync_util::GetPasswordSyncState(GetSyncService(0)),
      password_manager::sync_util::SyncState::kNotActive);

  // In the current implementation, the APIs below treat sync as enabled/active
  // even while paused.
  EXPECT_TRUE(
      password_manager::sync_util::IsSyncFeatureEnabledIncludingPasswords(
          GetSyncService(0)));
  EXPECT_EQ(password_manager::sync_util::
                GetAccountEmailIfSyncFeatureEnabledIncludingPasswords(
                    GetSyncService(0)),
            SyncTest::kDefaultUserEmail);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
class PasswordManagerSyncTestWithPolicy : public PasswordManagerSyncTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    PasswordManagerSyncTest::SetUpInProcessBrowserTestFixture();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  NiceMock<policy::MockConfigurationPolicyProvider>* policy_provider() {
    return &policy_provider_;
  }

 private:
  NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTestWithPolicy,
                       SyncTypesListDisabled) {
  ASSERT_TRUE(SetupClients());
  SetupSyncTransportWithPasswordAccountStorage();

  // Disable passwords via the kSyncTypesListDisabled policy.
  base::Value::List disabled_types;
  disabled_types.Append("passwords");
  policy::PolicyMap policies;
  policies.Set(policy::key::kSyncTypesListDisabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(disabled_types)), nullptr);
  policy_provider()->UpdateChromePolicy(policies);

  SyncActiveWithoutPasswordsChecker(GetSyncService(0)).Wait();
}

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       WipingAccountStoreUpdatesSavedPasswordsPresenter) {
  // Set up one credential in the account store and one presenter.
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  AddCredentialToFakeServer("user", "pass");
  SetupSyncTransportWithPasswordAccountStorage();
  ASSERT_EQ(GetAllLoginsFromAccountPasswordStore().size(), 1u);
  password_manager::SavedPasswordsPresenter presenter(
      AffiliationServiceFactory::GetForProfile(GetProfile(0)),
      ProfilePasswordStoreFactory::GetForProfile(
          GetProfile(0), ServiceAccessType::EXPLICIT_ACCESS),
      AccountPasswordStoreFactory::GetForProfile(
          GetProfile(0), ServiceAccessType::EXPLICIT_ACCESS));
  presenter.Init();
  {
    SavedPasswordsPresenterWaiter waiter(&presenter, 1);
    waiter.Wait();
  }

  password_manager::features_util::OptOutOfAccountStorageAndClearSettings(
      GetProfile(0)->GetPrefs(), GetSyncService(0));

  {
    SavedPasswordsPresenterWaiter waiter(&presenter, 0);
    waiter.Wait();
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
