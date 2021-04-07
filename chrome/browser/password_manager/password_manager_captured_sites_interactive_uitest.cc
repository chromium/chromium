// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/automated_tests/cache_replayer.h"
#include "chrome/browser/autofill/captured_sites_test_utils.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

using captured_sites_test_utils::CapturedSiteParams;
using captured_sites_test_utils::GetCapturedSites;
using captured_sites_test_utils::GetParamAsString;

namespace {

constexpr base::TimeDelta kWaitForSaveFallbackInterval =
    base::TimeDelta::FromSeconds(5);

// Return path to the Password Manager captured sites test root directory. The
// directory contains subdirectories for different password manager test
// scenarios. The test scenario subdirectories contain site capture files
// and test recipe replay files.
base::FilePath GetReplayFilesRootDirectory() {
  base::FilePath src_dir;
  if (base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir)) {
    return src_dir.AppendASCII("chrome")
        .AppendASCII("test")
        .AppendASCII("data")
        .AppendASCII("password")
        .AppendASCII("captured_sites");
  }

  ADD_FAILURE() << "Unable to obtain the Chromium src directory!";
  src_dir.clear();
  return src_dir;
}

}  // namespace

namespace password_manager {

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
  // TestRecipeReplayChromeFeatureActionExecutor:
  bool AddCredential(const std::string& origin,
                     const std::string& username,
                     const std::string& password) override {
    scoped_refptr<password_manager::TestPasswordStore> password_store =
        static_cast<password_manager::TestPasswordStore*>(
            PasswordStoreFactory::GetForProfile(
                browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
                .get());
    password_manager::PasswordForm signin_form;
    signin_form.url = GURL(origin);
    signin_form.signon_realm = origin;
    signin_form.password_value = base::ASCIIToUTF16(password);
    signin_form.username_value = base::ASCIIToUTF16(username);
    password_store->AddLogin(signin_form);
    return true;
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
    if (bubble_observer.WaitForFallbackForSaving(kWaitForSaveFallbackInterval))
      return true;
    ADD_FAILURE() << "Chrome did not show the save fallback icon!";
    return false;
  }

  bool HasChromeShownSavePasswordPrompt() override {
    BubbleObserver bubble_observer(WebContents());
    return bubble_observer.IsSavePromptShownAutomatically();
  }

  bool HasChromeStoredCredential(const std::string& origin,
                                 const std::string& username,
                                 const std::string& password) override {
    scoped_refptr<password_manager::TestPasswordStore> password_store =
        static_cast<password_manager::TestPasswordStore*>(
            PasswordStoreFactory::GetForProfile(
                browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
                .get());

    auto found = password_store->stored_passwords().find(origin);
    if (password_store->stored_passwords().end() == found) {
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
                  PasswordStoreFactory::GetInstance()->SetTestingFactory(
                      context, base::BindRepeating(
                                   &password_manager::BuildPasswordStore<
                                       content::BrowserContext,
                                       password_manager::TestPasswordStore>));
                }));
  }

  void SetUpOnMainThread() override {
    PasswordManagerBrowserTestBase::GetNewTab(browser(), &web_contents_);
    recipe_replayer_ =
        std::make_unique<captured_sites_test_utils::TestRecipeReplayer>(
            browser(), this);
    recipe_replayer()->Setup();
    SetServerUrlLoader(
        std::make_unique<ServerUrlLoader>(std::make_unique<ServerCacheReplayer>(
            GetParam().capture_file_path,
            ServerCacheReplayer::kOptionFailOnInvalidJsonRecord |
                ServerCacheReplayer::kOptionSplitRequestsByForm)));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kUsernameFirstFlow}, {});
    InProcessBrowserTest::SetUpCommandLine(command_line);
    captured_sites_test_utils::TestRecipeReplayer::SetUpCommandLine(
        command_line);
  }

  void SetServerUrlLoader(std::unique_ptr<ServerUrlLoader> server_url_loader) {
    server_url_loader_ = std::move(server_url_loader);
  }

  void TearDownOnMainThread() override {
    recipe_replayer()->Cleanup();
    // Need to delete the URL loader and its underlying interceptor on the main
    // thread. Will result in a fatal crash otherwise. The pointer  has its
    // memory cleaned up twice: first time in that single thread, a second time
    // when the fixture's destructor is called, which will have no effect since
    // the raw pointer will be nullptr.
    server_url_loader_.reset(nullptr);
  }

  captured_sites_test_utils::TestRecipeReplayer* recipe_replayer() {
    return recipe_replayer_.get();
  }

  content::WebContents* WebContents() {
    // return web_contents_;
    return web_contents_;
  }

 private:
  std::unique_ptr<captured_sites_test_utils::TestRecipeReplayer>
      recipe_replayer_;
  base::test::ScopedFeatureList feature_list_;
  content::WebContents* web_contents_ = nullptr;
  std::unique_ptr<ServerUrlLoader> server_url_loader_;

  base::CallbackListSubscription create_services_subscription_;

  DISALLOW_COPY_AND_ASSIGN(CapturedSitesPasswordManagerBrowserTest);
};

IN_PROC_BROWSER_TEST_P(CapturedSitesPasswordManagerBrowserTest, Recipe) {
  captured_sites_test_utils::PrintInstructions(
      "password_manager_captured_sites_interactive_uitest");

  base::FilePath src_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));

  bool test_completed = recipe_replayer()->ReplayTest(
      GetParam().capture_file_path, GetParam().recipe_file_path,
      captured_sites_test_utils::GetCommandFilePath());
  if (!test_completed)
    ADD_FAILURE() << "Full execution was unable to complete.";
}

// This test is called with a dynamic list and may be empty during the Autofill
// run instance, so adding GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST a la
// crbug/1192206
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    CapturedSitesPasswordManagerBrowserTest);
INSTANTIATE_TEST_SUITE_P(
    All,
    CapturedSitesPasswordManagerBrowserTest,
    testing::ValuesIn(GetCapturedSites(GetReplayFilesRootDirectory())),
    GetParamAsString());
}  // namespace password_manager
