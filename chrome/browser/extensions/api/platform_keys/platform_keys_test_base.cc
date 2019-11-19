// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/platform_keys/platform_keys_test_base.h"

#include "base/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/policy/affiliation_test_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_launcher.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "extensions/test/result_catcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kAffiliationID[] = "some-affiliation-id";
const char kTestUserinfoToken[] = "fake-userinfo-token";

PlatformKeysTestBase::PlatformKeysTestBase(
    SystemTokenStatus system_token_status,
    EnrollmentStatus enrollment_status,
    UserStatus user_status)
    : system_token_status_(system_token_status),
      enrollment_status_(enrollment_status),
      user_status_(user_status),
      account_id_(AccountId::FromUserEmailGaiaId(
          policy::AffiliationTestHelper::kEnterpriseUserEmail,
          policy::AffiliationTestHelper::kEnterpriseUserGaiaId)) {
  // Command line should not be tweaked as if user is already logged in.
  set_chromeos_user_ = false;
  // We log in without running browser.
  set_exit_when_last_browser_closes(false);
}

PlatformKeysTestBase::~PlatformKeysTestBase() {}

void PlatformKeysTestBase::SetUp() {
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir);

  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &FakeGaia::HandleRequest, base::Unretained(&fake_gaia_)));

  // Don't spin up the IO thread yet since no threads are allowed while
  // spawning sandbox host process. See crbug.com/322732.
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

  // Start https wrapper here so that the URLs can be pointed at it in
  // SetUpCommandLine().
  ASSERT_TRUE(gaia_https_forwarder_.Initialize(
      GaiaUrls::GetInstance()->gaia_url().host(),
      embedded_test_server()->base_url()));

  extensions::ExtensionApiTest::SetUp();
}

void PlatformKeysTestBase::SetUpCommandLine(base::CommandLine* command_line) {
  extensions::ExtensionApiTest::SetUpCommandLine(command_line);

  policy::AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
      command_line);

  const GURL gaia_url = gaia_https_forwarder_.GetURLForSSLHost(std::string());
  command_line->AppendSwitchASCII(::switches::kGaiaUrl, gaia_url.spec());
  command_line->AppendSwitchASCII(::switches::kLsoUrl, gaia_url.spec());
  command_line->AppendSwitchASCII(::switches::kGoogleApisUrl, gaia_url.spec());

  fake_gaia_.Initialize();
  fake_gaia_.set_issue_oauth_code_cookie(true);
}

void PlatformKeysTestBase::SetUpInProcessBrowserTestFixture() {
  extensions::ExtensionApiTest::SetUpInProcessBrowserTestFixture();

  chromeos::SessionManagerClient::InitializeFakeInMemory();

  policy::AffiliationTestHelper affiliation_helper =
      policy::AffiliationTestHelper::CreateForCloud(
          chromeos::FakeSessionManagerClient::Get());

  if (enrollment_status() == EnrollmentStatus::ENROLLED) {
    std::set<std::string> device_affiliation_ids;
    device_affiliation_ids.insert(kAffiliationID);
    ASSERT_NO_FATAL_FAILURE(affiliation_helper.SetDeviceAffiliationIDs(
        &device_policy_test_helper_, device_affiliation_ids));
    device_policy_test_helper_.InstallOwnerKey();
    install_attributes_.Get()->SetCloudManaged(
        policy::PolicyBuilder::kFakeDomain,
        policy::PolicyBuilder::kFakeDeviceId);
  }

  if (user_status() == UserStatus::MANAGED_AFFILIATED_DOMAIN) {
    std::set<std::string> user_affiliation_ids;
    user_affiliation_ids.insert(kAffiliationID);
    policy::UserPolicyBuilder user_policy;
    ASSERT_NO_FATAL_FAILURE(affiliation_helper.SetUserAffiliationIDs(
        &user_policy, account_id_, user_affiliation_ids));
  }

  EXPECT_CALL(mock_policy_provider_, IsInitializationComplete(testing::_))
      .WillRepeatedly(testing::Return(true));
  mock_policy_provider_.SetAutoRefresh();
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
      &mock_policy_provider_);
}

void PlatformKeysTestBase::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");
  // Start the accept thread as the sandbox host process has already been
  // spawned.
  embedded_test_server()->StartAcceptingConnections();

  FakeGaia::AccessTokenInfo token_info;
  token_info.scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);
  token_info.scopes.insert(GaiaConstants::kOAuthWrapBridgeUserInfoScope);
  token_info.audience = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  token_info.token = kTestUserinfoToken;
  token_info.email = account_id_.GetUserEmail();
  fake_gaia_.IssueOAuthToken(policy::AffiliationTestHelper::kFakeRefreshToken,
                             token_info);

  // On PRE_ test stage list of users is empty at this point. Then in the body
  // of PRE_ test kEnterpriseUser is added. Afterwards in the main test flow
  // after PRE_ test the list of user contains one kEnterpriseUser user.
  // This user logs in.
  if (!IsPreTest()) {
    policy::AffiliationTestHelper::LoginUser(account_id_);

    if (user_status() != UserStatus::UNMANAGED) {
      policy::ProfilePolicyConnector* const connector =
          profile()->GetProfilePolicyConnector();
      connector->OverrideIsManagedForTesting(true);
    }
  }

  if (system_token_status() == SystemTokenStatus::EXISTS) {
    base::RunLoop loop;
    base::PostTask(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&PlatformKeysTestBase::SetUpTestSystemSlotOnIO,
                       base::Unretained(this), loop.QuitClosure()));
    loop.Run();
  }

  extensions::ExtensionApiTest::SetUpOnMainThread();
}

void PlatformKeysTestBase::TearDownOnMainThread() {
  extensions::ExtensionApiTest::TearDownOnMainThread();

  if (system_token_status() == SystemTokenStatus::EXISTS) {
    base::RunLoop loop;
    base::PostTask(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&PlatformKeysTestBase::TearDownTestSystemSlotOnIO,
                       base::Unretained(this), loop.QuitClosure()));
    loop.Run();
  }
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
}

void PlatformKeysTestBase::PrepareTestSystemSlotOnIO(
    crypto::ScopedTestSystemNSSKeySlot* system_slot) {}

void PlatformKeysTestBase::RunPreTest() {
  policy::AffiliationTestHelper::PreLoginUser(account_id_);
}

bool PlatformKeysTestBase::TestExtension(const std::string& page_url) {
  DCHECK(!page_url.empty()) << "page_url cannot be empty";
  Browser* const browser = CreateBrowser(profile());

  extensions::ResultCatcher catcher;
  ui_test_utils::NavigateToURL(browser, GURL(page_url));

  if (!catcher.GetNextResult()) {
    message_ = catcher.message();
    return false;
  }
  return true;
}

bool PlatformKeysTestBase::IsPreTest() {
  return content::IsPreTest();
}

void PlatformKeysTestBase::SetUpTestSystemSlotOnIO(
    base::OnceClosure done_callback) {
  test_system_slot_ = std::make_unique<crypto::ScopedTestSystemNSSKeySlot>();
  ASSERT_TRUE(test_system_slot_->ConstructedSuccessfully());

  PrepareTestSystemSlotOnIO(test_system_slot_.get());

  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 std::move(done_callback));
}

void PlatformKeysTestBase::TearDownTestSystemSlotOnIO(
    base::OnceClosure done_callback) {
  test_system_slot_.reset();

  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 std::move(done_callback));
}
