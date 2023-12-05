// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/dbus/proxy_resolution_service_provider.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/net/system_proxy_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

constexpr char kLocalProxyUrl[] = "localhost:3128";

// Encode the PAC script as a data: URL.
std::string GetPacUrl(const char* pac_data) {
  std::string b64_encoded = base::Base64Encode(pac_data);
  return "data:application/x-javascript-config;base64," + b64_encoded;
}
}  // namespace

// Helper for calling ProxyResolutionServiceProvider's |ResolveProxyInternal()|
// method. Unlike the unit-tests which mock the network setup, this uses the
// default dependencies from the running browser.
class ProxyResolutionServiceProviderTestWrapper {
 public:
  ProxyResolutionServiceProviderTestWrapper() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  ProxyResolutionServiceProviderTestWrapper(
      const ProxyResolutionServiceProviderTestWrapper&) = delete;
  ProxyResolutionServiceProviderTestWrapper& operator=(
      const ProxyResolutionServiceProviderTestWrapper&) = delete;

  ~ProxyResolutionServiceProviderTestWrapper() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  // Calls ResolveProxyInternal() and returns its result synchronously as a
  // single string (which may be prefixed by "ERROR: " if it is an error message
  // as opposed to a proxy result).
  std::string ResolveProxyAndWait(const std::string& url) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    base::RunLoop run_loop;

    std::string result;
    impl_.ResolveProxyInternal(
        url,
        base::BindOnce(
            &ProxyResolutionServiceProviderTestWrapper::OnResolveProxyComplete,
            &result, run_loop.QuitClosure()),
        chromeos::SystemProxyOverride::kDefault);

    run_loop.Run();

    return result;
  }

 private:
  static void OnResolveProxyComplete(std::string* result,
                                     base::RepeatingClosure quit_closure,
                                     const std::string& error,
                                     const std::string& pac_string) {
    if (!error.empty()) {
      *result = "ERROR: " + error;
    } else {
      *result = pac_string;
    }

    std::move(quit_closure).Run();
  }

  ProxyResolutionServiceProvider impl_;
};

// Base test fixture that exposes a way to invoke ProxyResolutionServiceProvider
// synchronously from the UI thread.
class ProxyResolutionServiceProviderBaseBrowserTest
    : public MixinBasedInProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    proxy_service_ =
        std::make_unique<ProxyResolutionServiceProviderTestWrapper>();
  }

  void TearDownOnMainThread() override {
    proxy_service_.reset();
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  std::string ResolveProxyAndWait(const std::string& source_url) {
    return proxy_service_->ResolveProxyAndWait(source_url);
  }

 private:
  std::unique_ptr<ProxyResolutionServiceProviderTestWrapper> proxy_service_;
};

// Fixture that launches the browser with --proxy-server="https://proxy.test".
class ProxyResolutionServiceProviderManualProxyBrowserTest
    : public ProxyResolutionServiceProviderBaseBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kProxyServer,
                                    "https://proxy.test");
  }
};

// Tests that the D-Bus proxy resolver returns the correct result when using
// --proxy-server flag. These resolutions will happen synchronously at the //net
// layer.
IN_PROC_BROWSER_TEST_F(ProxyResolutionServiceProviderManualProxyBrowserTest,
                       ResolveProxy) {
  EXPECT_EQ("HTTPS proxy.test:443",
            ResolveProxyAndWait("http://www.google.com"));
}

// Simple PAC script that returns the same two proxies for all requests.
const char kPacData[] =
    "function FindProxyForURL(url, host) {\n"
    "  return 'PROXY foo1; PROXY foo2';\n"
    "}\n";

// Fixture that launches the browser with --proxy-pac-url="data:...".
class ProxyResolutionServiceProviderPacBrowserTest
    : public ProxyResolutionServiceProviderBaseBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kProxyPacUrl,
                                    GetPacUrl(kPacData));
  }
};

// Tests that the D-Bus proxy resolver returns the correct result when using
// --proxy-pac-url flag. These resolutions will happen asynchronously at the
// //net layer, as they need to query a PAC script.
IN_PROC_BROWSER_TEST_F(ProxyResolutionServiceProviderPacBrowserTest,
                       ResolveProxy) {
  EXPECT_EQ("PROXY foo1:80;PROXY foo2:80",
            ResolveProxyAndWait("http://www.google.com"));
}

// PAC script that returns a proxy for all url except for a whitelisted domain.
const char kPacDataWithWhitelistedDomain[] =
    "function FindProxyForURL(url, host) {\n"
    "  if (dnsDomainIs(host, '.direct.com'))\n"
    "    return 'DIRECT';\n"
    "  return 'PROXY foo1';\n"
    "}\n";

// Fixture that launches the browser with --proxy-pac-url="data:..." and
// System-proxy enabled. With System-proxy enabled and configured, all system
// service connections going trough an http web proxy will be connected through
// a local proxy that will perform the proxy authentication and connection
// setup.
class ProxyResolutionServiceProviderSystemProxyPolicyTest
    : public ProxyResolutionServiceProviderBaseBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kProxyPacUrl,
                                    GetPacUrl(kPacDataWithWhitelistedDomain));
  }

 protected:
  void SetLocalProxyAddress(const std::string& local_proxy_url) {
    SystemProxyManager::Get()->SetSystemProxyEnabledForTest(true);
    SystemProxyManager::Get()->SetSystemServicesProxyUrlForTest(
        local_proxy_url);
  }
};

// Tests that the proxy resolver returns the address of the local proxy when
// set.
IN_PROC_BROWSER_TEST_F(ProxyResolutionServiceProviderSystemProxyPolicyTest,
                       ResolveProxyLocalProxySet) {
  SetLocalProxyAddress(kLocalProxyUrl);
  EXPECT_EQ("PROXY localhost:3128; PROXY foo1:80",
            ResolveProxyAndWait("http://www.google.com"));
}

// Tests that the proxy list semicolon separator is not appended if the local
// proxy is not set.
IN_PROC_BROWSER_TEST_F(ProxyResolutionServiceProviderSystemProxyPolicyTest,
                       ResolveProxyNoSeparator) {
  SetLocalProxyAddress(/* local_proxy_url= */ std::string());
  EXPECT_EQ("PROXY foo1:80", ResolveProxyAndWait("http://www.google.com"));
}

// Tests that the proxy resolver doesn't return the local proxy address for
// DIRECT connections.
IN_PROC_BROWSER_TEST_F(ProxyResolutionServiceProviderSystemProxyPolicyTest,
                       ResolveProxyDirect) {
  SetLocalProxyAddress(kLocalProxyUrl);
  EXPECT_EQ("DIRECT", ResolveProxyAndWait("http://www.test.direct.com"));
}

class ProxyResolutionServiceAtLoginScreen
    : public ProxyResolutionServiceProviderBaseBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kProxyPacUrl,
                                    GetPacUrl(kPacData));
  }

 protected:
  LoginManagerMixin login_manager_{&mixin_host_};
};

// Tests that the D-Bus proxy resolver returns the proxy configured at the
// sign-in screen.
IN_PROC_BROWSER_TEST_F(ProxyResolutionServiceAtLoginScreen, ResolveProxy) {
  EXPECT_EQ("PROXY foo1:80;PROXY foo2:80",
            ResolveProxyAndWait("http://www.google.com"));
}

}  // namespace ash
