// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/run_until.h"
#include "chrome/browser/autofill/automated_tests/cache_replayer.h"
#include "chrome/browser/autofill/captured_sites_test_utils.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/chrome_password_change_service.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/password_manager/password_change_service_factory.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/password_manager/password_manager_uitest_util.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_switches.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/fake_password_store_backend.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/native_theme/mock_os_settings_provider.h"

using captured_sites_test_utils::CapturedSiteParams;
using captured_sites_test_utils::GetCapturedSites;
using captured_sites_test_utils::GetParamAsString;

namespace {

const char kPasswordChangeScenario[] = "automated_password_change";

// Return path to the Password Manager captured sites test root directory. The
// directory contains subdirectories for different password manager test
// scenarios. The test scenario subdirectories contain site capture files
// and test recipe replay files.
base::FilePath GetReplayFilesRootDirectory() {
  base::FilePath src_dir;
  if (base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir)) {
    return src_dir.AppendASCII("chrome")
        .AppendASCII("test")
        .AppendASCII("data")
        .AppendASCII("password")
        .AppendASCII("captured_sites")
        .AppendASCII("artifacts");
  }

  ADD_FAILURE() << "Unable to obtain the Chromium src directory!";
  src_dir.clear();
  return src_dir;
}

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

password_manager::PasswordForm CreatePasswordForm(
    const GURL& url,
    const std::u16string& username,
    const std::u16string& password) {
  password_manager::PasswordForm form;
  form.url = GURL(url);
  form.signon_realm = url.GetWithEmptyPath().spec();
  form.username_value = username;
  form.password_value = password;
  return form;
}

std::vector<CapturedSiteParams> GetPasswordManagerTests() {
  std::vector<CapturedSiteParams> tests =
      GetCapturedSites(GetReplayFilesRootDirectory());
  std::erase_if(tests, [](CapturedSiteParams params) {
    return params.scenario_dir == kPasswordChangeScenario;
  });
  return tests;
}

std::vector<CapturedSiteParams> GetAutomatedPasswordChangeTests() {
  std::vector<CapturedSiteParams> tests =
      GetCapturedSites(GetReplayFilesRootDirectory());
  std::erase_if(tests, [](CapturedSiteParams params) {
    return params.scenario_dir != kPasswordChangeScenario;
  });
  return tests;
}

}  // namespace

using autofill::test::ServerCacheReplayer;
using autofill::test::ServerUrlLoader;

// Harness for running password manager scenarios on captured real-world sites.
// Test params:
//  - string Recipe: the name of the captured site file and the test recipe
//        file.
class CapturedSitesPasswordManagerBrowserTest
    : public InProcessBrowserTest,
      public captured_sites_test_utils::
          TestRecipeReplayChromeFeatureActionExecutor,
      public ::testing::WithParamInterface<CapturedSiteParams> {
 public:
  CapturedSitesPasswordManagerBrowserTest(
      const CapturedSitesPasswordManagerBrowserTest&) = delete;
  CapturedSitesPasswordManagerBrowserTest& operator=(
      const CapturedSitesPasswordManagerBrowserTest&) = delete;

  // TestRecipeReplayChromeFeatureActionExecutor:
  bool AddCredential(const std::string& origin,
                     const std::string& username,
                     const std::string& password) override {
    scoped_refptr<password_manager::TestPasswordStore> password_store =
        GetDefaultPasswordStore(browser()->profile());
    password_manager::PasswordForm signin_form;
    signin_form.url = GURL(origin);
    signin_form.signon_realm = origin;
    signin_form.password_value = base::ASCIIToUTF16(password);
    signin_form.username_value = base::ASCIIToUTF16(username);
    password_store->AddLogin(signin_form);
    return true;
  }

  bool AddAutofillProfileInfo(const std::string& field_type,
                              const std::string& field_value) override {
    return profile_controller_->AddAutofillProfileInfo(field_type, field_value);
  }

  bool SavePassword() override {
    BubbleObserver bubble_observer(WebContents());
    if (bubble_observer.IsSavePromptAvailable()) {
      bubble_observer.AcceptSavePrompt();
      PasswordManagerBrowserTestBase::WaitForPasswordStore(browser());
      // Hide the Save Password Prompt UI.
      TabDialogs::FromWebContents(WebContents())->HideManagePasswordsBubble();
      content::RunAllPendingInMessageLoop();
      return true;
    }
    ADD_FAILURE() << "No Save Password prompt!";
    return false;
  }

  bool UpdatePassword() override {
    BubbleObserver bubble_observer(WebContents());
    if (bubble_observer.IsUpdatePromptAvailable()) {
      bubble_observer.AcceptUpdatePrompt();
      PasswordManagerBrowserTestBase::WaitForPasswordStore(browser());
      // Hide the Update Password Prompt UI.
      TabDialogs::FromWebContents(WebContents())->HideManagePasswordsBubble();
      content::RunAllPendingInMessageLoop();
      return true;
    }
    ADD_FAILURE() << "No Update Password prompt!";
    return false;
  }

  bool WaitForSaveFallback() override {
    BubbleObserver bubble_observer(WebContents());
    if (bubble_observer.WaitForFallbackForSaving()) {
      return true;
    }
    ADD_FAILURE() << "Chrome did not show the save fallback icon!";
    return false;
  }

  bool IsChromeShowingPasswordGenerationPrompt() override {
    return observer_.popup_showing() &&
           observer_.state() ==
               PasswordGenerationPopupController::kOfferGeneration;
  }

  bool HasChromeShownSavePasswordPrompt() override {
    BubbleObserver bubble_observer(WebContents());
    return bubble_observer.IsSavePromptShownAutomatically();
  }

  bool HasChromeStoredCredential(const std::string& origin,
                                 const std::string& username,
                                 const std::string& password) override {
    scoped_refptr<password_manager::TestPasswordStore> password_store =
        GetDefaultPasswordStore(browser()->profile());
    password_manager::FakePasswordStoreBackend* fake_backend =
        static_cast<password_manager::FakePasswordStoreBackend*>(
            password_store->GetBackendForTesting());

    auto found = fake_backend->stored_passwords().find(origin);
    if (fake_backend->stored_passwords().end() == found) {
      return false;
    }

    const std::vector<password_manager::PasswordForm>& passwords_vector =
        found->second;
    for (const auto& found_password : passwords_vector) {
      if (base::ASCIIToUTF16(username) == found_password.username_value &&
          base::ASCIIToUTF16(password) == found_password.password_value) {
        return true;
      }
    }

    return false;
  }

 protected:
  CapturedSitesPasswordManagerBrowserTest() = default;
  ~CapturedSitesPasswordManagerBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  // Set up a TestSyncService which will happily return
                  // "everything is active" so that password generation is
                  // considered enabled.
                  SyncServiceFactory::GetInstance()->SetTestingFactory(
                      context, base::BindRepeating(&BuildTestSyncService));

                  ProfilePasswordStoreFactory::GetInstance()->SetTestingFactory(
                      context,
                      base::BindRepeating(
                          &password_manager::BuildPasswordStoreWithFakeBackend<
                              content::BrowserContext>));

                  AccountPasswordStoreFactory::GetInstance()->SetTestingFactory(
                      context,
                      base::BindRepeating(
                          &password_manager::BuildPasswordStoreWithFakeBackend<
                              content::BrowserContext>));
                }));

    profile_controller_ =
        std::make_unique<captured_sites_test_utils::ProfileDataController>();
  }

  void SetUpOnMainThread() override {
    recipe_replayer_ =
        std::make_unique<captured_sites_test_utils::TestRecipeReplayer>(
            browser(), this);
    SetServerUrlLoader(
        std::make_unique<ServerUrlLoader>(std::make_unique<ServerCacheReplayer>(
            GetParam().capture_file_path,
            ServerCacheReplayer::kOptionFailOnInvalidJsonRecord |
                ServerCacheReplayer::kOptionSplitRequestsByForm)));

    ChromePasswordManagerClient* client =
        ChromePasswordManagerClient::FromWebContents(WebContents());
    client->SetTestObserver(&observer_);

    browser()->profile()->GetPrefs()->SetBoolean(::prefs::kSafeBrowsingEnabled,
                                                 false);

    // Disable the caret blinking to not generate any compositor frames from
    // just a blinking cursor.
    os_settings_provider_.SetCaretBlinkInterval(base::TimeDelta());
  }

  virtual void SetUpHostResolverRules(base::CommandLine* command_line) {
    captured_sites_test_utils::TestRecipeReplayer::SetUpHostResolverRules(
        command_line);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{autofill::features::debug::kAutofillServerCommunication, {}},
         {optimization_guide::features::
              kOptimizationGuideBypassPasswordChangeAuth,
          {}},
         {autofill::features::debug::kAutofillShowTypePredictions, {}}},
        {});
    command_line->AppendSwitch(autofill::switches::kShowAutofillSignatures);
    SetUpHostResolverRules(command_line);
    captured_sites_test_utils::TestRecipeReplayer::SetUpCommandLine(
        command_line);
  }

  void SetServerUrlLoader(std::unique_ptr<ServerUrlLoader> server_url_loader) {
    server_url_loader_ = std::move(server_url_loader);
  }

  void TearDownOnMainThread() override {
    recipe_replayer_.reset();
    // Need to delete the URL loader and its underlying interceptor on the main
    // thread. Will result in a fatal crash otherwise. The pointer  has its
    // memory cleaned up twice: first time in that single thread, a second time
    // when the fixture's destructor is called, which will have no effect since
    // the raw pointer will be nullptr.
    server_url_loader_.reset();
  }

  captured_sites_test_utils::TestRecipeReplayer* recipe_replayer() {
    return recipe_replayer_.get();
  }

  content::WebContents* WebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  TestGenerationPopupObserver observer_;
  std::unique_ptr<captured_sites_test_utils::TestRecipeReplayer>
      recipe_replayer_;
  std::unique_ptr<captured_sites_test_utils::ProfileDataController>
      profile_controller_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ServerUrlLoader> server_url_loader_;

  ui::MockOsSettingsProvider os_settings_provider_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_P(CapturedSitesPasswordManagerBrowserTest, Recipe) {
  captured_sites_test_utils::PrintInstructions(
      "password_manager_captured_sites_interactive_uitest");

  base::FilePath src_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir));

  bool test_completed = recipe_replayer()->ReplayTest(
      GetParam().capture_file_path, GetParam().recipe_file_path,
      captured_sites_test_utils::GetCommandFilePath());
  if (!test_completed) {
    ADD_FAILURE() << "Full execution was unable to complete.";
  }
}

// This test is called with a dynamic list and may be empty during the Autofill
// run instance, so adding GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST a la
// crbug/1192206
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    CapturedSitesPasswordManagerBrowserTest);
INSTANTIATE_TEST_SUITE_P(All,
                         CapturedSitesPasswordManagerBrowserTest,
                         testing::ValuesIn(GetPasswordManagerTests()),
                         GetParamAsString());

// Executes tests related to automated password change feature. Unlike other
// password manager tests, it allows invoking a live version of Model Execution
// Service.
class CapturedSitesAutomatedPasswordChangeBrowserTest
    : public CapturedSitesPasswordManagerBrowserTest {
 protected:
  bool TriggerPasswordChange(const GURL& url) override {
    // Override change-password-url command line parameter.
    command_line_->AppendSwitchUTF8(password_manager::kPasswordChangeUrl,
                                    url.spec());

    auto* password_change_service =
        PasswordChangeServiceFactory::GetForProfile(browser()->profile());
    password_change_service->OfferPasswordChangeUi(
        CreatePasswordForm(WebContents()->GetLastCommittedURL(), u"test",
                           u"pa$$word"),
        WebContents());
    PasswordChangeDelegate* delegate =
        password_change_service->GetPasswordChangeDelegate(WebContents());

    delegate->StartPasswordChangeFlow();
    return true;
  }

  bool WaitForPasswordChangeState(int state) override {
    ChromePasswordChangeService* password_change_service =
        PasswordChangeServiceFactory::GetForProfile(browser()->profile());
    PasswordChangeDelegate* delegate =
        password_change_service->GetPasswordChangeDelegate(WebContents());

    std::vector<PasswordChangeDelegate::State> terminal_states = {
        PasswordChangeDelegate::State::kPasswordSuccessfullyChanged,
        PasswordChangeDelegate::State::kPasswordChangeFailed,
        PasswordChangeDelegate::State::kChangePasswordFormNotFound,
        PasswordChangeDelegate::State::kOtpDetected,
        PasswordChangeDelegate::State::kLoginFormDetected,
    };
    PasswordChangeDelegate::State expected_state =
        static_cast<PasswordChangeDelegate::State>(state);

    EXPECT_TRUE(std::ranges::contains(terminal_states, expected_state))
        << "Unexpected password change state. It's possible to wait only for "
           "a terminal state.";

    return base::test::RunUntil([&]() {
      if (std::ranges::contains(terminal_states, delegate->GetCurrentState())) {
        EXPECT_EQ(expected_state, delegate->GetCurrentState())
            << "Password change reached an unexpected terminal state";
        return true;
      }
      return false;
    });
  }

  void SetUpInProcessBrowserTestFixture() override {
    host_resolver()->AllowDirectLookup(
        "chromemodelexecution-pa.googleapis.com");
    CapturedSitesPasswordManagerBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    CapturedSitesPasswordManagerBrowserTest::SetUpOnMainThread();
    web_page_replay_server_wrapper_ =
        std::make_unique<captured_sites_test_utils::WebPageReplayServerWrapper>(
            false, 8082, 8083);
  }

  void SetUpHostResolverRules(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        network::switches::kHostResolverRules,
        base::StringPrintf(
            "MAP *:80 127.0.0.1:%d,"
            "MAP *.googleapis.com:443 127.0.0.1:%d,"
            "MAP *:443 127.0.0.1:%d,"
            "EXCLUDE chromemodelexecution-pa.googleapis.com,"
            "EXCLUDE localhost",
            captured_sites_test_utils::TestRecipeReplayer::kHostHttpPort,
            captured_sites_test_utils::TestRecipeReplayer::kHostHttpsRecordPort,
            captured_sites_test_utils::TestRecipeReplayer::kHostHttpsPort));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line_ = command_line;
    command_line->AppendSwitchASCII(
        autofill::test::kAutofillServerBehaviorParam, "ProductionServer");
    CapturedSitesPasswordManagerBrowserTest::SetUpCommandLine(command_line);
  }

  void TearDownOnMainThread() override {
    command_line_ = nullptr;
    CapturedSitesPasswordManagerBrowserTest::TearDownOnMainThread();
    EXPECT_TRUE(web_page_replay_server_wrapper_->Stop())
        << "Cannot stop the local Web Page Replay server.";
  }

 private:
  raw_ptr<base::CommandLine> command_line_;
  std::unique_ptr<captured_sites_test_utils::WebPageReplayServerWrapper>
      web_page_replay_server_wrapper_;
};

IN_PROC_BROWSER_TEST_P(CapturedSitesAutomatedPasswordChangeBrowserTest,
                       Recipe) {
  captured_sites_test_utils::PrintInstructions(
      "password_manager_captured_sites_interactive_uitest");

  base::FilePath src_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir));

  bool test_completed = recipe_replayer()->ReplayTest(
      GetParam().capture_file_path, GetParam().recipe_file_path,
      captured_sites_test_utils::GetCommandFilePath());
  if (!test_completed) {
    ADD_FAILURE() << "Full execution was unable to complete.";
  }
}

// This test is instantiated with a dynamic list and will be empty during the
// Password run instance, so adding
// GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST a la crbug.com/1192206
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    CapturedSitesAutomatedPasswordChangeBrowserTest);
INSTANTIATE_TEST_SUITE_P(All,
                         CapturedSitesAutomatedPasswordChangeBrowserTest,
                         testing::ValuesIn(GetAutomatedPasswordChangeTests()),
                         GetParamAsString());
