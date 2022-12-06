// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/system_network_context_manager.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/first_party_sets_component_installer.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_features.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/user_agent.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/frame_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/base/features.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/dns/mock_host_resolver.h"
#include "net/net_buildflags.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"

using SystemNetworkContextManagerBrowsertest = InProcessBrowserTest;

const char* kSamePartyCookieName = "SamePartyCookie";
const char* kHostA = "a.test";

IN_PROC_BROWSER_TEST_F(SystemNetworkContextManagerBrowsertest,
                       StaticAuthParams) {
  // Test defaults.
  network::mojom::HttpAuthStaticParamsPtr static_params =
      SystemNetworkContextManager::GetHttpAuthStaticParamsForTesting();
  EXPECT_EQ("", static_params->gssapi_library_name);
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  // Test that prefs are reflected in params.

  PrefService* local_state = g_browser_process->local_state();
  const char dev_null[] = "/dev/null";
  local_state->SetString(prefs::kGSSAPILibraryName, dev_null);
  static_params =
      SystemNetworkContextManager::GetHttpAuthStaticParamsForTesting();
  EXPECT_EQ(dev_null, static_params->gssapi_library_name);
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) &&
        // !BUILDFLAG(IS_CHROMEOS)
}

IN_PROC_BROWSER_TEST_F(SystemNetworkContextManagerBrowsertest, AuthParams) {
  // Test defaults.
  network::mojom::HttpAuthDynamicParamsPtr dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_THAT(*dynamic_params->allowed_schemes,
              testing::ElementsAre("basic", "digest", "ntlm", "negotiate"));
  EXPECT_FALSE(dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_FALSE(dynamic_params->enable_negotiate_port);
  EXPECT_TRUE(dynamic_params->basic_over_http_enabled);
  EXPECT_EQ("", dynamic_params->server_allowlist);
  EXPECT_EQ("", dynamic_params->delegate_allowlist);
  EXPECT_FALSE(dynamic_params->delegate_by_kdc_policy);
  EXPECT_TRUE(dynamic_params->patterns_allowed_to_use_all_schemes.empty());

  PrefService* local_state = g_browser_process->local_state();

  local_state->SetBoolean(prefs::kDisableAuthNegotiateCnameLookup, true);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_THAT(*dynamic_params->allowed_schemes,
              testing::ElementsAre("basic", "digest", "ntlm", "negotiate"));
  EXPECT_TRUE(dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_FALSE(dynamic_params->enable_negotiate_port);
  EXPECT_TRUE(dynamic_params->basic_over_http_enabled);
  EXPECT_EQ("", dynamic_params->server_allowlist);
  EXPECT_EQ("", dynamic_params->delegate_allowlist);
  EXPECT_FALSE(dynamic_params->delegate_by_kdc_policy);
  EXPECT_TRUE(dynamic_params->patterns_allowed_to_use_all_schemes.empty());

  local_state->SetBoolean(prefs::kEnableAuthNegotiatePort, true);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_THAT(*dynamic_params->allowed_schemes,
              testing::ElementsAre("basic", "digest", "ntlm", "negotiate"));
  EXPECT_TRUE(dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_TRUE(dynamic_params->enable_negotiate_port);
  EXPECT_TRUE(dynamic_params->basic_over_http_enabled);
  EXPECT_EQ("", dynamic_params->server_allowlist);
  EXPECT_EQ("", dynamic_params->delegate_allowlist);
  EXPECT_FALSE(dynamic_params->delegate_by_kdc_policy);
  EXPECT_TRUE(dynamic_params->patterns_allowed_to_use_all_schemes.empty());

  local_state->SetBoolean(prefs::kBasicAuthOverHttpEnabled, false);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_THAT(*dynamic_params->allowed_schemes,
              testing::ElementsAre("basic", "digest", "ntlm", "negotiate"));
  EXPECT_TRUE(dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_TRUE(dynamic_params->enable_negotiate_port);
  EXPECT_FALSE(dynamic_params->basic_over_http_enabled);
  EXPECT_EQ("", dynamic_params->server_allowlist);
  EXPECT_EQ("", dynamic_params->delegate_allowlist);
  EXPECT_FALSE(dynamic_params->delegate_by_kdc_policy);
  EXPECT_TRUE(dynamic_params->patterns_allowed_to_use_all_schemes.empty());

  const char kServerAllowList[] = "foo";
  local_state->SetString(prefs::kAuthServerAllowlist, kServerAllowList);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_THAT(*dynamic_params->allowed_schemes,
              testing::ElementsAre("basic", "digest", "ntlm", "negotiate"));
  EXPECT_TRUE(dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_TRUE(dynamic_params->enable_negotiate_port);
  EXPECT_FALSE(dynamic_params->basic_over_http_enabled);
  EXPECT_EQ(kServerAllowList, dynamic_params->server_allowlist);
  EXPECT_EQ("", dynamic_params->delegate_allowlist);
  EXPECT_TRUE(dynamic_params->patterns_allowed_to_use_all_schemes.empty());

  const char kDelegateAllowList[] = "bar, baz";
  local_state->SetString(prefs::kAuthNegotiateDelegateAllowlist,
                         kDelegateAllowList);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_THAT(*dynamic_params->allowed_schemes,
              testing::ElementsAre("basic", "digest", "ntlm", "negotiate"));
  EXPECT_TRUE(dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_TRUE(dynamic_params->enable_negotiate_port);
  EXPECT_EQ(kServerAllowList, dynamic_params->server_allowlist);
  EXPECT_FALSE(dynamic_params->basic_over_http_enabled);
  EXPECT_EQ(kDelegateAllowList, dynamic_params->delegate_allowlist);
  EXPECT_FALSE(dynamic_params->delegate_by_kdc_policy);
  EXPECT_TRUE(dynamic_params->patterns_allowed_to_use_all_schemes.empty());

  local_state->SetString(prefs::kAuthSchemes, "basic");
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_THAT(*dynamic_params->allowed_schemes, testing::ElementsAre("basic"));
  EXPECT_TRUE(dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_TRUE(dynamic_params->enable_negotiate_port);
  EXPECT_FALSE(dynamic_params->basic_over_http_enabled);
  EXPECT_EQ(kServerAllowList, dynamic_params->server_allowlist);
  EXPECT_EQ(kDelegateAllowList, dynamic_params->delegate_allowlist);
  EXPECT_FALSE(dynamic_params->delegate_by_kdc_policy);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  local_state->SetString(prefs::kAuthSchemes, "basic");
  local_state->SetBoolean(prefs::kAuthNegotiateDelegateByKdcPolicy, true);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_THAT(*dynamic_params->allowed_schemes, testing::ElementsAre("basic"));
  EXPECT_TRUE(dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_TRUE(dynamic_params->enable_negotiate_port);
  EXPECT_FALSE(dynamic_params->basic_over_http_enabled);
  EXPECT_EQ(kServerAllowList, dynamic_params->server_allowlist);
  EXPECT_EQ(kDelegateAllowList, dynamic_params->delegate_allowlist);
  EXPECT_TRUE(dynamic_params->delegate_by_kdc_policy);
  EXPECT_TRUE(dynamic_params->patterns_allowed_to_use_all_schemes.empty());
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
  // The kerberos.enabled pref is false and the device is not Active Directory
  // managed by default.
  EXPECT_FALSE(dynamic_params->allow_gssapi_library_load);
  local_state->SetBoolean(prefs::kKerberosEnabled, true);
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();
  EXPECT_TRUE(dynamic_params->allow_gssapi_library_load);
  EXPECT_TRUE(dynamic_params->patterns_allowed_to_use_all_schemes.empty());
#endif  // BUILDFLAG(IS_CHROMEOS)

  base::Value patterns_allowed_to_use_all_schemes(base::Value::Type::LIST);
  patterns_allowed_to_use_all_schemes.Append(
      base::Value("*.allowed.google.com"));
  patterns_allowed_to_use_all_schemes.Append(base::Value("*.youtube.com"));
  local_state->Set(prefs::kAllHttpAuthSchemesAllowedForOrigins,
                   std::move(patterns_allowed_to_use_all_schemes));
  dynamic_params =
      SystemNetworkContextManager::GetHttpAuthDynamicParamsForTesting();

  EXPECT_TRUE(dynamic_params->negotiate_disable_cname_lookup);
  EXPECT_TRUE(dynamic_params->enable_negotiate_port);
  EXPECT_EQ(kServerAllowList, dynamic_params->server_allowlist);
  EXPECT_FALSE(dynamic_params->basic_over_http_enabled);
  EXPECT_EQ(kDelegateAllowList, dynamic_params->delegate_allowlist);
  EXPECT_EQ((std::vector<std::string>{"*.allowed.google.com", "*.youtube.com"}),
            dynamic_params->patterns_allowed_to_use_all_schemes);
}

class SystemNetworkContextManagerWithCustomProxyConfigBrowserTest
    : public SystemNetworkContextManagerBrowsertest {
 protected:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kIPAnonymizationProxyServer,
                                    "testproxy:80");
    command_line->AppendSwitchASCII(
        switches::kIPAnonymizationProxyAllowList,
        "a.test, foo.a.test, foo.test, b.test:1234");
    command_line->AppendSwitchASCII(switches::kIPAnonymizationProxyPassword,
                                    "value");
  }
};

IN_PROC_BROWSER_TEST_F(
    SystemNetworkContextManagerWithCustomProxyConfigBrowserTest,
    InitialCustomProxyConfig) {
  network::mojom::NetworkContextParamsPtr network_context_params =
      g_browser_process->system_network_context_manager()
          ->CreateDefaultNetworkContextParams();

  // Check that command line switches were correctly set in
  // `initial_custom_proxy_config`
  EXPECT_TRUE(network_context_params->initial_custom_proxy_config->rules
                  .reverse_bypass);
  EXPECT_TRUE(network_context_params->initial_custom_proxy_config
                  ->should_replace_direct);
  EXPECT_FALSE(network_context_params->initial_custom_proxy_config
                   ->should_override_existing_config);

  EXPECT_EQ(network_context_params->initial_custom_proxy_config->rules
                .single_proxies.ToValue(),
            base::test::ParseJson(R"(["testproxy:80"])"));
  EXPECT_EQ(network_context_params->initial_custom_proxy_config->rules
                .bypass_rules.ToString(),
            "a.test;foo.a.test;foo.test;b.test:1234;");

  net::HttpRequestHeaders expected_header;
  expected_header.SetHeader("password", "value");
  EXPECT_EQ(network_context_params->initial_custom_proxy_config
                ->connect_tunnel_headers.ToString(),
            expected_header.ToString());

  // Check that rules are applied correctly
  net::ProxyInfo result;
  network_context_params->initial_custom_proxy_config->rules.Apply(
      GURL("http://example.test"), &result);
  EXPECT_TRUE(result.did_bypass_proxy());
  EXPECT_EQ(result.proxy_server(), net::ProxyServer::Direct());

  network_context_params->initial_custom_proxy_config->rules.Apply(
      GURL("http://foo.test"), &result);
  EXPECT_FALSE(result.did_bypass_proxy());
  EXPECT_EQ(result.ToPacString(), "PROXY testproxy:80");

  network_context_params->initial_custom_proxy_config->rules.Apply(
      GURL("http://a.test"), &result);
  EXPECT_FALSE(result.did_bypass_proxy());
  EXPECT_EQ(result.ToPacString(), "PROXY testproxy:80");

  network_context_params->initial_custom_proxy_config->rules.Apply(
      GURL("https://a.test"), &result);
  EXPECT_FALSE(result.did_bypass_proxy());
  EXPECT_EQ(result.ToPacString(), "PROXY testproxy:80");

  network_context_params->initial_custom_proxy_config->rules.Apply(
      GURL("https://foo.a.test"), &result);
  EXPECT_FALSE(result.did_bypass_proxy());
  EXPECT_EQ(result.ToPacString(), "PROXY testproxy:80");

  network_context_params->initial_custom_proxy_config->rules.Apply(
      GURL("https://bar.a.test"), &result);
  EXPECT_TRUE(result.did_bypass_proxy());
  EXPECT_EQ(result.proxy_server(), net::ProxyServer::Direct());

  network_context_params->initial_custom_proxy_config->rules.Apply(
      GURL("https://b.test:1234"), &result);
  EXPECT_FALSE(result.did_bypass_proxy());
  EXPECT_EQ(result.ToPacString(), "PROXY testproxy:80");

  network_context_params->initial_custom_proxy_config->rules.Apply(
      GURL("https://b.test:5678"), &result);
  EXPECT_TRUE(result.did_bypass_proxy());
  EXPECT_EQ(result.proxy_server(), net::ProxyServer::Direct());
}

class SystemNetworkContextManagerWithFirstPartySetComponentBrowserTest
    : public SystemNetworkContextManagerBrowsertest {
 public:
  SystemNetworkContextManagerWithFirstPartySetComponentBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    SystemNetworkContextManagerBrowsertest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    ASSERT_TRUE(https_server()->Start());
  }

  void SetUpInProcessBrowserTestFixture() override {
    SystemNetworkContextManagerBrowsertest::SetUpInProcessBrowserTestFixture();
    feature_list_.InitWithFeatures(
        {features::kFirstPartySets, net::features::kSamePartyAttributeEnabled},
        {});
    CHECK(component_dir_.CreateUniqueTempDir());
    base::ScopedAllowBlockingForTesting allow_blocking;

    component_updater::FirstPartySetsComponentInstallerPolicy::
        WriteComponentForTesting(base::Version("1.2.3"),
                                 component_dir_.GetPath(),
                                 GetComponentContents());
  }

 protected:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    base::CommandLine default_command_line(base::CommandLine::NO_PROGRAM);
    SystemNetworkContextManagerBrowsertest::SetUpDefaultCommandLine(
        &default_command_line);
    test_launcher_utils::RemoveCommandLineSwitch(
        default_command_line, switches::kDisableComponentUpdate, command_line);
  }

  net::test_server::EmbeddedTestServer* https_server() {
    return &https_server_;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  GURL EchoCookiesUrl(const std::string& host) {
    return https_server_.GetURL(host, "/echoheader?Cookie");
  }

 private:
  std::string GetComponentContents() const {
    return "{\"primary\": \"https://a.test\", \"associatedSites\": [ "
           "\"https://b.test\", \"https://associatedsite1.test\"]}\n"
           "{\"primary\": \"https://c.test\", \"associatedSites\": [ "
           "\"https://d.test\", \"https://associatedsite2.test\"]}";
  }

  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir component_dir_;
  net::test_server::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(
    SystemNetworkContextManagerWithFirstPartySetComponentBrowserTest,
    PRE_ReloadsFirstPartySetsAfterCrash) {
  // Network service is not running out of process, so cannot be crashed.
  if (!content::IsOutOfProcessNetworkService())
    return;

  // Set a persistent cookie that will still be there after the network service
  // is crashed. We don't use the system network context here (which wouldn't
  // persist the cookie to disk), but that's ok - this test only cares that the
  // NetworkService gets reconfigured after a crash, and that that
  // reconfiguration includes setting up First-Party Sets.
  const GURL host_root = https_server()->GetURL(kHostA, "/");
  ASSERT_TRUE(content::SetCookie(
      browser()->profile(), host_root,
      base::StrCat(
          {kSamePartyCookieName,
           "=1; samesite=lax; secure; sameparty; max-age=2147483647"})));
  ASSERT_THAT(content::GetCookies(browser()->profile(), host_root),
              net::CookieStringIs(testing::UnorderedElementsAre(
                  testing::Key(kSamePartyCookieName))));
}

IN_PROC_BROWSER_TEST_F(
    SystemNetworkContextManagerWithFirstPartySetComponentBrowserTest,
    ReloadsFirstPartySetsAfterCrash) {
  // Network service is not running out of process, so cannot be crashed.
  if (!content::IsOutOfProcessNetworkService())
    return;

  const GURL host_root = https_server()->GetURL(kHostA, "/");
  ASSERT_THAT(content::GetCookies(browser()->profile(), host_root),
              net::CookieStringIs(testing::UnorderedElementsAre(
                  testing::Key(kSamePartyCookieName))));

  EXPECT_THAT(content::ArrangeFramesAndGetContentFromLeaf(
                  web_contents(), https_server(), "b.test(%s)", {0},
                  EchoCookiesUrl(kHostA)),
              net::CookieStringIs(testing::UnorderedElementsAre(
                  testing::Key(kSamePartyCookieName))));

  SimulateNetworkServiceCrash();

  EXPECT_THAT(content::ArrangeFramesAndGetContentFromLeaf(
                  web_contents(), https_server(), "b.test(%s)", {0},
                  EchoCookiesUrl(kHostA)),
              net::CookieStringIs(testing::UnorderedElementsAre(
                  testing::Key(kSamePartyCookieName))));
}

class SystemNetworkContextManagerReferrersFeatureBrowsertest
    : public SystemNetworkContextManagerBrowsertest,
      public testing::WithParamInterface<bool> {
 public:
  SystemNetworkContextManagerReferrersFeatureBrowsertest() {
    scoped_feature_list_.InitWithFeatureState(features::kNoReferrers,
                                              GetParam());
  }
  ~SystemNetworkContextManagerReferrersFeatureBrowsertest() override {}

  void SetUpOnMainThread() override {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that toggling the kNoReferrers feature correctly changes the default
// value of the kEnableReferrers pref.
IN_PROC_BROWSER_TEST_P(SystemNetworkContextManagerReferrersFeatureBrowsertest,
                       TestDefaultReferrerReflectsFeatureValue) {
  ASSERT_TRUE(g_browser_process);
  PrefService* local_state = g_browser_process->local_state();
  ASSERT_TRUE(local_state);
  EXPECT_NE(local_state->GetBoolean(prefs::kEnableReferrers), GetParam());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SystemNetworkContextManagerReferrersFeatureBrowsertest,
                         ::testing::Bool());

class SystemNetworkContextManagerFreezeQUICUaBrowsertest
    : public SystemNetworkContextManagerBrowsertest,
      public testing::WithParamInterface<bool> {
 public:
  SystemNetworkContextManagerFreezeQUICUaBrowsertest() {
    scoped_feature_list_.InitWithFeatureState(blink::features::kReduceUserAgent,
                                              GetParam());
  }
  ~SystemNetworkContextManagerFreezeQUICUaBrowsertest() override {}

  void SetUpOnMainThread() override {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(SystemNetworkContextManagerFreezeQUICUaBrowsertest,
                       QUICUaConfig) {
  network::mojom::NetworkContextParamsPtr network_context_params =
      g_browser_process->system_network_context_manager()
          ->CreateDefaultNetworkContextParams();

  std::string quic_ua = network_context_params->quic_user_agent_id;

  if (GetParam()) {  // if the UA Freeze feature is turned on
    EXPECT_EQ("", quic_ua);
  } else {
    EXPECT_THAT(quic_ua, testing::HasSubstr(chrome::GetChannelName(
                             chrome::WithExtendedStable(false))));
    EXPECT_THAT(quic_ua,
                testing::HasSubstr(
                    version_info::GetProductNameAndVersionForUserAgent()));
    EXPECT_THAT(quic_ua, testing::HasSubstr(content::BuildOSCpuInfo(
                             content::IncludeAndroidBuildNumber::Exclude,
                             content::IncludeAndroidModel::Include)));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         SystemNetworkContextManagerFreezeQUICUaBrowsertest,
                         ::testing::Bool());

class SystemNetworkContextManagerWPADQuickCheckBrowsertest
    : public SystemNetworkContextManagerBrowsertest,
      public testing::WithParamInterface<bool> {
 public:
  SystemNetworkContextManagerWPADQuickCheckBrowsertest() = default;
  ~SystemNetworkContextManagerWPADQuickCheckBrowsertest() override = default;
};

IN_PROC_BROWSER_TEST_P(SystemNetworkContextManagerWPADQuickCheckBrowsertest,
                       WPADQuickCheckPref) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(prefs::kQuickCheckEnabled, GetParam());

  network::mojom::NetworkContextParamsPtr network_context_params =
      g_browser_process->system_network_context_manager()
          ->CreateDefaultNetworkContextParams();
  EXPECT_EQ(GetParam(), network_context_params->pac_quick_check_enabled);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SystemNetworkContextManagerWPADQuickCheckBrowsertest,
                         ::testing::Bool());

class SystemNetworkContextManagerCertificateTransparencyBrowsertest
    : public SystemNetworkContextManagerBrowsertest,
      public testing::WithParamInterface<absl::optional<bool>> {
 public:
  SystemNetworkContextManagerCertificateTransparencyBrowsertest() {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        GetParam());
  }
  ~SystemNetworkContextManagerCertificateTransparencyBrowsertest() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        absl::nullopt);
  }
};
