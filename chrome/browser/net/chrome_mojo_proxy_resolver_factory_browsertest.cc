// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_mojo_proxy_resolver_factory.h"

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"

namespace {

constexpr base::TimeDelta kServiceShutdownTimeout = base::Seconds(3);

constexpr char kPacScript[] =
    "function FindProxyForURL(url, host) { return 'PROXY proxy.example.com:1; "
    "DIRECT'; }";

// Dummy consumer of a ProxyResolverFactory. It just calls CreateResolver, and
// keeps Mojo objects alive from when CreateResolver() is called until it's
// destroyed.
class DumbProxyResolverFactoryRequestClient
    : public proxy_resolver::mojom::ProxyResolverFactoryRequestClient {
 public:
  DumbProxyResolverFactoryRequestClient() = default;

  ~DumbProxyResolverFactoryRequestClient() override {
    EXPECT_TRUE(receiver_.is_bound());
  }

  void CreateResolver(
      proxy_resolver::mojom::ProxyResolverFactory* resolver_factory) {
    mojo::PendingRemote<
        proxy_resolver::mojom::ProxyResolverFactoryRequestClient>
        resolver_client;
    receiver_.Bind(resolver_client.InitWithNewPipeAndPassReceiver());
    resolver_factory->CreateResolver(kPacScript,
                                     resolver_.BindNewPipeAndPassReceiver(),
                                     std::move(resolver_client));
    // Wait for proxy resolver to be created, to avoid any races between
    // creating one resolver and destroying the next one.
    run_loop_.Run();
  }

 private:
  // ProxyResolverFactoryRequestClient implementation:
  void ReportResult(int32_t error) override {
    EXPECT_EQ(net::OK, error);
    run_loop_.Quit();
  }
  void Alert(const std::string& error) override {}
  void OnError(int32_t line_number, const std::string& error) override {}
  void ResolveDns(
      const std::string& hostname,
      net::ProxyResolveDnsOperation operation,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
          client) override {}

  mojo::Remote<proxy_resolver::mojom::ProxyResolver> resolver_;
  mojo::Receiver<proxy_resolver::mojom::ProxyResolverFactoryRequestClient>
      receiver_{this};
  base::RunLoop run_loop_;
};

using ChromeMojoProxyResolverFactoryBrowserTest = InProcessBrowserTest;

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
    if (!info.IsService<proxy_resolver::mojom::ProxyResolverFactory>())
      return;

    ASSERT_FALSE(is_service_running_);
    is_service_running_ = true;
    launch_loop_.Quit();
  }

  void OnServiceProcessTerminatedNormally(
      const content::ServiceProcessInfo& info) override {
    if (!info.IsService<proxy_resolver::mojom::ProxyResolverFactory>())
      return;

    ASSERT_TRUE(is_service_running_);
    is_service_running_ = false;
    death_loop_.Quit();
  }

 private:
  bool is_service_running_ = false;
  base::RunLoop launch_loop_;
  base::RunLoop death_loop_;
};

// Ensures the proxy resolver service is started correctly and stopped when no
// resolvers are open.
IN_PROC_BROWSER_TEST_F(ChromeMojoProxyResolverFactoryBrowserTest,
                       ServiceLifecycle) {
  // Set up the ProxyResolverFactory.
  mojo::Remote<proxy_resolver::mojom::ProxyResolverFactory> resolver_factory(
      ChromeMojoProxyResolverFactory::CreateWithSelfOwnedReceiver());

  ProxyResolverProcessObserver observer;

  // Create a resolver, this should create and start the service.
  std::unique_ptr<DumbProxyResolverFactoryRequestClient> resolver_client1 =
      std::make_unique<DumbProxyResolverFactoryRequestClient>();
  resolver_client1->CreateResolver(resolver_factory.get());
  observer.WaitForLaunch();

  // Create another resolver, no new service should be created (the listener
  // will assert if that's the case).
  std::unique_ptr<DumbProxyResolverFactoryRequestClient> resolver_client2 =
      std::make_unique<DumbProxyResolverFactoryRequestClient>();
  resolver_client2->CreateResolver(resolver_factory.get());

  // Close one resolver.
  resolver_client1.reset();

  // The service should not be stopped as there is another resolver.
  // Wait a little bit and check it's still running.
  {
    base::RunLoop run_loop;
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), kServiceShutdownTimeout);
    run_loop.Run();
  }

  EXPECT_TRUE(observer.is_service_running());

  // Close the last resolver, the service should now go away.
  resolver_client2.reset();
  observer.WaitForDeath();
}

// Same as above, but destroys the ProxyResolverFactory, which should have no
// impact on resolver lifetime.
IN_PROC_BROWSER_TEST_F(ChromeMojoProxyResolverFactoryBrowserTest,
                       DestroyFactory) {
  // Set up the ProxyResolverFactory.
  mojo::Remote<proxy_resolver::mojom::ProxyResolverFactory> resolver_factory(
      ChromeMojoProxyResolverFactory::CreateWithSelfOwnedReceiver());

  ProxyResolverProcessObserver observer;

  // Create a resolver, this should create and start the service.
  std::unique_ptr<DumbProxyResolverFactoryRequestClient> resolver_client1 =
      std::make_unique<DumbProxyResolverFactoryRequestClient>();
  resolver_client1->CreateResolver(resolver_factory.get());
  observer.WaitForLaunch();

  // Create another resolver, no new service should be created (the listener
  // will assert if that's the case).
  std::unique_ptr<DumbProxyResolverFactoryRequestClient> resolver_client2 =
      std::make_unique<DumbProxyResolverFactoryRequestClient>();
  resolver_client2->CreateResolver(resolver_factory.get());

  // Destroy the factory. Should not result in the resolver being destroyed.
  resolver_factory.reset();

  // Close one resolver.
  resolver_client1.reset();

  // The service should not be stopped as there is another resolver.
  // Wait a little bit and check it's still running.
  {
    base::RunLoop run_loop;
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), kServiceShutdownTimeout);
    run_loop.Run();
  }

  EXPECT_TRUE(observer.is_service_running());

  // Close the last resolver, the service should now go away.
  resolver_client2.reset();
  observer.WaitForDeath();
}

// Make sure the service can be started again after it's been stopped.
IN_PROC_BROWSER_TEST_F(ChromeMojoProxyResolverFactoryBrowserTest,
                       DestroyAndCreateService) {
  // Set up the ProxyResolverFactory.
  mojo::Remote<proxy_resolver::mojom::ProxyResolverFactory> resolver_factory(
      ChromeMojoProxyResolverFactory::CreateWithSelfOwnedReceiver());

  std::optional<ProxyResolverProcessObserver> observer{std::in_place};

  // Create a resolver, this should create and start the service.
  std::unique_ptr<DumbProxyResolverFactoryRequestClient> resolver_client =
      std::make_unique<DumbProxyResolverFactoryRequestClient>();
  resolver_client->CreateResolver(resolver_factory.get());
  observer->WaitForLaunch();

  // Close the resolver, the service should stop.
  resolver_client.reset();
  observer->WaitForDeath();

  observer.emplace();
  // Create a resolver again, using the same factory. This should create and
  // start the service.
  resolver_client = std::make_unique<DumbProxyResolverFactoryRequestClient>();
  resolver_client->CreateResolver(resolver_factory.get());
  observer->WaitForLaunch();

  // Close the resolver again, the service should stop.
  resolver_client.reset();
  observer->WaitForDeath();
}

}  // namespace
