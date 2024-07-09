// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/service_discovery_client_mdns.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/local_discovery/service_discovery_client_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "net/base/net_errors.h"
#include "net/socket/datagram_server_socket.h"

namespace net {
class IPAddress;
}

namespace local_discovery {

using content::BrowserThread;

// Base class for objects returned by ServiceDiscoveryClient implementation.
// Handles interaction of client code on UI thread end net code on mdns thread.
class ServiceDiscoveryClientMdns::Proxy {
 public:
  using WeakPtr = base::WeakPtr<Proxy>;

  explicit Proxy(ServiceDiscoveryClientMdns* client) : client_(client) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    client_->proxies_.AddObserver(this);
  }

  Proxy(const Proxy&) = delete;
  Proxy& operator=(const Proxy&) = delete;

  virtual ~Proxy() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    client_->proxies_.RemoveObserver(this);
  }

  // Returns true if object is not yet shutdown.
  virtual bool IsValid() = 0;

  // Notifies proxies that mDNS layer is going to be destroyed.
  virtual void OnMdnsDestroy() = 0;

  // Notifies proxies that new mDNS instance is ready.
  virtual void OnNewMdnsReady() {
    DCHECK(!client_->need_delay_mdns_tasks_);
    if (IsValid()) {
      for (auto& task : delayed_tasks_)
        client_->mdns_runner_->PostTask(FROM_HERE, std::move(task));
    }
    delayed_tasks_.clear();
  }

  // Runs callback using this method to abort callback if instance of |Proxy|
  // is deleted.
  void RunCallback(base::OnceClosure callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    std::move(callback).Run();
  }

 protected:
  void PostToMdnsThread(base::OnceClosure task) {
    DCHECK(IsValid());
    // The first task on the IO thread for each |mdns_| instance must be
    // InitMdns(). OnInterfaceListReady() could be delayed by
    // GetMDnsInterfacesToBind() running on a background task runner, so
    // PostToMdnsThread() could be called to post task for |mdns_| that is not
    // initialized yet.
    if (!client_->need_delay_mdns_tasks_) {
      client_->mdns_runner_->PostTask(FROM_HERE, std::move(task));
      return;
    }
    delayed_tasks_.emplace_back(std::move(task));
  }

  static bool PostToUIThread(base::OnceClosure task) {
    return content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                        std::move(task));
  }

  ServiceDiscoveryClient* client() {
    return client_->client_.get();
  }

  WeakPtr GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  template<class T>
  void DeleteOnMdnsThread(T* t) {
    if (!t)
      return;
    // If DeleteSoon fails to run `t` will be leaked as it is unsafe to
    // delete t on the current thread.
    client_->mdns_runner_->DeleteSoon(FROM_HERE, t);
  }

 private:
  scoped_refptr<ServiceDiscoveryClientMdns> client_;
  // Delayed |mdns_runner_| tasks.
  std::vector<base::OnceClosure> delayed_tasks_;
  base::WeakPtrFactory<Proxy> weak_ptr_factory_{this};
};

namespace {

const int kMaxRestartAttempts = 10;
const int kRestartDelayOnNetworkChangeSeconds = 3;

using MdnsInitCallback = base::OnceCallback<void(int)>;

class SocketFactory : public net::MDnsSocketFactory {
 public:
  explicit SocketFactory(const net::InterfaceIndexFamilyList& interfaces)
      : interfaces_(interfaces) {}

  SocketFactory(const SocketFactory&) = delete;
  SocketFactory& operator=(const SocketFactory&) = delete;

  // net::MDnsSocketFactory implementation:
  void CreateSockets(std::vector<std::unique_ptr<net::DatagramServerSocket>>*
                         sockets) override {
    for (size_t i = 0; i < interfaces_.size(); ++i) {
      DCHECK(interfaces_[i].second == net::ADDRESS_FAMILY_IPV4 ||
             interfaces_[i].second == net::ADDRESS_FAMILY_IPV6);
      std::unique_ptr<net::DatagramServerSocket> socket(CreateAndBindMDnsSocket(
          interfaces_[i].second, interfaces_[i].first, nullptr /* net_log */));
      if (socket)
        sockets->push_back(std::move(socket));
    }
  }

 private:
  net::InterfaceIndexFamilyList interfaces_;
};

void InitMdns(MdnsInitCallback on_initialized,
              const net::InterfaceIndexFamilyList& interfaces,
              net::MDnsClient* mdns) {
  SocketFactory socket_factory(interfaces);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_initialized),
                                mdns->StartListening(&socket_factory)));
}

template<class T>
class ProxyBase : public ServiceDiscoveryClientMdns::Proxy, public T {
 public:
  using Base = ProxyBase<T>;

  explicit ProxyBase(ServiceDiscoveryClientMdns* client)
      : Proxy(client) {
  }

  ProxyBase(const ProxyBase&) = delete;
  ProxyBase& operator=(const ProxyBase&) = delete;

  ~ProxyBase() override {
    DeleteOnMdnsThread(implementation_.release());
  }

  bool IsValid() override {
    return !!implementation();
  }

  void OnMdnsDestroy() override {
    DeleteOnMdnsThread(implementation_.release());
  }

 protected:
  void set_implementation(std::unique_ptr<T> implementation) {
    implementation_ = std::move(implementation);
  }

  T* implementation()  const {
    return implementation_.get();
  }

 private:
  std::unique_ptr<T> implementation_;
};

class ServiceWatcherProxy : public ProxyBase<ServiceWatcher> {
 public:
  ServiceWatcherProxy(ServiceDiscoveryClientMdns* client_mdns,
                      const std::string& service_type,
                      ServiceWatcher::UpdatedCallback callback)
      : ProxyBase(client_mdns),
        service_type_(service_type),
        callback_(callback) {
    // It's safe to call |CreateServiceWatcher| on UI thread, because
    // |MDnsClient| is not used there. It's simplify implementation.
    set_implementation(client()->CreateServiceWatcher(
        service_type, base::BindRepeating(&ServiceWatcherProxy::OnCallback,
                                          GetWeakPtr(), std::move(callback))));
  }

  ServiceWatcherProxy(const ServiceWatcherProxy&) = delete;
  ServiceWatcherProxy& operator=(const ServiceWatcherProxy&) = delete;

  // ServiceWatcher methods.
  void Start() override {
    if (implementation()) {
      PostToMdnsThread(base::BindOnce(&ServiceWatcher::Start,
                                      base::Unretained(implementation())));
    }
  }

  void DiscoverNewServices() override {
    if (implementation()) {
      PostToMdnsThread(base::BindOnce(&ServiceWatcher::DiscoverNewServices,
                                      base::Unretained(implementation())));
    }
  }

  void SetActivelyRefreshServices(bool actively_refresh_services) override {
    if (implementation()) {
      PostToMdnsThread(base::BindOnce(
          &ServiceWatcher::SetActivelyRefreshServices,
          base::Unretained(implementation()), actively_refresh_services));
    }
  }

  std::string GetServiceType() const override { return service_type_; }

  void OnNewMdnsReady() override {
    ProxyBase<ServiceWatcher>::OnNewMdnsReady();
    if (!implementation())
      callback_.Run(ServiceWatcher::UPDATE_INVALIDATED, "");
  }

 private:
  static void OnCallback(const WeakPtr& proxy,
                         ServiceWatcher::UpdatedCallback callback,
                         UpdateType a1,
                         const std::string& a2) {
    DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
    PostToUIThread(base::BindOnce(&Base::RunCallback, proxy,
                                  base::BindOnce(std::move(callback), a1, a2)));
  }
  std::string service_type_;
  ServiceWatcher::UpdatedCallback callback_;
};

class ServiceResolverProxy : public ProxyBase<ServiceResolver> {
 public:
  ServiceResolverProxy(ServiceDiscoveryClientMdns* client_mdns,
                       const std::string& service_name,
                       ServiceResolver::ResolveCompleteCallback callback)
      : ProxyBase(client_mdns), service_name_(service_name) {
    // It's safe to call |CreateServiceResolver| on UI thread, because
    // |MDnsClient| is not used there. It's simplify implementation.
    set_implementation(client()->CreateServiceResolver(
        service_name, base::BindOnce(&ServiceResolverProxy::OnCallback,
                                     GetWeakPtr(), std::move(callback))));
  }

  ServiceResolverProxy(const ServiceResolverProxy&) = delete;
  ServiceResolverProxy& operator=(const ServiceResolverProxy&) = delete;

  // ServiceResolver methods.
  void StartResolving() override {
    if (implementation()) {
      PostToMdnsThread(base::BindOnce(&ServiceResolver::StartResolving,
                                      base::Unretained(implementation())));
    }
  }

  std::string GetName() const override { return service_name_; }

 private:
  static void OnCallback(const WeakPtr& proxy,
                         ServiceResolver::ResolveCompleteCallback callback,
                         RequestStatus a1,
                         const ServiceDescription& a2) {
    DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
    PostToUIThread(base::BindOnce(&Base::RunCallback, proxy,
                                  base::BindOnce(std::move(callback), a1, a2)));
  }

  std::string service_name_;
};

class LocalDomainResolverProxy : public ProxyBase<LocalDomainResolver> {
 public:
  LocalDomainResolverProxy(ServiceDiscoveryClientMdns* client_mdns,
                           const std::string& domain,
                           net::AddressFamily address_family,
                           LocalDomainResolver::IPAddressCallback callback)
      : ProxyBase(client_mdns) {
    // It's safe to call |CreateLocalDomainResolver| on UI thread, because
    // |MDnsClient| is not used there. It's simplify implementation.
    set_implementation(client()->CreateLocalDomainResolver(
        domain, address_family,
        base::BindOnce(&LocalDomainResolverProxy::OnCallback, GetWeakPtr(),
                       std::move(callback))));
  }

  LocalDomainResolverProxy(const LocalDomainResolverProxy&) = delete;
  LocalDomainResolverProxy& operator=(const LocalDomainResolverProxy&) = delete;

  // LocalDomainResolver methods.
  void Start() override {
    if (implementation()) {
      PostToMdnsThread(base::BindOnce(&LocalDomainResolver::Start,
                                      base::Unretained(implementation())));
    }
  }

 private:
  static void OnCallback(const WeakPtr& proxy,
                         LocalDomainResolver::IPAddressCallback callback,
                         bool a1,
                         const net::IPAddress& a2,
                         const net::IPAddress& a3) {
    DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
    PostToUIThread(
        base::BindOnce(&Base::RunCallback, proxy,
                       base::BindOnce(std::move(callback), a1, a2, a3)));
  }
};

}  // namespace

ServiceDiscoveryClientMdns::ServiceDiscoveryClientMdns()
    : mdns_runner_(content::GetIOThreadTaskRunner({})) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  StartNewClient();
}

std::unique_ptr<ServiceWatcher>
ServiceDiscoveryClientMdns::CreateServiceWatcher(
    const std::string& service_type,
    ServiceWatcher::UpdatedCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return std::make_unique<ServiceWatcherProxy>(this, service_type,
                                               std::move(callback));
}

std::unique_ptr<ServiceResolver>
ServiceDiscoveryClientMdns::CreateServiceResolver(
    const std::string& service_name,
    ServiceResolver::ResolveCompleteCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return std::make_unique<ServiceResolverProxy>(this, service_name,
                                                std::move(callback));
}

std::unique_ptr<LocalDomainResolver>
ServiceDiscoveryClientMdns::CreateLocalDomainResolver(
    const std::string& domain,
    net::AddressFamily address_family,
    LocalDomainResolver::IPAddressCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return std::make_unique<LocalDomainResolverProxy>(
      this, domain, address_family, std::move(callback));
}

ServiceDiscoveryClientMdns::~ServiceDiscoveryClientMdns() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
  DestroyMdns();
}

void ServiceDiscoveryClientMdns::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Only network changes resets counter.
  restart_attempts_ = 0;
  ScheduleStartNewClient();
}

void ServiceDiscoveryClientMdns::ScheduleStartNewClient() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OnBeforeMdnsDestroy();
  if (restart_attempts_ >= kMaxRestartAttempts)
    return;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ServiceDiscoveryClientMdns::StartNewClient,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Seconds(kRestartDelayOnNetworkChangeSeconds *
                    (1 << restart_attempts_)));
}

void ServiceDiscoveryClientMdns::StartNewClient() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ++restart_attempts_;
  DestroyMdns();
  mdns_ = net::MDnsClient::CreateDefault();
  client_ = std::make_unique<ServiceDiscoveryClientImpl>(mdns_.get());
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&net::GetMDnsInterfacesToBind),
      base::BindOnce(&ServiceDiscoveryClientMdns::OnInterfaceListReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ServiceDiscoveryClientMdns::OnInterfaceListReady(
    const net::InterfaceIndexFamilyList& interfaces) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mdns_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &InitMdns,
          base::BindOnce(&ServiceDiscoveryClientMdns::OnMdnsInitialized,
                         weak_ptr_factory_.GetWeakPtr()),
          interfaces, base::Unretained(mdns_.get())));
}

void ServiceDiscoveryClientMdns::OnMdnsInitialized(int net_error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (net_error != net::OK) {
    ScheduleStartNewClient();
    return;
  }

  // Initialization is done, no need to delay tasks.
  need_delay_mdns_tasks_ = false;
  for (Proxy& observer : proxies_)
    observer.OnNewMdnsReady();
}

void ServiceDiscoveryClientMdns::OnBeforeMdnsDestroy() {
  need_delay_mdns_tasks_ = true;
  weak_ptr_factory_.InvalidateWeakPtrs();
  for (Proxy& observer : proxies_)
    observer.OnMdnsDestroy();
}

void ServiceDiscoveryClientMdns::DestroyMdns() {
  OnBeforeMdnsDestroy();
  // After calling Proxy::OnMdnsDestroy(), all references to |client_| and
  // |mdns_| should be destroyed.
  if (client_)
    mdns_runner_->DeleteSoon(FROM_HERE, client_.release());
  if (mdns_)
    mdns_runner_->DeleteSoon(FROM_HERE, mdns_.release());
}

}  // namespace local_discovery
