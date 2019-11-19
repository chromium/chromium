// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/local_discovery/service_discovery_client_impl.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/record_rdata.h"

namespace local_discovery {

namespace {

// TODO(noamsml): Make this configurable through the LocalDomainResolver
// interface.
const int kLocalDomainSecondAddressTimeoutMs = 100;

const int kInitialRequeryTimeSeconds = 1;
const int kMaxRequeryTimeSeconds = 2;  // Time for last requery

}  // namespace

ServiceDiscoveryClientImpl::ServiceDiscoveryClientImpl(
    net::MDnsClient* mdns_client) : mdns_client_(mdns_client) {
}

ServiceDiscoveryClientImpl::~ServiceDiscoveryClientImpl() {
}

std::unique_ptr<ServiceWatcher>
ServiceDiscoveryClientImpl::CreateServiceWatcher(
    const std::string& service_type,
    ServiceWatcher::UpdatedCallback callback) {
  return std::make_unique<ServiceWatcherImpl>(service_type, std::move(callback),
                                              mdns_client_);
}

std::unique_ptr<ServiceResolver>
ServiceDiscoveryClientImpl::CreateServiceResolver(
    const std::string& service_name,
    ServiceResolver::ResolveCompleteCallback callback) {
  return std::make_unique<ServiceResolverImpl>(
      service_name, std::move(callback), mdns_client_);
}

std::unique_ptr<LocalDomainResolver>
ServiceDiscoveryClientImpl::CreateLocalDomainResolver(
    const std::string& domain,
    net::AddressFamily address_family,
    LocalDomainResolver::IPAddressCallback callback) {
  return std::make_unique<LocalDomainResolverImpl>(
      domain, address_family, std::move(callback), mdns_client_);
}

ServiceWatcherImpl::ServiceWatcherImpl(const std::string& service_type,
                                       ServiceWatcher::UpdatedCallback callback,
                                       net::MDnsClient* mdns_client)
    : service_type_(service_type),
      callback_(std::move(callback)),
      started_(false),
      actively_refresh_services_(false),
      mdns_client_(mdns_client) {}

void ServiceWatcherImpl::Start() {
  DCHECK(!started_);
  listener_ = mdns_client_->CreateListener(
      net::dns_protocol::kTypePTR, service_type_, this);
  started_ = listener_->Start();
  if (started_)
    ReadCachedServices();
}

ServiceWatcherImpl::~ServiceWatcherImpl() {
}

void ServiceWatcherImpl::DiscoverNewServices() {
  DCHECK(started_);
  SendQuery(kInitialRequeryTimeSeconds);
}

void ServiceWatcherImpl::SetActivelyRefreshServices(
    bool actively_refresh_services) {
  DCHECK(started_);
  actively_refresh_services_ = actively_refresh_services;

  for (auto& it : services_)
    it.second->SetActiveRefresh(actively_refresh_services);
}

void ServiceWatcherImpl::ReadCachedServices() {
  DCHECK(started_);
  CreateTransaction(false /*network*/, true /*cache*/, &transaction_cache_);
}

bool ServiceWatcherImpl::CreateTransaction(
    bool network,
    bool cache,
    std::unique_ptr<net::MDnsTransaction>* transaction) {
  int transaction_flags = 0;
  if (network)
    transaction_flags |= net::MDnsTransaction::QUERY_NETWORK;

  if (cache)
    transaction_flags |= net::MDnsTransaction::QUERY_CACHE;

  if (transaction_flags) {
    *transaction = mdns_client_->CreateTransaction(
        net::dns_protocol::kTypePTR, service_type_, transaction_flags,
        base::Bind(&ServiceWatcherImpl::OnTransactionResponse,
                   AsWeakPtr(), transaction));
    return (*transaction)->Start();
  }

  return true;
}

std::string ServiceWatcherImpl::GetServiceType() const {
  return listener_->GetName();
}

void ServiceWatcherImpl::OnRecordUpdate(
    net::MDnsListener::UpdateType update,
    const net::RecordParsed* record) {
  DCHECK(started_);
  if (record->type() == net::dns_protocol::kTypePTR) {
    DCHECK(record->name() == GetServiceType());
    const net::PtrRecordRdata* rdata = record->rdata<net::PtrRecordRdata>();

    switch (update) {
      case net::MDnsListener::RECORD_ADDED:
        AddService(rdata->ptrdomain());
        break;
      case net::MDnsListener::RECORD_CHANGED:
        NOTREACHED();
        break;
      case net::MDnsListener::RECORD_REMOVED:
        RemovePTR(rdata->ptrdomain());
        break;
    }
    return;
  }

  DCHECK(record->type() == net::dns_protocol::kTypeSRV ||
         record->type() == net::dns_protocol::kTypeTXT);
  DCHECK(services_.find(record->name()) != services_.end());

  if (record->type() == net::dns_protocol::kTypeSRV) {
    if (update == net::MDnsListener::RECORD_REMOVED)
      RemoveSRV(record->name());
    else if (update == net::MDnsListener::RECORD_ADDED)
      AddSRV(record->name());
  }

  // If this is the first time we see an SRV record, do not send
  // an UPDATE_CHANGED.
  if (record->type() != net::dns_protocol::kTypeSRV ||
      update != net::MDnsListener::RECORD_ADDED) {
    DeferUpdate(UPDATE_CHANGED, record->name());
  }
}

void ServiceWatcherImpl::OnCachePurged() {
  // Not yet implemented.
}

void ServiceWatcherImpl::OnTransactionResponse(
    std::unique_ptr<net::MDnsTransaction>* transaction,
    net::MDnsTransaction::Result result,
    const net::RecordParsed* record) {
  DCHECK(started_);
  if (result == net::MDnsTransaction::RESULT_RECORD) {
    AddService(record->rdata<net::PtrRecordRdata>()->ptrdomain());
  } else if (result == net::MDnsTransaction::RESULT_DONE) {
    transaction->reset();
  }

  // Do nothing for NSEC records. It is an error for hosts to broadcast an NSEC
  // record for PTR records on any name.
}

ServiceWatcherImpl::ServiceListeners::ServiceListeners(
    const std::string& service_name,
    ServiceWatcherImpl* watcher,
    net::MDnsClient* mdns_client)
    : service_name_(service_name), mdns_client_(mdns_client),
      update_pending_(false), has_ptr_(true), has_srv_(false) {
  srv_listener_ = mdns_client->CreateListener(
      net::dns_protocol::kTypeSRV, service_name, watcher);
  txt_listener_ = mdns_client->CreateListener(
      net::dns_protocol::kTypeTXT, service_name, watcher);
}

ServiceWatcherImpl::ServiceListeners::~ServiceListeners() {
}

bool ServiceWatcherImpl::ServiceListeners::Start() {
  return srv_listener_->Start() && txt_listener_->Start();
}

void ServiceWatcherImpl::ServiceListeners::SetActiveRefresh(
    bool active_refresh) {
  srv_listener_->SetActiveRefresh(active_refresh);

  if (active_refresh && !has_srv_) {
    DCHECK(has_ptr_);
    srv_transaction_ = mdns_client_->CreateTransaction(
        net::dns_protocol::kTypeSRV, service_name_,
        net::MDnsTransaction::SINGLE_RESULT |
        net::MDnsTransaction::QUERY_CACHE | net::MDnsTransaction::QUERY_NETWORK,
        base::Bind(&ServiceWatcherImpl::ServiceListeners::OnSRVRecord,
                   base::Unretained(this)));
    srv_transaction_->Start();
  } else if (!active_refresh) {
    srv_transaction_.reset();
  }
}

void ServiceWatcherImpl::ServiceListeners::OnSRVRecord(
    net::MDnsTransaction::Result result,
    const net::RecordParsed* record) {
  set_has_srv(!!record);
}

void ServiceWatcherImpl::ServiceListeners::set_has_srv(bool has_srv) {
  has_srv_ = has_srv;
  srv_transaction_.reset();
}

void ServiceWatcherImpl::AddService(const std::string& service) {
  DCHECK(started_);

  std::unique_ptr<ServiceListeners>& listener = services_[service];
  if (!listener) {
    listener.reset(new ServiceListeners(service, this, mdns_client_));
    bool success = listener->Start();
    DCHECK(success);
    listener->SetActiveRefresh(actively_refresh_services_);
    DeferUpdate(UPDATE_ADDED, service);
  }
  listener->set_has_ptr(true);
}

void ServiceWatcherImpl::AddSRV(const std::string& service) {
  DCHECK(started_);

  auto it = services_.find(service);
  if (it != services_.end())
    it->second->set_has_srv(true);
}

void ServiceWatcherImpl::DeferUpdate(ServiceWatcher::UpdateType update_type,
                                     const std::string& service_name) {
  auto it = services_.find(service_name);
  if (it != services_.end() && !it->second->update_pending()) {
    it->second->set_update_pending(true);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&ServiceWatcherImpl::DeliverDeferredUpdate,
                                  AsWeakPtr(), update_type, service_name));
  }
}

void ServiceWatcherImpl::DeliverDeferredUpdate(
    ServiceWatcher::UpdateType update_type, const std::string& service_name) {
  auto it = services_.find(service_name);
  if (it != services_.end()) {
    it->second->set_update_pending(false);
    if (!callback_.is_null())
      callback_.Run(update_type, service_name);
  }
}

void ServiceWatcherImpl::RemovePTR(const std::string& service) {
  DCHECK(started_);

  auto it = services_.find(service);
  if (it != services_.end()) {
    it->second->set_has_ptr(false);
    if (!it->second->has_ptr_or_srv()) {
      services_.erase(it);
      if (!callback_.is_null())
        callback_.Run(UPDATE_REMOVED, service);
    }
  }
}

void ServiceWatcherImpl::RemoveSRV(const std::string& service) {
  DCHECK(started_);

  auto it = services_.find(service);
  if (it != services_.end()) {
    it->second->set_has_srv(false);
    if (!it->second->has_ptr_or_srv()) {
      services_.erase(it);
      if (!callback_.is_null())
        callback_.Run(UPDATE_REMOVED, service);
    }
  }
}

void ServiceWatcherImpl::OnNsecRecord(const std::string& name,
                                      unsigned rrtype) {
  // Do nothing. It is an error for hosts to broadcast an NSEC record for PTR
  // on any name.
}

void ServiceWatcherImpl::ScheduleQuery(int timeout_seconds) {
  if (timeout_seconds <= kMaxRequeryTimeSeconds) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ServiceWatcherImpl::SendQuery, AsWeakPtr(),
                       timeout_seconds * 2 /*next_timeout_seconds*/),
        base::TimeDelta::FromSeconds(timeout_seconds));
  }
}

void ServiceWatcherImpl::SendQuery(int next_timeout_seconds) {
  CreateTransaction(true /*network*/, false /*cache*/, &transaction_network_);
  ScheduleQuery(next_timeout_seconds);
}

ServiceResolverImpl::ServiceResolverImpl(const std::string& service_name,
                                         ResolveCompleteCallback callback,
                                         net::MDnsClient* mdns_client)
    : service_name_(service_name),
      callback_(std::move(callback)),
      metadata_resolved_(false),
      address_resolved_(false),
      mdns_client_(mdns_client) {}

void ServiceResolverImpl::StartResolving() {
  address_resolved_ = false;
  metadata_resolved_ = false;
  service_staging_ = ServiceDescription();
  service_staging_.service_name = service_name_;

  if (!CreateTxtTransaction() || !CreateSrvTransaction()) {
    ServiceNotFound(ServiceResolver::STATUS_REQUEST_TIMEOUT);
  }
}

ServiceResolverImpl::~ServiceResolverImpl() {
}

bool ServiceResolverImpl::CreateTxtTransaction() {
  txt_transaction_ = mdns_client_->CreateTransaction(
      net::dns_protocol::kTypeTXT, service_name_,
      net::MDnsTransaction::SINGLE_RESULT | net::MDnsTransaction::QUERY_CACHE |
      net::MDnsTransaction::QUERY_NETWORK,
      base::Bind(&ServiceResolverImpl::TxtRecordTransactionResponse,
                 AsWeakPtr()));
  return txt_transaction_->Start();
}

// TODO(noamsml): quick-resolve for AAAA records.  Since A records tend to be in
void ServiceResolverImpl::CreateATransaction() {
  a_transaction_ = mdns_client_->CreateTransaction(
      net::dns_protocol::kTypeA,
      service_staging_.address.host(),
      net::MDnsTransaction::SINGLE_RESULT | net::MDnsTransaction::QUERY_CACHE,
      base::Bind(&ServiceResolverImpl::ARecordTransactionResponse,
                 AsWeakPtr()));
  a_transaction_->Start();
}

bool ServiceResolverImpl::CreateSrvTransaction() {
  srv_transaction_ = mdns_client_->CreateTransaction(
      net::dns_protocol::kTypeSRV, service_name_,
      net::MDnsTransaction::SINGLE_RESULT | net::MDnsTransaction::QUERY_CACHE |
      net::MDnsTransaction::QUERY_NETWORK,
      base::Bind(&ServiceResolverImpl::SrvRecordTransactionResponse,
                 AsWeakPtr()));
  return srv_transaction_->Start();
}

std::string ServiceResolverImpl::GetName() const {
  return service_name_;
}

void ServiceResolverImpl::SrvRecordTransactionResponse(
    net::MDnsTransaction::Result status, const net::RecordParsed* record) {
  srv_transaction_.reset();
  if (status != net::MDnsTransaction::RESULT_RECORD) {
    ServiceNotFound(MDnsStatusToRequestStatus(status));
    return;
  }

  DCHECK(record);
  service_staging_.address = RecordToAddress(record);
  service_staging_.last_seen = record->time_created();
  CreateATransaction();
}

void ServiceResolverImpl::TxtRecordTransactionResponse(
    net::MDnsTransaction::Result status, const net::RecordParsed* record) {
  txt_transaction_.reset();
  if (status == net::MDnsTransaction::RESULT_RECORD) {
    DCHECK(record);
    service_staging_.metadata = RecordToMetadata(record);
  } else {
    service_staging_.metadata.clear();
  }

  metadata_resolved_ = true;
  AlertCallbackIfReady();
}

void ServiceResolverImpl::ARecordTransactionResponse(
    net::MDnsTransaction::Result status, const net::RecordParsed* record) {
  a_transaction_.reset();

  if (status == net::MDnsTransaction::RESULT_RECORD) {
    DCHECK(record);
    service_staging_.ip_address = RecordToIPAddress(record);
  } else {
    service_staging_.ip_address = net::IPAddress();
  }

  address_resolved_ = true;
  AlertCallbackIfReady();
}

void ServiceResolverImpl::AlertCallbackIfReady() {
  if (metadata_resolved_ && address_resolved_) {
    txt_transaction_.reset();
    srv_transaction_.reset();
    a_transaction_.reset();
    if (!callback_.is_null())
      std::move(callback_).Run(STATUS_SUCCESS, service_staging_);
  }
}

void ServiceResolverImpl::ServiceNotFound(
    ServiceResolver::RequestStatus status) {
  txt_transaction_.reset();
  srv_transaction_.reset();
  a_transaction_.reset();
  if (!callback_.is_null())
    std::move(callback_).Run(status, ServiceDescription());
}

ServiceResolver::RequestStatus ServiceResolverImpl::MDnsStatusToRequestStatus(
    net::MDnsTransaction::Result status) const {
  switch (status) {
    case net::MDnsTransaction::RESULT_RECORD:
      return ServiceResolver::STATUS_SUCCESS;
    case net::MDnsTransaction::RESULT_NO_RESULTS:
      return ServiceResolver::STATUS_REQUEST_TIMEOUT;
    case net::MDnsTransaction::RESULT_NSEC:
      return ServiceResolver::STATUS_KNOWN_NONEXISTENT;
    case net::MDnsTransaction::RESULT_DONE:  // Pass through.
    default:
      NOTREACHED();
      return ServiceResolver::STATUS_REQUEST_TIMEOUT;
  }
}

const std::vector<std::string>& ServiceResolverImpl::RecordToMetadata(
    const net::RecordParsed* record) const {
  DCHECK(record->type() == net::dns_protocol::kTypeTXT);
  return record->rdata<net::TxtRecordRdata>()->texts();
}

net::HostPortPair ServiceResolverImpl::RecordToAddress(
    const net::RecordParsed* record) const {
  DCHECK(record->type() == net::dns_protocol::kTypeSRV);
  const net::SrvRecordRdata* srv_rdata = record->rdata<net::SrvRecordRdata>();
  return net::HostPortPair(srv_rdata->target(), srv_rdata->port());
}

net::IPAddress ServiceResolverImpl::RecordToIPAddress(
    const net::RecordParsed* record) const {
  DCHECK(record->type() == net::dns_protocol::kTypeA);
  return record->rdata<net::ARecordRdata>()->address();
}

LocalDomainResolverImpl::LocalDomainResolverImpl(
    const std::string& domain,
    net::AddressFamily address_family,
    IPAddressCallback callback,
    net::MDnsClient* mdns_client)
    : domain_(domain),
      address_family_(address_family),
      callback_(std::move(callback)),
      transactions_finished_(0),
      mdns_client_(mdns_client) {}

LocalDomainResolverImpl::~LocalDomainResolverImpl() {
  timeout_callback_.Cancel();
}

void LocalDomainResolverImpl::Start() {
  if (address_family_ == net::ADDRESS_FAMILY_IPV4 ||
      address_family_ == net::ADDRESS_FAMILY_UNSPECIFIED) {
    transaction_a_ = CreateTransaction(net::dns_protocol::kTypeA);
    transaction_a_->Start();
  }

  if (address_family_ == net::ADDRESS_FAMILY_IPV6 ||
      address_family_ == net::ADDRESS_FAMILY_UNSPECIFIED) {
    transaction_aaaa_ = CreateTransaction(net::dns_protocol::kTypeAAAA);
    transaction_aaaa_->Start();
  }
}

std::unique_ptr<net::MDnsTransaction>
LocalDomainResolverImpl::CreateTransaction(uint16_t type) {
  return mdns_client_->CreateTransaction(
      type, domain_, net::MDnsTransaction::SINGLE_RESULT |
                     net::MDnsTransaction::QUERY_CACHE |
                     net::MDnsTransaction::QUERY_NETWORK,
      base::Bind(&LocalDomainResolverImpl::OnTransactionComplete,
                 base::Unretained(this)));
}

void LocalDomainResolverImpl::OnTransactionComplete(
    net::MDnsTransaction::Result result, const net::RecordParsed* record) {
  transactions_finished_++;

  if (result == net::MDnsTransaction::RESULT_RECORD) {
    if (record->type() == net::dns_protocol::kTypeA) {
      address_ipv4_ = record->rdata<net::ARecordRdata>()->address();
    } else {
      DCHECK_EQ(net::dns_protocol::kTypeAAAA, record->type());
      address_ipv6_ = record->rdata<net::AAAARecordRdata>()->address();
    }
  }

  if (transactions_finished_ == 1 &&
      address_family_ == net::ADDRESS_FAMILY_UNSPECIFIED) {
    timeout_callback_.Reset(
        base::BindOnce(&LocalDomainResolverImpl::SendResolvedAddresses,
                       base::Unretained(this)));

    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, timeout_callback_.callback(),
        base::TimeDelta::FromMilliseconds(kLocalDomainSecondAddressTimeoutMs));
  } else if (transactions_finished_ == 2
      || address_family_ != net::ADDRESS_FAMILY_UNSPECIFIED) {
    SendResolvedAddresses();
  }
}

bool LocalDomainResolverImpl::IsSuccess() {
  return address_ipv4_.IsValid() || address_ipv6_.IsValid();
}

void LocalDomainResolverImpl::SendResolvedAddresses() {
  transaction_a_.reset();
  transaction_aaaa_.reset();
  timeout_callback_.Cancel();
  std::move(callback_).Run(IsSuccess(), address_ipv4_, address_ipv6_);
}

}  // namespace local_discovery
