// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_mojo_proxy_resolver_factory.h"

#include <stdint.h>

#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"

namespace {

constexpr base::TimeDelta kServiceShutdownTimeout =
    base::TimeDelta::FromSeconds(3);

constexpr char kPacScript[] =
    "function FindProxyForURL(url, host) { return 'PROXY proxy.example.com:1; "
    "DIRECT'; }";

// An implementation of ServiceManagerListener that tracks creation/destruction
// of the proxy resolver service.
class TestServiceManagerListener
    : public service_manager::mojom::ServiceManagerListener {
 public:
  explicit TestServiceManagerListener(
      service_manager::mojom::ServiceManager* service_manager)
      : binding_(this) {
    service_manager::mojom::ServiceManagerListenerPtr listener_ptr;
    binding_.Bind(mojo::MakeRequest(&listener_ptr));
    service_manager->AddListener(std::move(listener_ptr));
  }

  bool service_running() const { return service_running_; }

  void WaitUntilServiceStarted() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(!on_service_event_loop_closure_);
    if (service_running_)
      return;
    base::RunLoop run_loop;
    on_service_event_loop_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    on_service_event_loop_closure_.Reset();
  }

  void WaitUntilServiceStopped() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(!on_service_event_loop_closure_);
    if (!service_running_)
      return;
    base::RunLoop run_loop;
    on_service_event_loop_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    on_service_event_loop_closure_.Reset();
  }

 private:
  // service_manager::mojom::ServiceManagerListener implementation:

  void OnInit(std::vector<service_manager::mojom::RunningServiceInfoPtr>
                  running_services) override {}

  void OnServiceCreated(
      service_manager::mojom::RunningServiceInfoPtr service) override {}

  void OnServiceStarted(const service_manager::Identity& identity,
                        uint32_t pid) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (identity.name() != proxy_resolver::mojom::kProxyResolverServiceName)
      return;

    EXPECT_FALSE(service_running_);
    service_running_ = true;
    if (on_service_event_loop_closure_)
      std::move(on_service_event_loop_closure_).Run();
  }

  void OnServicePIDReceived(const service_manager::Identity& identity,
                            uint32_t pid) override {}
  void OnServiceFailedToStart(
      const service_manager::Identity& identity) override {}

  void OnServiceStopped(const service_manager::Identity& identity) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (identity.name() != proxy_resolver::mojom::kProxyResolverServiceName)
      return;

    EXPECT_TRUE(service_running_);
    service_running_ = false;
    if (on_service_event_loop_closure_)
      std::move(on_service_event_loop_closure_).Run();
  }

  mojo::Binding<service_manager::mojom::ServiceManagerListener> binding_;

  bool service_running_ = false;
  base::Closure on_service_event_loop_closure_;

  DISALLOW_COPY_AND_ASSIGN(TestServiceManagerListener);
};

// Dummy consumer of a ProxyResolverFactory. It just calls CreateResolver, and
// keeps Mojo objects alive from when CreateResolver() is called until it's
// destroyed.
class DumbProxyResolverFactoryRequestClient
    : public proxy_resolver::mojom::ProxyResolverFactoryRequestClient {
 public:
  DumbProxyResolverFactoryRequestClient() : binding_(this) {}

  ~DumbProxyResolverFactoryRequestClient() override {
    EXPECT_TRUE(binding_.is_bound());
  }

  void CreateResolver(
      proxy_resolver::mojom::ProxyResolverFactory* resolver_factory) {
    proxy_resolver::mojom::ProxyResolverFactoryRequestClientPtr resolver_client;
    binding_.Bind(mojo::MakeRequest(&resolver_client));
    resolver_factory->CreateResolver(kPacScript, mojo::MakeRequest(&resolver_),
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
      std::unique_ptr<net::HostResolver::RequestInfo> request_info,
      ::net::interfaces::HostResolverRequestClientPtr client) override {}

  proxy_resolver::mojom::ProxyResolverPtr resolver_;
  mojo::Binding<proxy_resolver::mojom::ProxyResolverFactoryRequestClient>
      binding_;
  base::RunLoop run_loop_;
};

class ChromeMojoProxyResolverFactoryBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    // Access the service manager so a listener for service creation/destruction
    // can be set-up.
    content::ServiceManagerConnection::GetForProcess()
        ->GetConnector()
        ->BindInterface(service_manager::mojom::kServiceName,
                        &service_manager_);

    listener_ =
        std::make_unique<TestServiceManagerListener>(service_manager_.get());
  }

  TestServiceManagerListener* listener() const { return listener_.get(); }

 private:
  mojo::InterfacePtr<service_manager::mojom::ServiceManager> service_manager_;
  std::unique_ptr<TestServiceManagerListener> listener_;
};

// Ensures the proxy resolver service is started correctly and stopped when no
// resolvers are open.
IN_PROC_BROWSER_TEST_F(ChromeMojoProxyResolverFactoryBrowserTest,
                       ServiceLifecycle) {
  // Set up the ProxyResolverFactory.
  proxy_resolver::mojom::ProxyResolverFactoryPtr resolver_factory =
      ChromeMojoProxyResolverFactory::CreateWithStrongBinding();

  // Create a resolver, this should create and start the service.
  std::unique_ptr<DumbProxyResolverFactoryRequestClient> resolver_client1 =
      std::make_unique<DumbProxyResolverFactoryRequestClient>();
  resolver_client1->CreateResolver(resolver_factory.get());

  listener()->WaitUntilServiceStarted();

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
    base::PostDelayedTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                                    run_loop.QuitClosure(),
                                    kServiceShutdownTimeout);
    run_loop.Run();
  }
  ASSERT_TRUE(listener()->service_running());

  // Close the last resolver, the service should now go away.
  resolver_client2.reset();
  listener()->WaitUntilServiceStopped();
}

// Same as above, but destroys the ProxyResolverFactory, which should have no
// impact on resolver lifetime.
IN_PROC_BROWSER_TEST_F(ChromeMojoProxyResolverFactoryBrowserTest,
                       DestroyFactory) {
  // Set up the ProxyResolverFactory.
  proxy_resolver::mojom::ProxyResolverFactoryPtr resolver_factory =
      ChromeMojoProxyResolverFactory::CreateWithStrongBinding();

  // Create a resolver, this should create and start the service.
  std::unique_ptr<DumbProxyResolverFactoryRequestClient> resolver_client1 =
      std::make_unique<DumbProxyResolverFactoryRequestClient>();
  resolver_client1->CreateResolver(resolver_factory.get());

  listener()->WaitUntilServiceStarted();

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
    base::PostDelayedTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                                    run_loop.QuitClosure(),
                                    kServiceShutdownTimeout);
    run_loop.Run();
  }
  ASSERT_TRUE(listener()->service_running());

  // Close the last resolver, the service should now go away.
  resolver_client2.reset();
  listener()->WaitUntilServiceStopped();
}

// Make sure the service can be started again after it's been stopped.
IN_PROC_BROWSER_TEST_F(ChromeMojoProxyResolverFactoryBrowserTest,
                       DestroyAndCreateService) {
  // Set up the ProxyResolverFactory.
  proxy_resolver::mojom::ProxyResolverFactoryPtr resolver_factory =
      ChromeMojoProxyResolverFactory::CreateWithStrongBinding();

  // Create a resolver, this should create and start the service.
  std::unique_ptr<DumbProxyResolverFactoryRequestClient> resolver_client =
      std::make_unique<DumbProxyResolverFactoryRequestClient>();
  resolver_client->CreateResolver(resolver_factory.get());
  listener()->WaitUntilServiceStarted();

  // Close the resolver, the service should stop.
  resolver_client.reset();
  listener()->WaitUntilServiceStopped();

  // Create a resolver again, using the same factory. This should create and
  // start the service.
  resolver_client = std::make_unique<DumbProxyResolverFactoryRequestClient>();
  resolver_client->CreateResolver(resolver_factory.get());
  listener()->WaitUntilServiceStarted();

  // Close the resolver again, the service should stop.
  resolver_client.reset();
  listener()->WaitUntilServiceStopped();
}

}  // namespace
