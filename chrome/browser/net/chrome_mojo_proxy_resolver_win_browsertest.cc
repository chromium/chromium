// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_mojo_proxy_resolver_win.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/proxy_resolution/win/winhttp_status.h"
#include "services/proxy_resolver_win/public/mojom/proxy_resolver_win.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kTestUrl[] = "https://example.test/";

class ProxyResolverProcessObserver
    : public content::ServiceProcessHost::Observer {
 public:
  ProxyResolverProcessObserver() {
    content::ServiceProcessHost::AddObserver(this);
  }

  ProxyResolverProcessObserver(const ProxyResolverProcessObserver&) = delete;
  ProxyResolverProcessObserver& operator=(const ProxyResolverProcessObserver&) =
      delete;

  ~ProxyResolverProcessObserver() override {
    content::ServiceProcessHost::RemoveObserver(this);
  }

  bool is_service_running() const { return is_service_running_; }

  void WaitForLaunch() { launch_loop_.Run(); }
  void WaitForDeath() { death_loop_.Run(); }

 private:
  // content::ServiceProcessHost::Observer:
  void OnServiceProcessLaunched(
      const content::ServiceProcessInfo& info) override {
    if (!info.IsService<
            proxy_resolver_win::mojom::WindowsSystemProxyResolver>()) {
      return;
    }

    EXPECT_FALSE(is_service_running_);
    is_service_running_ = true;
    launch_loop_.Quit();
  }

  void OnServiceProcessTerminatedNormally(
      const content::ServiceProcessInfo& info) override {
    if (!info.IsService<
            proxy_resolver_win::mojom::WindowsSystemProxyResolver>()) {
      return;
    }

    EXPECT_TRUE(is_service_running_);
    is_service_running_ = false;
    death_loop_.Quit();
  }

 private:
  bool is_service_running_ = false;
  base::RunLoop launch_loop_;
  base::RunLoop death_loop_;
};

using ChromeMojoProxyResolverWinBrowserTest = InProcessBrowserTest;

// Ensures the proxy resolver service is started correctly and stopped when no
// resolvers are open.
IN_PROC_BROWSER_TEST_F(ChromeMojoProxyResolverWinBrowserTest,
                       ServiceLifecycle) {
  // Set up the ProxyResolverFactory.
  mojo::Remote<proxy_resolver_win::mojom::WindowsSystemProxyResolver>
      proxy_resolver_win(
          ChromeMojoProxyResolverWin::CreateWithSelfOwnedReceiverForTesting(
              base::TimeDelta()));

  ProxyResolverProcessObserver observer;

  // Attempt to resolve a proxy. This should create and start the service.
  base::RunLoop proxy_resolution_1;
  proxy_resolver_win->GetProxyForUrl(
      GURL(kTestUrl),
      base::BindLambdaForTesting(
          [&](const net::ProxyList& proxy_list,
              net::WinHttpStatus winhttp_status,
              int windows_error) { proxy_resolution_1.Quit(); }));
  observer.WaitForLaunch();

  // Resolve another proxy. No new service should be created (the listener will
  // assert if that's the case).
  base::RunLoop proxy_resolution_2;
  proxy_resolver_win->GetProxyForUrl(
      GURL(kTestUrl),
      base::BindLambdaForTesting(
          [&](const net::ProxyList& proxy_list,
              net::WinHttpStatus winhttp_status,
              int windows_error) { proxy_resolution_2.Quit(); }));
  EXPECT_TRUE(observer.is_service_running());

  // Wait for proxy resolution to complete. Once that's done, the service should
  // go away.
  proxy_resolution_1.Run();
  proxy_resolution_2.Run();
  observer.WaitForDeath();
}

// Same as above, but destroys the WindowsSystemProxyResolver, which should have
// no impact on service lifetime.
IN_PROC_BROWSER_TEST_F(ChromeMojoProxyResolverWinBrowserTest, DestroyResolver) {
  mojo::Remote<proxy_resolver_win::mojom::WindowsSystemProxyResolver>
      proxy_resolver_win(
          ChromeMojoProxyResolverWin::CreateWithSelfOwnedReceiverForTesting(
              base::TimeDelta()));

  ProxyResolverProcessObserver observer;

  // Attempt to resolve a proxy. This should create and start the service.
  proxy_resolver_win->GetProxyForUrl(
      GURL(kTestUrl),
      base::BindLambdaForTesting([&](const net::ProxyList& proxy_list,
                                     net::WinHttpStatus winhttp_status,
                                     int windows_error) {
        ADD_FAILURE() << "The GetProxyForURL callback should be dropped";
      }));
  observer.WaitForLaunch();

  // Destroy the resolver. The callback will never hit and the service should
  // eventually go away.
  proxy_resolver_win.reset();
  EXPECT_TRUE(observer.is_service_running());
  observer.WaitForDeath();
}

// Make sure the service can be started again after it's been stopped.
IN_PROC_BROWSER_TEST_F(ChromeMojoProxyResolverWinBrowserTest,
                       DestroyAndCreateService) {
  mojo::Remote<proxy_resolver_win::mojom::WindowsSystemProxyResolver>
      proxy_resolver_win(
          ChromeMojoProxyResolverWin::CreateWithSelfOwnedReceiverForTesting(
              base::TimeDelta()));

  ProxyResolverProcessObserver observer;

  // Attempt to resolve a proxy. This should create and start the service.
  base::RunLoop proxy_resolution_1;
  proxy_resolver_win->GetProxyForUrl(
      GURL(kTestUrl),
      base::BindLambdaForTesting(
          [&](const net::ProxyList& proxy_list,
              net::WinHttpStatus winhttp_status,
              int windows_error) { proxy_resolution_1.Quit(); }));
  observer.WaitForLaunch();

  // Wait for proxy resolution to complete. Once that's done, the service should
  // go away.
  proxy_resolution_1.Run();
  observer.WaitForDeath();

  ProxyResolverProcessObserver observer2;
  // Attempt to resolve another proxy. This should recreate and start the
  // service.
  base::RunLoop proxy_resolution_2;
  proxy_resolver_win->GetProxyForUrl(
      GURL(kTestUrl),
      base::BindLambdaForTesting(
          [&](const net::ProxyList& proxy_list,
              net::WinHttpStatus winhttp_status,
              int windows_error) { proxy_resolution_2.Quit(); }));
  observer2.WaitForLaunch();

  // Wait for proxy resolution to complete again. Once that's done, the service
  // should go away.
  proxy_resolution_2.Run();
  observer2.WaitForDeath();
}

}  // namespace
