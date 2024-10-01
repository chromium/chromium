// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/platform_keys/platform_keys_test_base.h"

#include <array>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/extensions/mixin_based_extension_apitest.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
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

constexpr char kAffiliationID[] = "some-affiliation-id";
constexpr char kTestUserinfoToken[] = "fake-userinfo-token";

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
  cryptohome_mixin_.MarkUserAsExisting(account_id_);
  cryptohome_mixin_.ApplyAuthConfig(
      account_id_,
      ash::test::UserAuthConfig::Create(ash::test::kDefaultAuthSetup));
}

PlatformKeysTestBase::~PlatformKeysTestBase() {}

void PlatformKeysTestBase::SetUp() {
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir);

  net::EmbeddedTestServer::ServerCertificateConfig gaia_cert_config;
  gaia_cert_config.dns_names = {GaiaUrls::GetInstance()->gaia_url().host()};
  gaia_server_.SetSSLConfig(gaia_cert_config);
  gaia_server_.RegisterRequestHandler(base::BindRepeating(
      &FakeGaia::HandleRequest, base::Unretained(&fake_gaia_)));

  // Initialize the server to allocate a port, so that URLs can be pointed at it
  // in SetUpCommandLine(). Don't spin up the IO thread yet since no threads are
  // allowed while spawning sandbox host process. See crbug.com/322732.
  ASSERT_TRUE(gaia_server_.InitializeAndListen());

  ash::platform_keys::PlatformKeysServiceFactory::GetInstance()->SetTestingMode(
      true);

  if (system_token_status() == SystemTokenStatus::EXISTS) {
    CreateTestSystemSlot();
  }

  extensions::MixinBasedExtensionApiTest::SetUp();
}

void PlatformKeysTestBase::SetUpCommandLine(base::CommandLine* command_line) {
  extensions::MixinBasedExtensionApiTest::SetUpCommandLine(command_line);

  policy::AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
      command_line);

  const GURL gaia_url =
      gaia_server_.GetURL(GaiaUrls::GetInstance()->gaia_url().host(), "/");
  command_line->AppendSwitchASCII(::switches::kGaiaUrl, gaia_url.spec());
  command_line->AppendSwitchASCII(::switches::kLsoUrl, gaia_url.spec());
  command_line->AppendSwitchASCII(::switches::kGoogleApisUrl, gaia_url.spec());

  fake_gaia_.Initialize();
  fake_gaia_.set_issue_oauth_code_cookie(true);
}

void PlatformKeysTestBase::SetUpInProcessBrowserTestFixture() {
  extensions::MixinBasedExtensionApiTest::SetUpInProcessBrowserTestFixture();

  ash::SessionManagerClient::InitializeFakeInMemory();

  policy::AffiliationTestHelper affiliation_helper =
      policy::AffiliationTestHelper::CreateForCloud(
          ash::FakeSessionManagerClient::Get());

  if (enrollment_status() == EnrollmentStatus::ENROLLED) {
    ASSERT_NO_FATAL_FAILURE(affiliation_helper.SetDeviceAffiliationIDs(
        &device_policy_test_helper_,
        std::array{std::string_view(kAffiliationID)}));
    device_policy_test_helper_.InstallOwnerKey();
    install_attributes_.Get()->SetCloudManaged(
        policy::PolicyBuilder::kFakeDomain,
        policy::PolicyBuilder::kFakeDeviceId);
  }

  if (user_status() == UserStatus::MANAGED_AFFILIATED_DOMAIN) {
    policy::UserPolicyBuilder user_policy;
    ASSERT_NO_FATAL_FAILURE(affiliation_helper.SetUserAffiliationIDs(
        &user_policy, account_id_,
        std::array{std::string_view(kAffiliationID)}));
  }

  mock_policy_provider_.SetDefaultReturns(
      /*is_initialization_complete_return=*/true,
      /*is_first_policy_load_complete_return=*/true);
  mock_policy_provider_.SetAutoRefresh();
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
      &mock_policy_provider_);
}

void PlatformKeysTestBase::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");
  // Start the accept thread as the sandbox host process has already been
  // spawned.
  ASSERT_TRUE(embedded_test_server()->Start());
  gaia_server_.StartAcceptingConnections();

  FakeGaia::AccessTokenInfo token_info;
  token_info.scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);
  token_info.scopes.insert(GaiaConstants::kGoogleUserInfoEmail);
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
    // Call specializations of the virtual method that configures the created
    // system slot.
    content::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&PlatformKeysTestBase::PrepareTestSystemSlotOnIO,
                       base::Unretained(this),
                       base::Unretained(test_system_slot_.get())),
        loop.QuitClosure());
    loop.Run();
  }

  extensions::MixinBasedExtensionApiTest::SetUpOnMainThread();
}

void PlatformKeysTestBase::TearDownOnMainThread() {
  extensions::MixinBasedExtensionApiTest::TearDownOnMainThread();

  ash::platform_keys::PlatformKeysServiceFactory::GetInstance()->SetTestingMode(
      false);

  if (system_token_status() == SystemTokenStatus::EXISTS) {
    base::RunLoop loop;
    content::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&PlatformKeysTestBase::TearDownTestSystemSlotOnIO,
                       base::Unretained(this)),
        loop.QuitClosure());
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
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, GURL(page_url)));

  if (!catcher.GetNextResult()) {
    message_ = catcher.message();
    return false;
  }
  return true;
}

bool PlatformKeysTestBase::IsPreTest() {
  return content::IsPreTest();
}

void PlatformKeysTestBase::CreateTestSystemSlot() {
  test_system_slot_ = std::make_unique<crypto::ScopedTestSystemNSSKeySlot>(
      /*simulate_token_loader=*/false);
  ASSERT_TRUE(test_system_slot_->ConstructedSuccessfully());
}

void PlatformKeysTestBase::TearDownTestSystemSlotOnIO() {
  test_system_slot_.reset();
}
