// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_IMPL_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/local_discovery/service_discovery_client.h"
#include "net/dns/mdns_client.h"

namespace local_discovery {

class ServiceDiscoveryClientImpl : public ServiceDiscoveryClient {
 public:
  // |mdns_client| must outlive the Service Discovery Client.
  explicit ServiceDiscoveryClientImpl(net::MDnsClient* mdns_client);
  ~ServiceDiscoveryClientImpl() override;

  // ServiceDiscoveryClient implementation:
  std::unique_ptr<ServiceWatcher> CreateServiceWatcher(
      const std::string& service_type,
      ServiceWatcher::UpdatedCallback callback) override;

  std::unique_ptr<ServiceResolver> CreateServiceResolver(
      const std::string& service_name,
      ServiceResolver::ResolveCompleteCallback callback) override;

  std::unique_ptr<LocalDomainResolver> CreateLocalDomainResolver(
      const std::string& domain,
      net::AddressFamily address_family,
      LocalDomainResolver::IPAddressCallback callback) override;

 private:
  net::MDnsClient* mdns_client_;

  DISALLOW_COPY_AND_ASSIGN(ServiceDiscoveryClientImpl);
};

class ServiceWatcherImpl : public ServiceWatcher,
                           public net::MDnsListener::Delegate,
                           public base::SupportsWeakPtr<ServiceWatcherImpl> {
 public:
  ServiceWatcherImpl(const std::string& service_type,
                     ServiceWatcher::UpdatedCallback callback,
                     net::MDnsClient* mdns_client);
  // Listening will automatically stop when the destructor is called.
  ~ServiceWatcherImpl() override;

  // ServiceWatcher implementation:
  void Start() override;

  void DiscoverNewServices() override;

  void SetActivelyRefreshServices(bool actively_refresh_services) override;

  std::string GetServiceType() const override;

  void OnRecordUpdate(net::MDnsListener::UpdateType update,
                      const net::RecordParsed* record) override;

  void OnNsecRecord(const std::string& name, unsigned rrtype) override;

  void OnCachePurged() override;

  virtual void OnTransactionResponse(
      std::unique_ptr<net::MDnsTransaction>* transaction,
      net::MDnsTransaction::Result result,
      const net::RecordParsed* record);

 private:
  struct ServiceListeners {
    ServiceListeners(const std::string& service_name,
                     ServiceWatcherImpl* watcher,
                     net::MDnsClient* mdns_client);
    ~ServiceListeners();
    bool Start();
    void SetActiveRefresh(bool auto_update);

    void set_update_pending(bool update_pending) {
      update_pending_ = update_pending;
    }

    bool update_pending() { return update_pending_; }

    void set_has_ptr(bool has_ptr) {
      has_ptr_ = has_ptr;
    }

    void set_has_srv(bool has_srv);

    bool has_ptr_or_srv() { return has_ptr_ || has_srv_; }

   private:
    void OnSRVRecord(net::MDnsTransaction::Result result,
                     const net::RecordParsed* record);

    void DoQuerySRV();

    std::unique_ptr<net::MDnsListener> srv_listener_;
    std::unique_ptr<net::MDnsListener> txt_listener_;
    std::unique_ptr<net::MDnsTransaction> srv_transaction_;

    std::string service_name_;
    net::MDnsClient* mdns_client_;
    bool update_pending_;

    bool has_ptr_;
    bool has_srv_;
  };

  using ServiceListenersMap =
      std::map<std::string, std::unique_ptr<ServiceListeners>>;

  void ReadCachedServices();
  void AddService(const std::string& service);
  void RemovePTR(const std::string& service);
  void RemoveSRV(const std::string& service);
  void AddSRV(const std::string& service);
  bool CreateTransaction(bool active,
                         bool alert_existing_services,
                         std::unique_ptr<net::MDnsTransaction>* transaction);

  void DeferUpdate(ServiceWatcher::UpdateType update_type,
                   const std::string& service_name);
  void DeliverDeferredUpdate(ServiceWatcher::UpdateType update_type,
                             const std::string& service_name);

  void ScheduleQuery(int timeout_seconds);

  void SendQuery(int next_timeout_seconds);

  const std::string service_type_;
  ServiceListenersMap services_;
  std::unique_ptr<net::MDnsTransaction> transaction_network_;
  std::unique_ptr<net::MDnsTransaction> transaction_cache_;
  std::unique_ptr<net::MDnsListener> listener_;

  ServiceWatcher::UpdatedCallback callback_;
  bool started_;
  bool actively_refresh_services_;

  net::MDnsClient* const mdns_client_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWatcherImpl);
};

class ServiceResolverImpl
    : public ServiceResolver,
      public base::SupportsWeakPtr<ServiceResolverImpl> {
 public:
  ServiceResolverImpl(const std::string& service_name,
                      ServiceResolver::ResolveCompleteCallback callback,
                      net::MDnsClient* mdns_client);

  ~ServiceResolverImpl() override;

  // ServiceResolver implementation:
  void StartResolving() override;
  std::string GetName() const override;

 private:
  // Respond to transaction finishing for SRV records.
  void SrvRecordTransactionResponse(net::MDnsTransaction::Result status,
                                    const net::RecordParsed* record);

  // Respond to transaction finishing for TXT records.
  void TxtRecordTransactionResponse(net::MDnsTransaction::Result status,
                                    const net::RecordParsed* record);

  // Respond to transaction finishing for A records.
  void ARecordTransactionResponse(net::MDnsTransaction::Result status,
                                  const net::RecordParsed* record);

  void AlertCallbackIfReady();

  void ServiceNotFound(RequestStatus status);

  // Convert a TXT record to a vector of strings (metadata).
  const std::vector<std::string>& RecordToMetadata(
      const net::RecordParsed* record) const;

  // Convert an SRV record to a host and port pair.
  net::HostPortPair RecordToAddress(
      const net::RecordParsed* record) const;

  // Convert an A record to an IP address.
  net::IPAddress RecordToIPAddress(const net::RecordParsed* record) const;

  // Convert an MDns status to a service discovery status.
  RequestStatus MDnsStatusToRequestStatus(
      net::MDnsTransaction::Result status) const;

  bool CreateTxtTransaction();
  bool CreateSrvTransaction();
  void CreateATransaction();

  const std::string service_name_;
  ResolveCompleteCallback callback_;

  bool metadata_resolved_;
  bool address_resolved_;

  std::unique_ptr<net::MDnsTransaction> txt_transaction_;
  std::unique_ptr<net::MDnsTransaction> srv_transaction_;
  std::unique_ptr<net::MDnsTransaction> a_transaction_;

  ServiceDescription service_staging_;

  net::MDnsClient* const mdns_client_;

  DISALLOW_COPY_AND_ASSIGN(ServiceResolverImpl);
};

class LocalDomainResolverImpl : public LocalDomainResolver {
 public:
  LocalDomainResolverImpl(const std::string& domain,
                          net::AddressFamily address_family,
                          IPAddressCallback callback,
                          net::MDnsClient* mdns_client);
  ~LocalDomainResolverImpl() override;

  void Start() override;

  const std::string& domain() const { return domain_; }

 private:
  void OnTransactionComplete(
      net::MDnsTransaction::Result result,
      const net::RecordParsed* record);

  std::unique_ptr<net::MDnsTransaction> CreateTransaction(uint16_t type);

  bool IsSuccess();

  void SendResolvedAddresses();

  const std::string domain_;
  net::AddressFamily address_family_;
  IPAddressCallback callback_;

  std::unique_ptr<net::MDnsTransaction> transaction_a_;
  std::unique_ptr<net::MDnsTransaction> transaction_aaaa_;

  int transactions_finished_;

  net::MDnsClient* const mdns_client_;

  net::IPAddress address_ipv4_;
  net::IPAddress address_ipv6_;

  base::CancelableOnceClosure timeout_callback_;

  DISALLOW_COPY_AND_ASSIGN(LocalDomainResolverImpl);
};


}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_IMPL_H_
