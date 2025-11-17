// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/compiler_specific.h"
#include "base/containers/span_reader.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/util.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/ssl_test_util.h"
#include "net/test/test_doh_server.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "third_party/boringssl/src/include/openssl/tls1.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/saml/fake_saml_idp_mixin.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/test/auth_ui_utils.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/users/test_users.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/net_errors.h"
#endif

namespace policy {

namespace {

// Returns an SSLServerConfig that causes the test server to reject any TLS
// handshake unless it specifies the three TLS 1.3 cipher names in the expected
// order for CNSA, which is different from the default order. This effectively
// causes the connection to fail unless the PreferSlowCiphers policy is
// correctly configured for "cnsa".
net::SSLServerConfig GetServerConfigForPreferSlowCiphersTest() {
  const auto kExpectedCipherNamesWithPolicy = std::to_array<const char*>(
      {"TLS_AES_256_GCM_SHA384", "TLS_AES_128_GCM_SHA256",
       "TLS_CHACHA20_POLY1305_SHA256"});

  net::SSLServerConfig ssl_config;
  ssl_config.version_min = net::SSL_PROTOCOL_VERSION_TLS1_3;
  ssl_config.version_max = net::SSL_PROTOCOL_VERSION_TLS1_3;
  // Make the test server only accept the expected TLS 1.3 ciphers in the
  // order specified by the policy, or otherwise reject the handshake.
  ssl_config.client_hello_callback_for_testing =
      base::BindLambdaForTesting([&](const SSL_CLIENT_HELLO* client_hello) {
        // Each cipher is encoded as a two-byte, big-endian integer.
        CHECK(client_hello->cipher_suites_len % 2 == 0);
        // SAFETY: BoringSSL API guarantees that `client_hello->cipher_suites`
        // and `client_hello->ciphers_suites_len` describe a valid span.
        auto cipher_suites_reader = base::SpanReader{UNSAFE_BUFFERS(base::span{
            client_hello->cipher_suites, client_hello->cipher_suites_len})};
        uint16_t value;
        std::vector<std::string> cipher_names;
        while (cipher_suites_reader.ReadU16BigEndian(value)) {
          // Skip values we don't recognize as any TLS 1.3 cipher.
          const SSL_CIPHER* cipher = SSL_get_cipher_by_value(value);
          if (!cipher || SSL_CIPHER_get_min_version(cipher) != TLS1_3_VERSION) {
            continue;
          }
          cipher_names.emplace_back(SSL_CIPHER_standard_name(cipher));
        }
        return !std::ranges::search(cipher_names,
                                    kExpectedCipherNamesWithPolicy)
                    .empty();
      });
  return ssl_config;
}

bool GetLocalStateBooleanPref(const std::string& pref_name) {
  return g_browser_process->local_state()->GetBoolean(pref_name);
}

std::optional<bool> GetManagedBooleanPref(PrefService* prefs,
                                          const std::string_view pref_name) {
  if (prefs->IsManagedPreference(pref_name)) {
    return prefs->GetBoolean(pref_name);
  }
  return std::nullopt;
}

std::optional<bool> GetLocalStateManagedBooleanPref(
    const std::string_view pref_name) {
  return GetManagedBooleanPref(g_browser_process->local_state(), pref_name);
}

std::optional<std::string> GetManagedStringPref(
    PrefService* prefs,
    const std::string_view pref_name) {
  if (prefs->IsManagedPreference(pref_name)) {
    return prefs->GetString(pref_name);
  }
  return std::nullopt;
}

std::optional<std::string> GetLocalStateManagedStringPref(
    const std::string_view pref_name) {
  return GetManagedStringPref(g_browser_process->local_state(), pref_name);
}

}  // namespace

class SSLPolicyTest : public PolicyTest {
 public:
  SSLPolicyTest() = default;
  ~SSLPolicyTest() override = default;

  void TearDownOnMainThread() override {
    ASSERT_TRUE(https_server_.ShutdownAndWaitUntilComplete());
    PolicyTest::TearDownOnMainThread();
  }

 protected:
  struct LoadResult {
    bool success;
    std::u16string title;
  };

  bool StartTestServer(const net::SSLServerConfig& ssl_config) {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    return https_server_.Start();
  }

  LoadResult LoadPage(std::string_view path) {
    return LoadPage(https_server_.GetURL(path));
  }

  LoadResult LoadPage(const GURL& url) {
    EXPECT_TRUE(NavigateToUrl(url, this));
    content::WebContents* web_contents =
        chrome_test_utils::GetActiveWebContents(this);
    if (web_contents->GetController().GetLastCommittedEntry()->GetPageType() ==
        content::PAGE_TYPE_ERROR) {
      return LoadResult{false, u""};
    }
    return LoadResult{true, web_contents->GetTitle()};
  }

  void ExpectVersionOrCipherMismatch() {
    content::WebContents* web_contents =
        chrome_test_utils::GetActiveWebContents(this);
    EXPECT_TRUE(content::EvalJs(web_contents,
                                "document.body.innerHTML.indexOf('ERR_SSL_"
                                "VERSION_OR_CIPHER_MISMATCH') >= 0")
                    .ExtractBool());
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(SSLPolicyTest, PostQuantumEnabledPolicy) {
  net::SSLServerConfig ssl_config;
  ssl_config.curves_for_testing = {NID_X25519MLKEM768};
  ASSERT_TRUE(StartTestServer(ssl_config));

  // Should be able to load a page from the test server because policy is in
  // the default state and feature is enabled.
  EXPECT_EQ(
      GetLocalStateManagedBooleanPref(prefs::kPostQuantumKeyAgreementEnabled),
      std::nullopt);
  LoadResult result = LoadPage("/title2.html");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(u"Title Of Awesomeness", result.title);

  // Disable the policy.
  PolicyMap policies;
  SetPolicy(&policies, key::kPostQuantumKeyAgreementEnabled,
            base::Value(false));
  UpdateProviderPolicy(policies);
  content::FlushNetworkServiceInstanceForTesting();

  // Page loads should now fail.
  EXPECT_EQ(
      GetLocalStateManagedBooleanPref(prefs::kPostQuantumKeyAgreementEnabled),
      false);
  result = LoadPage("/title3.html");
  EXPECT_FALSE(result.success);

  // Enable the policy.
  PolicyMap policies2;
  SetPolicy(&policies2, key::kPostQuantumKeyAgreementEnabled,
            base::Value(true));
  UpdateProviderPolicy(policies2);
  content::FlushNetworkServiceInstanceForTesting();

  // Page load should now succeed.
  EXPECT_EQ(
      GetLocalStateManagedBooleanPref(prefs::kPostQuantumKeyAgreementEnabled),
      true);
  result = LoadPage("/title2.html");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(u"Title Of Awesomeness", result.title);
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SSLPolicyTest, DevicePostQuantumEnabledPolicy) {
  net::SSLServerConfig ssl_config;
  ssl_config.curves_for_testing = {NID_X25519MLKEM768};
  ASSERT_TRUE(StartTestServer(ssl_config));

  // Should be able to load a page from the test server because policy is in
  // the default state and feature is enabled.
  EXPECT_EQ(
      GetLocalStateManagedBooleanPref(prefs::kPostQuantumKeyAgreementEnabled),
      std::nullopt);
  EXPECT_EQ(GetLocalStateManagedBooleanPref(
                prefs::kDevicePostQuantumKeyAgreementEnabled),
            std::nullopt);
  LoadResult result = LoadPage("/title2.html");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(u"Title Of Awesomeness", result.title);

  {
    // Disable the device policy.
    PolicyMap policies;
    SetPolicy(&policies, key::kDevicePostQuantumKeyAgreementEnabled,
              base::Value(false));
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  // Page loads should now fail.
  EXPECT_EQ(
      GetLocalStateManagedBooleanPref(prefs::kPostQuantumKeyAgreementEnabled),
      std::nullopt);
  EXPECT_EQ(GetLocalStateManagedBooleanPref(
                prefs::kDevicePostQuantumKeyAgreementEnabled),
            false);
  result = LoadPage("/title3.html");
  EXPECT_FALSE(result.success);

  {
    // Try enabling the non-device policy, while the device policy is disabled.
    PolicyMap policies;
    SetPolicy(&policies, key::kPostQuantumKeyAgreementEnabled,
              base::Value(true));
    SetPolicy(&policies, key::kDevicePostQuantumKeyAgreementEnabled,
              base::Value(false));
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  // Page loads should still fail since the device policy takes precedence.
  EXPECT_EQ(
      GetLocalStateManagedBooleanPref(prefs::kPostQuantumKeyAgreementEnabled),
      true);
  EXPECT_EQ(GetLocalStateManagedBooleanPref(
                prefs::kDevicePostQuantumKeyAgreementEnabled),
            false);
  result = LoadPage("/title3.html");
  EXPECT_FALSE(result.success);

  {
    // Enable the device policy.
    PolicyMap policies;
    SetPolicy(&policies, key::kDevicePostQuantumKeyAgreementEnabled,
              base::Value(true));
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  // Page load should now succeed.
  EXPECT_EQ(
      GetLocalStateManagedBooleanPref(prefs::kPostQuantumKeyAgreementEnabled),
      std::nullopt);
  EXPECT_EQ(GetLocalStateManagedBooleanPref(
                prefs::kDevicePostQuantumKeyAgreementEnabled),
            true);
  result = LoadPage("/title2.html");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(u"Title Of Awesomeness", result.title);

  {
    // Try disabling the non-device policy, while the device policy is enabled.
    PolicyMap policies;
    SetPolicy(&policies, key::kPostQuantumKeyAgreementEnabled,
              base::Value(false));
    SetPolicy(&policies, key::kDevicePostQuantumKeyAgreementEnabled,
              base::Value(true));
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  // Page load should still succeed since the device policy takes precedence.
  EXPECT_EQ(
      GetLocalStateManagedBooleanPref(prefs::kPostQuantumKeyAgreementEnabled),
      false);
  EXPECT_EQ(GetLocalStateManagedBooleanPref(
                prefs::kDevicePostQuantumKeyAgreementEnabled),
            true);
  result = LoadPage("/title2.html");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(u"Title Of Awesomeness", result.title);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(SSLPolicyTest, PreferSlowKexAlgorithmsPolicy) {
  net::SSLServerConfig ssl_config;
  ssl_config.curves_for_testing = {NID_ML_KEM_1024};
  ASSERT_TRUE(StartTestServer(ssl_config));

  // Should fail to load a page from the test server because, by default, we
  // don't negotiate ML-KEM-1024.
  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowKexAlgorithms),
            std::nullopt);
  LoadResult result = LoadPage("/title2.html");
  EXPECT_FALSE(result.success);

  // Set the policy to cnsa2 to prefer ML-KEM-1024.
  {
    PolicyMap policies;
    SetPolicy(&policies, key::kPreferSlowKexAlgorithms, base::Value("cnsa2"));
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  // Page load should now succeed.
  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowKexAlgorithms),
            "cnsa2");
  result = LoadPage("/title2.html");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(u"Title Of Awesomeness", result.title);

  // Set the policy to an unrecognized value; this falls back to the defaults.
  {
    PolicyMap policies;
    SetPolicy(&policies, key::kPreferSlowKexAlgorithms, base::Value("bogus"));
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  // Page load should now fail.
  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowKexAlgorithms),
            "bogus");
  result = LoadPage("/title2.html");
  EXPECT_FALSE(result.success);
}

IN_PROC_BROWSER_TEST_F(SSLPolicyTest,
                       PostQuantumDisabledOverridesPreferSlowKexAlgorithms) {
  net::SSLServerConfig ssl_config;
  ssl_config.curves_for_testing = {NID_ML_KEM_1024};
  ASSERT_TRUE(StartTestServer(ssl_config));

  PolicyMap policies;
  SetPolicy(&policies, key::kPreferSlowKexAlgorithms, base::Value("cnsa2"));
  SetPolicy(&policies, key::kPostQuantumKeyAgreementEnabled,
            base::Value(false));
  UpdateProviderPolicy(policies);
  content::FlushNetworkServiceInstanceForTesting();

  // Should fail to load a page from the test server because setting
  // PostQuantumKeyAgreementEnabled to disabled takes precedence over the
  // PreferSlowKexAlgorithms policy.
  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowKexAlgorithms),
            "cnsa2");
  EXPECT_FALSE(
      GetLocalStateManagedBooleanPref(prefs::kPostQuantumKeyAgreementEnabled)
          .value());
  LoadResult result = LoadPage("/title2.html");
  EXPECT_FALSE(result.success);
}

IN_PROC_BROWSER_TEST_F(SSLPolicyTest, PreferSlowCiphersPolicy) {
  ASSERT_TRUE(StartTestServer(GetServerConfigForPreferSlowCiphersTest()));

  // Should fail to load a page from the test server because the default
  // cipher order doesn't match and the test server rejects the handshake.
  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowCiphers),
            std::nullopt);
  LoadResult result = LoadPage("/title2.html");
  EXPECT_FALSE(result.success);

  // Set the policy to cnsa to prefer the TLS 1.3 ciphers in the expected order.
  {
    PolicyMap policies;
    SetPolicy(&policies, key::kPreferSlowCiphers, base::Value("cnsa"));
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  // Page load should now succeed.
  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowCiphers), "cnsa");
  result = LoadPage("/title2.html");
  EXPECT_TRUE(result.success);
  EXPECT_EQ(u"Title Of Awesomeness", result.title);

  // Set the policy to an unrecognized value; this falls back to the defaults.
  {
    PolicyMap policies;
    SetPolicy(&policies, key::kPreferSlowCiphers, base::Value("bogus"));
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  // Page load should now fail.
  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowCiphers), "bogus");
  result = LoadPage("/title2.html");
  EXPECT_FALSE(result.success);
}

#if BUILDFLAG(IS_CHROMEOS)
// Tests for the device login screen policies on ChromeOS, using a SAML login
// flow for ease of testing. SAML-related test code is adapted from
// //chrome/browser/ash/login/saml/saml_browsertest.cc.
class SSLDeviceLoginScreenPolicyTest : public ash::OobeBaseTest {
 public:
  SSLDeviceLoginScreenPolicyTest() {
    // Delay starting the SAML test server to customize the server behavior for
    // each test.
    fake_saml_idp_.set_auto_start_saml_servers(false);
    // Prevent the default FakeGaia configuration from overwriting SAML configs.
    fake_gaia_.set_initialize_configuration(false);
  }

  ~SSLDeviceLoginScreenPolicyTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ash::OobeBaseTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kOobeSkipPostLogin);
    command_line->AppendSwitch(ash::switches::kAllowFailedPolicyFetchForTest);
  }

  void SetUpOnMainThread() override {
    ash::OobeBaseTest::SetUpOnMainThread();
    login_profile_ = Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetSigninBrowserContext());
    ASSERT_TRUE(login_profile_);
    policy_helper_.InstallOwnerKey();
  }

  void TearDownOnMainThread() override {
    login_profile_ = nullptr;
    ash::OobeBaseTest::TearDownOnMainThread();
  }

  bool StartFakeSamlServerWithConfig(const net::SSLServerConfig& ssl_config) {
    fake_saml_idp_.saml_server()->SetSSLConfig(
        ash::FakeSamlIdpMixin::GetServerCertConfig(), ssl_config);
    return fake_saml_idp_.StartSamlServersNow();
  }

  std::optional<std::string> GetLoginProfileManagedStringPref(
      const std::string_view pref_name) {
    return GetManagedStringPref(login_profile_->GetPrefs(), pref_name);
  }

  void RefreshDevicePoliciesAndWaitForPrefChange(
      const std::string_view pref_name) {
    PrefService* prefs = login_profile_->GetPrefs();
    ASSERT_TRUE(prefs);

    PrefChangeRegistrar registrar;
    base::test::TestFuture<std::string_view> pref_changed_future;
    registrar.Init(prefs);
    registrar.Add(pref_name,
                  base::BindRepeating(
                      pref_changed_future.GetRepeatingCallback(), pref_name));
    policy_helper_.RefreshDevicePolicy();
    EXPECT_EQ(pref_name, pref_changed_future.Take());
  }

  // If `expected_net_error` is std::nullopt, then SAML login is expected to
  // succeed. Otherwise this expects to see an error screen when attempting to
  // load the SAML page.
  void SetUpAndAttemptSamlLogin(
      std::optional<net::Error> expected_net_error = std::nullopt) {
    fake_gaia_.fake_gaia()->RegisterSamlUser(account_id_.GetUserEmail(),
                                             fake_saml_idp_.GetSamlPageUrl());
    fake_gaia_.fake_gaia()->SetConfigurationHelper(account_id_.GetUserEmail(),
                                                   "fake-auth-SID-cookie",
                                                   "fake-auth-LSID-cookie");
    fake_gaia_.SetupFakeGaiaForLogin(account_id_.GetUserEmail(),
                                     account_id_.GetGaiaId(),
                                     FakeGaiaMixin::kFakeRefreshToken);
    fake_saml_idp_.SetLoginHTMLTemplate("saml_login.html");

    ash::OobeUI* oobe_ui = ash::LoginDisplayHost::default_host()->GetOobeUI();
    ash::test::OobeScreenWatcher<ash::ErrorScreenView> error_screen_watcher(
        oobe_ui);

    // Wait for the Gaia signin UI to load (in a webview).
    std::unique_ptr<ash::test::GaiaPageActor> gaia_ui =
        ash::test::AwaitGaiaSigninUI();

    std::optional<content::TestNavigationObserver>
        navigation_net_error_observer;
    if (expected_net_error.has_value()) {
      // Observe the OOBE UI and its children, including the signin webview, for
      // the expected net::Error.
      content::WebContents* oobe_web_contents =
          ash::LoginDisplayHost::default_host()->GetOobeWebContents();
      navigation_net_error_observer.emplace(oobe_web_contents,
                                            *expected_net_error);
      for (content::WebContents* wc :
           oobe_web_contents->GetInnerWebContents()) {
        navigation_net_error_observer->WatchWebContents(wc);
      }
    }

    // Submit the user's email to load the SAML page.
    gaia_ui->SubmitFullAuthEmail(account_id_);

    if (expected_net_error.has_value()) {
      navigation_net_error_observer->WaitForNavigationFinished();
      EXPECT_FALSE(navigation_net_error_observer->last_navigation_succeeded());
      EXPECT_EQ(navigation_net_error_observer->last_net_error_code(),
                *expected_net_error);
      const GURL& error_nav_url =
          navigation_net_error_observer->last_navigation_url();
      GURL::Replacements remove_query_str;
      remove_query_str.ClearQuery();
      EXPECT_EQ(error_nav_url.ReplaceComponents(remove_query_str),
                fake_saml_idp_.GetSamlPageUrl());

      if (!error_screen_watcher.has_target_screen_been_shown()) {
        ash::OobeScreenWaiter(ash::ErrorScreenView::kScreenId).Wait();
      }
      EXPECT_TRUE(error_screen_watcher.has_target_screen_been_shown());
      // The ErrorScreen sees issues with SAML page loading as "offline".
      // We don't currently see the ash::NetworkError::ERROR_REASON_FRAME_ERROR
      // that has been passed to ErrorScreen.
      EXPECT_EQ(oobe_ui->GetErrorScreen()->GetErrorState(),
                ash::NetworkError::ERROR_STATE_OFFLINE);
      return;
    }

    auto saml_waiter = CreateGaiaPageEventWaiter("samlPageLoaded");
    // The back button appears when the UI is ready.
    WaitForGaiaPageBackButtonUpdate();
    saml_waiter->Wait();
    // Fill in the SAML IdP form and submit.
    SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
    SigninFrameJS().TypeIntoPath("fake_password", {"Password"});
    SigninFrameJS().TapOn("Submit");
    ash::test::WaitForPrimaryUserSessionStart();
    EXPECT_FALSE(error_screen_watcher.has_target_screen_been_shown());
  }

 protected:
  AccountId account_id_ = AccountId::FromUserEmailGaiaId(
      ash::saml_test_users::kFirstUserCorpExampleComEmail,
      FakeGaiaMixin::kFakeUserGaiaId);
  ash::CryptohomeMixin cryptohome_mixin_{&mixin_host_};
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  DevicePolicyCrosTestHelper policy_helper_;
  FakeGaiaMixin fake_gaia_{&mixin_host_};
  ash::FakeSamlIdpMixin fake_saml_idp_{&mixin_host_, &fake_gaia_};
  raw_ptr<Profile> login_profile_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(SSLDeviceLoginScreenPolicyTest,
                       DeviceLoginScreenPreferSlowKexAlgorithmsPolicy) {
  // Set up the SAML server so it only allows clients supporting ML-KEM-1024.
  net::SSLServerConfig ssl_config;
  ssl_config.curves_for_testing = {NID_ML_KEM_1024};
  ASSERT_TRUE(StartFakeSamlServerWithConfig(ssl_config));

  // Check the initial state (no device or user policy).
  ASSERT_EQ(GetLoginProfileManagedStringPref(prefs::kPreferSlowKexAlgorithms),
            std::nullopt);
  ASSERT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowKexAlgorithms),
            std::nullopt);

  // Set the device login screen policy to cnsa2 to prefer ML-KEM-1024.
  policy_helper_.device_policy()
      ->payload()
      .mutable_deviceloginscreenpreferslowkexalgorithms()
      ->set_value("cnsa2");
  RefreshDevicePoliciesAndWaitForPrefChange(prefs::kPreferSlowKexAlgorithms);
  content::FlushNetworkServiceInstanceForTesting();

  // The pref is now set to the correct value in the login profile, and the
  // local state is unaffected.
  EXPECT_EQ(GetLoginProfileManagedStringPref(prefs::kPreferSlowKexAlgorithms),
            "cnsa2");
  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowKexAlgorithms),
            std::nullopt);

  // Login should succeed.
  SetUpAndAttemptSamlLogin();
}

IN_PROC_BROWSER_TEST_F(
    SSLDeviceLoginScreenPolicyTest,
    PreferSlowKexAlgorithmsDefaultOnLoginScreen_HandshakeFails) {
  // Set up the SAML server so it only allows clients supporting ML-KEM-1024.
  net::SSLServerConfig ssl_config;
  ssl_config.curves_for_testing = {NID_ML_KEM_1024};
  ASSERT_TRUE(StartFakeSamlServerWithConfig(ssl_config));

  // Without the value "cnsa2" for the device login screen policy, SAML login
  // should not work because we cannot negotiate any group for key exchange.
  ASSERT_EQ(GetLoginProfileManagedStringPref(prefs::kPreferSlowKexAlgorithms),
            std::nullopt);
  SetUpAndAttemptSamlLogin(net::ERR_SSL_VERSION_OR_CIPHER_MISMATCH);
}

IN_PROC_BROWSER_TEST_F(SSLDeviceLoginScreenPolicyTest,
                       DeviceLoginScreenPreferSlowCiphersPolicy) {
  // Set up the SAML server so it only allows clients with the CNSA cipher list
  // order.
  ASSERT_TRUE(
      StartFakeSamlServerWithConfig(GetServerConfigForPreferSlowCiphersTest()));

  // Check the initial state (no device or user policy).
  ASSERT_EQ(GetLoginProfileManagedStringPref(prefs::kPreferSlowCiphers),
            std::nullopt);
  ASSERT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowCiphers),
            std::nullopt);

  // Set the device login screen policy to cnsa to prefer the TLS 1.3 ciphers in
  // the expected order.
  policy_helper_.device_policy()
      ->payload()
      .mutable_deviceloginscreenpreferslowciphers()
      ->set_value("cnsa");
  RefreshDevicePoliciesAndWaitForPrefChange(prefs::kPreferSlowCiphers);

  // The pref is now set to the correct value in the login profile, and the
  // local state is unaffected.
  EXPECT_EQ(GetLoginProfileManagedStringPref(prefs::kPreferSlowCiphers),
            "cnsa");
  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowCiphers),
            std::nullopt);

  // Login should succeed.
  SetUpAndAttemptSamlLogin();
}

IN_PROC_BROWSER_TEST_F(SSLDeviceLoginScreenPolicyTest,
                       PreferSlowCiphersDefaultOnLoginScreen_HandshakeFails) {
  // Set up the SAML server so it only allows clients with the CNSA cipher list
  // order.
  ASSERT_TRUE(
      StartFakeSamlServerWithConfig(GetServerConfigForPreferSlowCiphersTest()));

  // Without the value "cnsa" for the device login screen policy, SAML login
  // should not work because the test server rejects the handshake.
  ASSERT_EQ(GetLoginProfileManagedStringPref(prefs::kPreferSlowCiphers),
            std::nullopt);
  SetUpAndAttemptSamlLogin(net::ERR_SSL_VERSION_OR_CIPHER_MISMATCH);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// Tests the interaction between features::kCryptographyComplianceCnsa and the
// policies. The test parameter indicates whether the feature is enabled.
class SSLPolicyTestWithCnsaFeature : public SSLPolicyTest,
                                     public testing::WithParamInterface<bool> {
 public:
  SSLPolicyTestWithCnsaFeature() {
    features_.InitWithFeatureState(features::kCryptographyComplianceCnsa,
                                   IsCnsaFeatureEnabled());
  }

  bool IsCnsaFeatureEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(/*no prefix*/,
                         SSLPolicyTestWithCnsaFeature,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(SSLPolicyTestWithCnsaFeature,
                       CryptographyComplianceFeatureKeyExchange) {
  net::SSLServerConfig ssl_config;
  ssl_config.curves_for_testing = {NID_ML_KEM_1024};
  ASSERT_TRUE(StartTestServer(ssl_config));

  // If the policies disable CNSA behavior, the base::Feature can override and
  // enable it.
  {
    PolicyMap policies;
    SetPolicy(&policies, key::kPreferSlowKexAlgorithms, base::Value("default"));
#if BUILDFLAG(IS_CHROMEOS)
    SetPolicy(&policies, key::kDeviceLoginScreenPreferSlowKexAlgorithms,
              base::Value("default"));
#endif
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowKexAlgorithms),
            "default");
  LoadResult result = LoadPage("/title2.html");
  EXPECT_EQ(result.success, IsCnsaFeatureEnabled());

  // If the policy enables CNSA behavior, it is enabled regardless of the
  // base::Feature.
  {
    PolicyMap policies;
    SetPolicy(&policies, key::kPreferSlowKexAlgorithms, base::Value("cnsa2"));
#if BUILDFLAG(IS_CHROMEOS)
    SetPolicy(&policies, key::kDeviceLoginScreenPreferSlowKexAlgorithms,
              base::Value("cnsa2"));
#endif
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowKexAlgorithms),
            "cnsa2");
  result = LoadPage("/title2.html");
  EXPECT_TRUE(result.success);
}

IN_PROC_BROWSER_TEST_P(SSLPolicyTestWithCnsaFeature,
                       CryptographyComplianceFeatureCiphers) {
  ASSERT_TRUE(StartTestServer(GetServerConfigForPreferSlowCiphersTest()));

  // If the policy disables CNSA behavior, the base::Feature can override and
  // enable it.
  {
    PolicyMap policies;
    SetPolicy(&policies, key::kPreferSlowCiphers, base::Value("default"));
#if BUILDFLAG(IS_CHROMEOS)
    SetPolicy(&policies, key::kDeviceLoginScreenPreferSlowCiphers,
              base::Value("default"));
#endif
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowCiphers),
            "default");
  LoadResult result = LoadPage("/title2.html");
  EXPECT_EQ(result.success, IsCnsaFeatureEnabled());

  // If the policy enables CNSA behavior, it is enabled regardless of the
  // base::Feature.
  {
    PolicyMap policies;
    SetPolicy(&policies, key::kPreferSlowCiphers, base::Value("cnsa"));
#if BUILDFLAG(IS_CHROMEOS)
    SetPolicy(&policies, key::kDeviceLoginScreenPreferSlowCiphers,
              base::Value("cnsa"));
#endif
    UpdateProviderPolicy(policies);
    content::FlushNetworkServiceInstanceForTesting();
  }

  EXPECT_EQ(GetLocalStateManagedStringPref(prefs::kPreferSlowCiphers), "cnsa");
  result = LoadPage("/title2.html");
  EXPECT_TRUE(result.success);
}

class ECHPolicyTest : public SSLPolicyTest {
 public:
  // a.test is covered by `CERT_TEST_NAMES`.
  static constexpr std::string_view kHostname = "a.test";
  static constexpr std::string_view kPublicName = "public-name.test";
  static constexpr std::string_view kDohServerHostname = "doh.test";

  static constexpr std::string_view kECHSuccessTitle = "Negotiated ECH";
  static constexpr std::string_view kECHFailureTitle = "Did not negotiate ECH";

  ECHPolicyTest() : ech_server_{net::EmbeddedTestServer::TYPE_HTTPS} {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{net::features::kUseDnsHttpsSvcb,
          {{"UseDnsHttpsSvcbEnforceSecureResponse", "true"}}}},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    // Configure `ech_server_` to enable and require ECH.
    net::SSLServerConfig server_config;
    std::vector<uint8_t> ech_config_list;
    server_config.ech_keys = net::MakeTestEchKeys(
        kPublicName, /*max_name_len=*/64, &ech_config_list);
    ASSERT_TRUE(server_config.ech_keys);
    ech_server_.RegisterRequestHandler(
        base::BindRepeating(&ECHPolicyTest::HandleRequest));
    ech_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES,
                             server_config);

    ASSERT_TRUE(ech_server_.Start());

    // Start a DoH server, which ensures we use a resolver with HTTPS RR
    // support. Configure it to serve records for `ech_server_`.
    doh_server_.SetHostname(kDohServerHostname);
    url::SchemeHostPort ech_host(GetURL("/"));
    doh_server_.AddAddressRecord(ech_host.host(),
                                 net::IPAddress::IPv4Localhost());
    doh_server_.AddRecord(net::BuildTestHttpsServiceRecord(
        net::dns_util::GetNameForHttpsQuery(ech_host),
        /*priority=*/1, /*service_name=*/ech_host.host(),
        {net::BuildTestHttpsServiceEchConfigParam(ech_config_list)}));
    ASSERT_TRUE(doh_server_.Start());

    // Add a single bootstrapping rule so we can resolve the DoH server.
    host_resolver()->AddRule(kDohServerHostname, "127.0.0.1");

    // The net stack doesn't enable DoH when it can't find a system DNS config
    // (see https://crbug.com/1251715).
    SetReplaceSystemDnsConfig();

    // Via policy, configure the network service to use `doh_server_`.
    UpdateProviderPolicy(PolicyMapWithDohServer());
    content::FlushNetworkServiceInstanceForTesting();
  }

  PolicyMap PolicyMapWithDohServer() {
    PolicyMap policies;
    SetPolicy(&policies, key::kDnsOverHttpsMode, base::Value("secure"));
    SetPolicy(&policies, key::kDnsOverHttpsTemplates,
              base::Value(doh_server_.GetTemplate()));
    return policies;
  }

  GURL GetURL(std::string_view path) {
    return ech_server_.GetURL(kHostname, path);
  }

 private:
  static std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content_type("text/html; charset=utf-8");
    if (request.ssl_info->encrypted_client_hello) {
      response->set_content(
          base::StrCat({"<title>", kECHSuccessTitle, "</title>"}));
    } else {
      response->set_content(
          base::StrCat({"<title>", kECHFailureTitle, "</title>"}));
    }
    return response;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  net::TestDohServer doh_server_;
  net::EmbeddedTestServer ech_server_;
};

IN_PROC_BROWSER_TEST_F(ECHPolicyTest, ECHEnabledPolicy) {
  // By default, the policy does not inhibit ECH.
  EXPECT_TRUE(GetLocalStateBooleanPref(prefs::kEncryptedClientHelloEnabled));
  LoadResult result = LoadPage(GetURL("/a"));
  EXPECT_TRUE(result.success);
  EXPECT_EQ(base::ASCIIToUTF16(kECHSuccessTitle), result.title);

  // Disable the policy.
  PolicyMap policies = PolicyMapWithDohServer();
  SetPolicy(&policies, key::kEncryptedClientHelloEnabled, base::Value(false));
  UpdateProviderPolicy(policies);
  content::FlushNetworkServiceInstanceForTesting();

  // ECH should no longer be enabled.
  EXPECT_FALSE(GetLocalStateBooleanPref(prefs::kEncryptedClientHelloEnabled));
  result = LoadPage(GetURL("/b"));
  EXPECT_TRUE(result.success);
  EXPECT_EQ(base::ASCIIToUTF16(kECHFailureTitle), result.title);
}

// TLS13EarlyDataPolicyTest relies on the fact that EmbeddedTestServer
// uses HTTP/1.1 without connection reuse (unless the protocol is explicitly
// specified). If EmbeddedTestServer ever gains connection reuse by default,
// we'll need to force it off.
class TLS13EarlyDataPolicyTest : public SSLPolicyTest,
                                 public ::testing::WithParamInterface<bool> {
 public:
  static constexpr std::string_view kHostname = "a.test";
  static constexpr std::string_view kEarlyDataCheckPath = "/test-request";
  static constexpr std::string_view kEarlyDataAcceptedTitle = "accepted";
  static constexpr std::string_view kEarlyDataNotAcceptedTitle = "not accepted";

  TLS13EarlyDataPolicyTest()
      : test_server_{net::EmbeddedTestServer::TYPE_HTTPS} {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (GetParam()) {
      enabled_features.emplace_back(net::features::kHappyEyeballsV3);
    } else {
      disabled_features.emplace_back(net::features::kHappyEyeballsV3);
    }
    disabled_features.emplace_back(net::features::kEnableTLS13EarlyData);
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetUpOnMainThread() override {
    net::SSLServerConfig server_config;
    server_config.early_data_enabled = true;
    test_server_.RegisterRequestHandler(
        base::BindRepeating(&TLS13EarlyDataPolicyTest::HandleRequest));
    test_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES,
                              server_config);
    ASSERT_TRUE(test_server_.Start());

    host_resolver()->AddRule(kHostname, "127.0.0.1");
  }

  GURL GetURL(std::string_view path) {
    return test_server_.GetURL(kHostname, path);
  }

  void NavigateToTestPage() {
    ASSERT_TRUE(NavigateToUrl(GetURL("/index.html"), this));
  }

  std::string FetchResourceForEarlyDataCheck(GURL url) {
    content::EvalJsResult result =
        content::EvalJs(chrome_test_utils::GetActiveWebContents(this),
                        content::JsReplace(R"(
      fetch($1).then(res => res.text())
    )",
                                           GetURL(kEarlyDataCheckPath)));
    return result.ExtractString();
  }

 private:
  static std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->AddCustomHeader("Connection", "close");
    if (request.GetURL().GetPath() == kEarlyDataCheckPath) {
      response->set_content_type("text/plain; charset=utf-8");
      if (request.ssl_info->early_data_accepted) {
        response->set_content(kEarlyDataAcceptedTitle);
      } else {
        response->set_content(kEarlyDataNotAcceptedTitle);
      }
    } else {
      response->set_content_type("text/html; charset=utf-8");
      response->set_content("<html></html>");
    }
    return response;
  }

  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer test_server_;
};

INSTANTIATE_TEST_SUITE_P(, TLS13EarlyDataPolicyTest, ::testing::Bool());

IN_PROC_BROWSER_TEST_P(TLS13EarlyDataPolicyTest,
                       TLS13EarlyDataPolicyNoOverride) {
  EXPECT_FALSE(GetLocalStateBooleanPref(prefs::kTLS13EarlyDataEnabled));

  NavigateToTestPage();

  EXPECT_EQ(FetchResourceForEarlyDataCheck(GetURL(kEarlyDataCheckPath)),
            kEarlyDataNotAcceptedTitle);
}

// TODO(crbug.com/418717917, crbug.com/419211957): Flaky on Windows and Android.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
#define MAYBE_TLS13EarlyDataPolicyEnable DISABLED_TLS13EarlyDataPolicyEnable
#else
#define MAYBE_TLS13EarlyDataPolicyEnable TLS13EarlyDataPolicyEnable
#endif
IN_PROC_BROWSER_TEST_P(TLS13EarlyDataPolicyTest,
                       MAYBE_TLS13EarlyDataPolicyEnable) {
  PolicyMap policies;
  SetPolicy(&policies, key::kTLS13EarlyDataEnabled, base::Value(true));
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(GetLocalStateBooleanPref(prefs::kTLS13EarlyDataEnabled));
  content::FlushNetworkServiceInstanceForTesting();

  NavigateToTestPage();

  EXPECT_EQ(FetchResourceForEarlyDataCheck(GetURL(kEarlyDataCheckPath)),
            kEarlyDataAcceptedTitle);
}

IN_PROC_BROWSER_TEST_P(TLS13EarlyDataPolicyTest, TLS13EarlyDataPolicyDisable) {
  PolicyMap policies;
  SetPolicy(&policies, key::kTLS13EarlyDataEnabled, base::Value(false));
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(GetLocalStateBooleanPref(prefs::kTLS13EarlyDataEnabled));

  NavigateToTestPage();

  EXPECT_EQ(FetchResourceForEarlyDataCheck(GetURL(kEarlyDataCheckPath)),
            kEarlyDataNotAcceptedTitle);
}

}  // namespace policy
