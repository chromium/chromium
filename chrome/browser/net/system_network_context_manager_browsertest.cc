// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/system_network_context_manager.h"

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "components/component_updater/installer_policies/first_party_sets_component_installer_policy.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_info.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/user_agent.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/base/features.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/dns/mock_host_resolver.h"
#include "net/net_buildflags.h"
#include "sandbox/policy/features.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#include "sandbox/policy/linux/sandbox_seccomp_bpf_linux.h"
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

using SystemNetworkContextManagerBrowsertest = InProcessBrowserTest;

const char* kCookieName = "Cookie";
const char* kHostA = "a.test";
const char* kHostB = "b.test";

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

  base::Value::List patterns_allowed_to_use_all_schemes;
  patterns_allowed_to_use_all_schemes.Append("*.allowed.google.com");
  patterns_allowed_to_use_all_schemes.Append("*.youtube.com");
  local_state->SetList(prefs::kAllHttpAuthSchemesAllowedForOrigins,
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

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
// GSSAPI is currently incompatible with the network service sandbox
// (crbug.com/1474362). It isn't known until the browser is already started
// whether GSSAPI is desired, so if Chrome detects that GSSAPI is desired after
// the network service has already started sandboxed, the network
// service must be restarted so the sandbox can be removed.
class SystemNetworkContextManagerNetworkServiceSandboxBrowsertest
    : public SystemNetworkContextManagerBrowsertest,
      public content::ServiceProcessHost::Observer,
      public testing::WithParamInterface<bool> {
 public:
  // On both ChromeOS and Linux, a pref determines whether GSSAPI is desired in
  // the network service. This pref will determine whether the network service
  // is sandboxed, and when it changes from false to true this should trigger a
  // network service restart to remove the sandbox.
  const char* kGssapiDesiredPref =
#if BUILDFLAG(IS_CHROMEOS)
      prefs::kKerberosEnabled;
#elif BUILDFLAG(IS_LINUX)
      prefs::kReceivedHttpAuthNegotiateHeader;
#endif

  SystemNetworkContextManagerNetworkServiceSandboxBrowsertest() {
    sandbox_desired_ = GetParam();
    scoped_feature_list_.InitWithFeatureState(
        sandbox::policy::features::kNetworkServiceSandbox, sandbox_desired_);
  }

  void SetUpOnMainThread() override {
    // If the sandbox or the seccomp policy is disabled, these tests are
    // meaningless.
    if (!sandbox::policy::SandboxSeccompBPF::IsSeccompBPFDesired()) {
      GTEST_SKIP();
    }

    SystemNetworkContextManagerBrowsertest::SetUpOnMainThread();

    content::ServiceProcessHost::AddObserver(this);
    auto running_processes =
        content::ServiceProcessHost::GetRunningProcessInfo();
    for (const auto& info : running_processes) {
      if (info.IsService<network::mojom::NetworkService>()) {
        network_process_ = info.GetProcess().Duplicate();
        break;
      }
    }
  }

  void WaitForNextLaunch() {
    launch_run_loop_.emplace();
    launch_run_loop_->Run();
  }

  void ExpectNetworkService(bool seccomp_sandboxed,
                            bool allows_gssapi_library_load) {
    // The network service may have been launched but has not yet sandboxed
    // itself. So, wait for the Mojo endpoints to start accepting messages.
    mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
    content::GetNetworkService()->BindTestInterfaceForTesting(
        network_service_test.BindNewPipeAndPassReceiver());
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    // Log() is sync so this thread will wait for this call to succeed.
    network_service_test->Log(
        "Logging in network service to ensure it's ready.");

    // Now the test can check if the seccomp sandbox has been applied.
    EXPECT_EQ(seccomp_sandboxed,
              GetNetworkServiceProcess().IsSeccompSandboxed());

    bool network_service_allows_gssapi_library_load;
    ASSERT_TRUE(network_service_test->AllowsGSSAPILibraryLoad(
        &network_service_allows_gssapi_library_load));
    EXPECT_EQ(allows_gssapi_library_load,
              network_service_allows_gssapi_library_load);
  }

  base::Process GetNetworkServiceProcess() {
    CHECK(content::IsOutOfProcessNetworkService());
    return network_process_.Duplicate();
  }

 protected:
  bool sandbox_desired_;

 private:
  void OnServiceProcessLaunched(
      const content::ServiceProcessInfo& info) override {
    if (!info.IsService<network::mojom::NetworkService>()) {
      return;
    }
    network_process_ = info.GetProcess().Duplicate();
    if (launch_run_loop_) {
      launch_run_loop_->Quit();
    }
  }

  void OnServiceProcessTerminatedNormally(
      const content::ServiceProcessInfo& info) override {}

  void OnServiceProcessCrashed(
      const content::ServiceProcessInfo& info) override {}

  base::test::ScopedFeatureList scoped_feature_list_;
  base::Process network_process_;
  std::optional<base::RunLoop> launch_run_loop_;
};

IN_PROC_BROWSER_TEST_P(
    SystemNetworkContextManagerNetworkServiceSandboxBrowsertest,
    NetworkServiceRestartsUnsandboxedOnGssapiDesired) {
  PrefService* local_state = g_browser_process->local_state();

  // Ensure GSSAPI starts as "undesired".
  EXPECT_FALSE(local_state->GetBoolean(kGssapiDesiredPref));
  // Ensure the network service starts sandboxed (if desired) and cannot load
  // GSSAPI libraries.
  ExpectNetworkService(/*seccomp_sandboxed=*/sandbox_desired_,
                       /*allows_gssapi_library_load=*/false);

  // Now signal that GSSAPI is desired.
  local_state->SetBoolean(kGssapiDesiredPref, true);
  EXPECT_TRUE(local_state->GetBoolean(kGssapiDesiredPref));
  // If the network service was sandboxed it should automatically restart and
  // be unsandboxed. In any case it should now respect the pref and allow GSSAPI
  // library loads.
  if (sandbox_desired_) {
    WaitForNextLaunch();
  }
  ExpectNetworkService(/*seccomp_sandboxed=*/false,
                       /*allows_gssapi_library_load=*/true);

  // After killing the network service, it should still restart unsandboxed and
  // allow GSSAPI library loads.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&content::RestartNetworkService)));
  WaitForNextLaunch();
  ExpectNetworkService(/*seccomp_sandboxed=*/false,
                       /*allows_gssapi_library_load=*/true);
}

IN_PROC_BROWSER_TEST_P(
    SystemNetworkContextManagerNetworkServiceSandboxBrowsertest,
    PRE_NetworkServiceStartsUnsandboxedWithGssapiDesired) {
  PrefService* local_state = g_browser_process->local_state();
  // Signal that GSSAPI is desired. This should persist across browser restarts
  // like any pref.
  local_state->SetBoolean(kGssapiDesiredPref, true);
  EXPECT_TRUE(local_state->GetBoolean(kGssapiDesiredPref));
}

IN_PROC_BROWSER_TEST_P(
    SystemNetworkContextManagerNetworkServiceSandboxBrowsertest,
    NetworkServiceStartsUnsandboxedWithGssapiDesired) {
  // Ensure the network service starts sandboxed and allows GSSAPI library
  // loads.
  ExpectNetworkService(/*seccomp_sandboxed=*/false,
                       /*allows_gssapi_library_load=*/true);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SystemNetworkContextManagerNetworkServiceSandboxBrowsertest,
    testing::Bool(),
    [](const testing::TestParamInfo<bool>& info) {
      return info.param ? "NetworkSandboxDesired"
                        : "NetworkSandboxFullyDisabled";
    });

#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_LINUX)
class SystemNetworkContextManagerHttpNegotiateHeader
    : public SystemNetworkContextManagerBrowsertest {
 public:
  static constexpr char kHttpsNegotiateAuthPath[] = "/http_negotiate_auth";

  SystemNetworkContextManagerHttpNegotiateHeader()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    SystemNetworkContextManagerBrowsertest::SetUpOnMainThread();

    https_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    https_server_.RegisterRequestHandler(
        base::BindRepeating(&SystemNetworkContextManagerHttpNegotiateHeader::
                                SendBackHttpNegotiateHeader,
                            base::Unretained(this)));
    ASSERT_TRUE(https_server_.Start());
  }

  std::unique_ptr<net::test_server::HttpResponse> SendBackHttpNegotiateHeader(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != kHttpsNegotiateAuthPath) {
      return nullptr;
    }

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_UNAUTHORIZED);
    http_response->AddCustomHeader("WWW-Authenticate", "Negotiate");
    return http_response;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  net::test_server::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(SystemNetworkContextManagerHttpNegotiateHeader,
                       DISABLED_SetsPrefOnHttpNegotiateHeader) {
  PrefService* local_state = g_browser_process->local_state();

  // Ensure the pref starts false.
  EXPECT_FALSE(
      local_state->GetBoolean(prefs::kReceivedHttpAuthNegotiateHeader));

  PrefChangeRegistrar pref_change_registrar;
  pref_change_registrar.Init(local_state);

  base::RunLoop wait_for_set_pref_loop;
  pref_change_registrar.Add(prefs::kReceivedHttpAuthNegotiateHeader,
                            wait_for_set_pref_loop.QuitClosure());

  // Navigate to a URL that requests negotiate authentication.
  EXPECT_FALSE(NavigateToURL(web_contents(),
                             https_server_.GetURL(kHttpsNegotiateAuthPath)));
  wait_for_set_pref_loop.Run();

  // Ensure the pref is now true.
  EXPECT_TRUE(local_state->GetBoolean(prefs::kReceivedHttpAuthNegotiateHeader));
}
#endif  // BUILDFLAG(IS_LINUX)

namespace {
struct CookieAccess {
  content::CookieAccessDetails::Type type;
  std::string cookie_name;
  std::string cookie_value;
  net::CookieAccessResult cookie_access_result;

  friend std::ostream& operator<<(std::ostream& o, const CookieAccess& d) {
    o << (d.type == content::CookieAccessDetails::Type::kRead ? "read"
                                                              : "change");
    o << " name=" << d.cookie_name;
    o << " value=" << d.cookie_value;
    o << " access_result=";
    net::PrintTo(d.cookie_access_result, &o);
    return o;
  }

 public:
  bool operator==(const CookieAccess&) const = default;
};

class CookieTracker : public content::WebContentsObserver {
 public:
  explicit CookieTracker(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  void OnCookiesAccessed(content::NavigationHandle* navigation,
                         const content::CookieAccessDetails& details) override {
    OnCookiesAccessed(details);
  }

  void OnCookiesAccessed(content::RenderFrameHost* rfh,
                         const content::CookieAccessDetails& details) override {
    OnCookiesAccessed(details);
  }

  void WaitForCookies(size_t count) {
    waiting_for_cookies_count_ = count;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    QuitIfReady();
    run_loop.Run();
  }

  std::vector<CookieAccess>& cookie_accesses() { return cookie_accesses_; }

 private:
  void OnCookiesAccessed(const content::CookieAccessDetails& details) {
    for (const auto& cookie_with_access_result :
         details.cookie_access_result_list) {
      cookie_accesses_.emplace_back(details.type,
                                    cookie_with_access_result.cookie.Name(),
                                    cookie_with_access_result.cookie.Value(),
                                    cookie_with_access_result.access_result);
    }

    QuitIfReady();
  }

  void QuitIfReady() {
    if (quit_closure_.is_null()) {
      return;
    }
    if (cookie_accesses_.size() < waiting_for_cookies_count_) {
      return;
    }
    std::move(quit_closure_).Run();
  }

  std::vector<CookieAccess> cookie_accesses_;
  size_t waiting_for_cookies_count_ = 0;
  base::OnceClosure quit_closure_;
};

}  // namespace

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
    // Since we set kWaitForFirstPartySetsInit, all cookie-carrying network
    // requests are blocked until FPS is initialized.
    feature_list_.InitWithFeatures(
        {net::features::kWaitForFirstPartySetsInit,
         net::features::kForceThirdPartyCookieBlocking},
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
    SystemNetworkContextManagerBrowsertest::SetUpDefaultCommandLine(
        command_line);
    command_line->RemoveSwitch(switches::kDisableComponentUpdate);
  }

  net::test_server::EmbeddedTestServer* https_server() {
    return &https_server_;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
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
          {kCookieName, "=1; SameSite=None; secure; max-age=2147483647"})));
  ASSERT_THAT(content::GetCookies(browser()->profile(), host_root),
              net::CookieStringIs(
                  testing::UnorderedElementsAre(testing::Key(kCookieName))));
}

IN_PROC_BROWSER_TEST_F(
    SystemNetworkContextManagerWithFirstPartySetComponentBrowserTest,
    ReloadsFirstPartySetsAfterCrash) {
  // Network service is not running out of process, so cannot be crashed.
  if (!content::IsOutOfProcessNetworkService())
    return;

  CookieTracker cookie_tracker(web_contents());

  const GURL url_a = https_server()->GetURL(kHostA, "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_a));
  cookie_tracker.WaitForCookies(2);
  const CookieAccess expected_first_party_access{
      content::CookieAccessDetails::Type::kRead, "Cookie", "1",
      net::CookieAccessResult(
          net::CookieEffectiveSameSite::NO_RESTRICTION,
          net::CookieInclusionStatus(
              net::CookieInclusionStatus::WARN_PORT_MISMATCH),
          net::CookieAccessSemantics::NONLEGACY, true)};
  EXPECT_THAT(cookie_tracker.cookie_accesses(),
              testing::ElementsAre(
                  // a.test/title1.html
                  expected_first_party_access,
                  // a.test/favicon.ico
                  expected_first_party_access));
  cookie_tracker.cookie_accesses().clear();

  const GURL url_b_cross_site(https_server()->GetURL(
      kHostB, "/cross_site_iframe_factory.html?b.test(a.test)"));
  EXPECT_TRUE(NavigateToURL(web_contents(), url_b_cross_site));
  cookie_tracker.WaitForCookies(2);
  net::CookieInclusionStatus expected_third_party_inclusion_status;
  // If the sites are in the same Related Website Sets, we're expecting the
  // EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET exclusion reason.
  expected_third_party_inclusion_status.AddExclusionReason(
      net::CookieInclusionStatus::
          EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET);
  expected_third_party_inclusion_status.AddExclusionReason(
      net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT);
  expected_third_party_inclusion_status.AddWarningReason(
      net::CookieInclusionStatus::WARN_PORT_MISMATCH);
  const CookieAccess expected_third_party_access{
      content::CookieAccessDetails::Type::kRead, "Cookie", "1",
      net::CookieAccessResult(net::CookieEffectiveSameSite::NO_RESTRICTION,
                              expected_third_party_inclusion_status,
                              net::CookieAccessSemantics::NONLEGACY, true)};
  EXPECT_THAT(cookie_tracker.cookie_accesses(),
              testing::ElementsAre(
                  // a.test iframe under b.test
                  expected_third_party_access,
                  // a.test/tree_parser_util.js in an iframe under b.test
                  expected_third_party_access));
  cookie_tracker.cookie_accesses().clear();

  SimulateNetworkServiceCrash();

  EXPECT_TRUE(NavigateToURL(web_contents(), url_b_cross_site));
  cookie_tracker.WaitForCookies(2);
  EXPECT_THAT(cookie_tracker.cookie_accesses(),
              testing::ElementsAre(
                  // a.test iframe under b.test
                  expected_third_party_access,
                  // a.test/tree_parser_util.js in an iframe under b.test
                  expected_third_party_access));
  cookie_tracker.cookie_accesses().clear();
}

class SystemNetworkContextManagerReferrersFeatureBrowsertest
    : public SystemNetworkContextManagerBrowsertest,
      public testing::WithParamInterface<bool> {
 public:
  SystemNetworkContextManagerReferrersFeatureBrowsertest() {
    scoped_feature_list_.InitWithFeatureState(features::kNoReferrers,
                                              GetParam());
  }
  ~SystemNetworkContextManagerReferrersFeatureBrowsertest() override = default;

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
    : public SystemNetworkContextManagerBrowsertest {
 public:
  SystemNetworkContextManagerFreezeQUICUaBrowsertest() = default;
  ~SystemNetworkContextManagerFreezeQUICUaBrowsertest() override = default;

  void SetUpOnMainThread() override {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

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
      public testing::WithParamInterface<std::optional<bool>> {
 public:
  SystemNetworkContextManagerCertificateTransparencyBrowsertest() {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        GetParam());
  }
  ~SystemNetworkContextManagerCertificateTransparencyBrowsertest() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        std::nullopt);
  }
};
