// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/gaia_remote_consent_flow.h"

#include "chrome/browser/extensions/api/identity/identity_private_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/api_test_utils.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_auth_test_util.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/network/portal_detector/mock_network_portal_detector.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// TODO(rsult): Issues with creating a primary account on Lacros on test setup.
// Should be reworked asap to make it pass as this feature is available on
// Lacros as well.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
namespace extensions {

namespace {

constexpr char kTestEmail[] = "test@example.com";
constexpr char kGaiaId[] = "gaia_id_for_test_example.com";
constexpr char kFakeRefreshToken[] = "fake-refersh-token";

constexpr char kTestAuthSIDCookie[] = "fake-auth-SID-cookie";
constexpr char kTestAuthLSIDCookie[] = "fake-auth-LSID-cookie";

}  // namespace

class MockGaiaRemoteConsentFlowDelegate
    : public GaiaRemoteConsentFlow::Delegate {
 public:
  MOCK_METHOD1(OnGaiaRemoteConsentFlowFailed,
               void(GaiaRemoteConsentFlow::Failure failure));
  MOCK_METHOD2(OnGaiaRemoteConsentFlowApproved,
               void(const std::string& consent_result,
                    const std::string& gaia_id));
};

class GaiaRemoteConsentFlowBrowserTest : public InProcessBrowserTest {
 public:
  GaiaRemoteConsentFlowBrowserTest()
      : fake_gaia_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    std::unique_ptr<ash::MockNetworkPortalDetector>
        mock_network_portal_detector_ =
            std::make_unique<ash::MockNetworkPortalDetector>();

    EXPECT_CALL(*mock_network_portal_detector_, GetCaptivePortalStatus())
        .Times(testing::AnyNumber())
        .WillRepeatedly(
            testing::Return(ash::NetworkPortalDetector::CaptivePortalStatus::
                                CAPTIVE_PORTAL_STATUS_ONLINE));
    ash::network_portal_detector::InitializeForTesting(
        mock_network_portal_detector_.release());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    fake_gaia_test_server()->AddDefaultHandlers(GetChromeTestDataDir());
    fake_gaia_test_server_.RegisterRequestHandler(base::BindRepeating(
        &FakeGaia::HandleRequest, base::Unretained(&fake_gaia_)));
  }

  void SetUp() override {
    ASSERT_TRUE(fake_gaia_test_server_.InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    fake_gaia_test_server_.StartAcceptingConnections();
    fake_gaia_.SetFakeMergeSessionParams(kTestEmail, kTestAuthSIDCookie,
                                         kTestAuthLSIDCookie);
  }

  void TearDownOnMainThread() override {
    // Destroying GaiaRemoteConsentFlow early to avoid receiving unexpected
    // messages from Observed objects such as the app window.
    flow_.reset();
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    const GURL& base_url = fake_gaia_test_server_.base_url();
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch("ignore-certificate-errors");
    command_line->AppendSwitchASCII(switches::kGaiaUrl, base_url.spec());
    command_line->AppendSwitchASCII(switches::kGoogleApisUrl, base_url.spec());
    command_line->AppendSwitchASCII(switches::kOAuth2ClientID, base_url.spec());

    fake_gaia_.Initialize();
    fake_gaia_.MapEmailToGaiaId(kTestEmail, kGaiaId);

    FakeGaia::AccessTokenInfo token_info;
    token_info.token = "fake-userinfo-token-1";
    token_info.id_token = kGaiaId;
    token_info.audience = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
    token_info.email = kTestEmail;
    token_info.any_scope = true;
    token_info.user_id = kGaiaId;
    fake_gaia_.IssueOAuthToken(kFakeRefreshToken, token_info);
  }

  CoreAccountInfo CreateFakeAccountInfoAndSetAsPrimary() {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile());
    CoreAccountInfo account_info = SetPrimaryAccount(
        identity_manager, kTestEmail, signin::ConsentLevel::kSync);
    SetRefreshTokenForPrimaryAccount(identity_manager, kFakeRefreshToken);

    AccountInfo primary_account_info =
        identity_manager->FindExtendedAccountInfoByAccountId(
            account_info.account_id);
    DCHECK(!primary_account_info.IsEmpty());
    return primary_account_info;
  }

  void LaunchAndWaitGaiaRemoteConsentFlow() {
    CoreAccountInfo account_info = CreateFakeAccountInfoAndSetAsPrimary();
    ExtensionTokenKey token_key("extension_id", account_info,
                                std::set<std::string>());
    RemoteConsentResolutionData resolution_data;
    resolution_data.url = fake_gaia_test_server()->GetURL("/title1.html");

    flow_ = std::make_unique<GaiaRemoteConsentFlow>(&mock(), profile(),
                                                    token_key, resolution_data);

    content::TestNavigationObserver navigation_observer(resolution_data.url);
    navigation_observer.StartWatchingNewWebContents();

    flow_->Start();

    navigation_observer.Wait();
  }

  void SimulateConsentResult(const std::string& consent_value,
                             const std::string& window_id) {
    scoped_refptr<ExtensionFunction> func =
        base::MakeRefCounted<IdentityPrivateSetConsentResultFunction>();
    ASSERT_TRUE(api_test_utils::RunFunction(
        func.get(), "[\"" + consent_value + "\", \"" + window_id + "\"]",
        profile()));
  }

  MockGaiaRemoteConsentFlowDelegate& mock() {
    return mock_gaia_remote_consent_flow_delegate_;
  }

  net::EmbeddedTestServer* fake_gaia_test_server() {
    return &fake_gaia_test_server_;
  }

  Profile* profile() { return browser()->profile(); }

  GaiaRemoteConsentFlow* flow() { return flow_.get(); }

 private:
  std::unique_ptr<GaiaRemoteConsentFlow> flow_;

  MockGaiaRemoteConsentFlowDelegate mock_gaia_remote_consent_flow_delegate_;

  net::EmbeddedTestServer fake_gaia_test_server_;
  FakeGaia fake_gaia_;
};

IN_PROC_BROWSER_TEST_F(GaiaRemoteConsentFlowBrowserTest,
                       SimulateInvalidConsent) {
  LaunchAndWaitGaiaRemoteConsentFlow();

  EXPECT_CALL(mock(),
              OnGaiaRemoteConsentFlowFailed(
                  GaiaRemoteConsentFlow::Failure::INVALID_CONSENT_RESULT));
  SimulateConsentResult("invalid_consent",
                        flow()->GetWebAuthFlowForTesting()->GetAppWindowKey());
}

IN_PROC_BROWSER_TEST_F(GaiaRemoteConsentFlowBrowserTest, SimulateNoGrant) {
  LaunchAndWaitGaiaRemoteConsentFlow();

  EXPECT_CALL(mock(), OnGaiaRemoteConsentFlowFailed(
                          GaiaRemoteConsentFlow::Failure::NO_GRANT));
  std::string declined_consent = gaia::GenerateOAuth2MintTokenConsentResult(
      /*approved=*/false, "consent_not_granted", kGaiaId);
  SimulateConsentResult(declined_consent,
                        flow()->GetWebAuthFlowForTesting()->GetAppWindowKey());
}

IN_PROC_BROWSER_TEST_F(GaiaRemoteConsentFlowBrowserTest,
                       SimulateAccessGranted) {
  LaunchAndWaitGaiaRemoteConsentFlow();

  std::string approved_consent = gaia::GenerateOAuth2MintTokenConsentResult(
      /*approved=*/true, "consent_granted", kGaiaId);
  EXPECT_CALL(mock(),
              OnGaiaRemoteConsentFlowApproved(approved_consent, kGaiaId));
  SimulateConsentResult(approved_consent,
                        flow()->GetWebAuthFlowForTesting()->GetAppWindowKey());
}

}  // namespace extensions
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
