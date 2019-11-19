// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/service_discovery_client_mac.h"

#import <arpa/inet.h>
#import <Foundation/Foundation.h>
#import <net/if_dl.h>
#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"

using local_discovery::ServiceWatcherImplMac;
using local_discovery::ServiceResolverImplMac;

@interface NetServiceBrowserDelegate
    : NSObject<NSNetServiceBrowserDelegate, NSNetServiceDelegate> {
 @private
  ServiceWatcherImplMac::NetServiceBrowserContainer* container_;  // weak.
  base::scoped_nsobject<NSMutableArray> services_;
}

- (id)initWithContainer:
        (ServiceWatcherImplMac::NetServiceBrowserContainer*)serviceWatcherImpl;
- (void)clearDiscoveredServices;

@end

@interface NetServiceDelegate : NSObject <NSNetServiceDelegate> {
  @private
   ServiceResolverImplMac::NetServiceContainer* container_;
}

- (id)initWithContainer:
        (ServiceResolverImplMac::NetServiceContainer*)serviceResolverImpl;

@end

namespace local_discovery {

namespace {

const char kServiceDiscoveryThreadName[] = "Service Discovery Thread";

const NSTimeInterval kResolveTimeout = 10.0;

// Extracts the instance name, name type and domain from a full service name or
// the service type and domain from a service type. Returns true if successful.
// TODO(justinlin): This current only handles service names with format
// <name>._<protocol2>._<protocol1>.<domain>. Service names with
// subtypes will not parse correctly:
// <name>._<type>._<sub>._<protocol2>._<protocol1>.<domain>.
bool ExtractServiceInfo(const std::string& service,
                        bool is_service_name,
                        base::scoped_nsobject<NSString>* instance,
                        base::scoped_nsobject<NSString>* type,
                        base::scoped_nsobject<NSString>* domain) {
  if (service.empty())
    return false;

  const size_t last_period = service.find_last_of('.');
  if (last_period == std::string::npos || service.length() <= last_period)
    return false;

  if (!is_service_name) {
    type->reset(base::SysUTF8ToNSString(service.substr(0, last_period) + "."),
                base::scoped_policy::RETAIN);
  } else {
    // Find third last period that delimits type and instance name.
    size_t type_period = last_period;
    for (int i = 0; i < 2; ++i) {
      type_period = service.find_last_of('.', type_period - 1);
      if (type_period == std::string::npos)
        return false;
    }

    instance->reset(base::SysUTF8ToNSString(service.substr(0, type_period)),
                    base::scoped_policy::RETAIN);
    type->reset(
        base::SysUTF8ToNSString(
            service.substr(type_period + 1, last_period - type_period)),
        base::scoped_policy::RETAIN);
  }
  domain->reset(base::SysUTF8ToNSString(service.substr(last_period + 1) + "."),
                base::scoped_policy::RETAIN);

  return [*domain length] > 0 &&
         [*type length] > 0 &&
         (!is_service_name || [*instance length] > 0);
}

void ParseTxtRecord(NSData* record, std::vector<std::string>* output) {
  if ([record length] <= 1)
    return;

  VLOG(1) << "ParseTxtRecord: " << [record length];

  const uint8_t* bytes = reinterpret_cast<const uint8_t*>([record bytes]);
  size_t size = [record length];
  size_t offset = 0;
  while (offset < size) {
    uint8_t record_size = bytes[offset++];
    if (offset > size - record_size)
      break;

    base::scoped_nsobject<NSString> txt_record(
        [[NSString alloc] initWithBytes:&bytes[offset]
                                 length:record_size
                               encoding:NSUTF8StringEncoding]);
    if (txt_record) {
      std::string txt_record_string = base::SysNSStringToUTF8(txt_record);
      VLOG(1) << "TxtRecord: " << txt_record_string;
      output->push_back(std::move(txt_record_string));
    } else {
      VLOG(1) << "TxtRecord corrupted at offset " << offset;
    }

    offset += record_size;
  }
}

}  // namespace

ServiceDiscoveryClientMac::ServiceDiscoveryClientMac() {}
ServiceDiscoveryClientMac::~ServiceDiscoveryClientMac() {}

std::unique_ptr<ServiceWatcher> ServiceDiscoveryClientMac::CreateServiceWatcher(
    const std::string& service_type,
    ServiceWatcher::UpdatedCallback callback) {
  StartThreadIfNotStarted();
  VLOG(1) << "CreateServiceWatcher: " << service_type;
  return std::make_unique<ServiceWatcherImplMac>(
      service_type, std::move(callback),
      service_discovery_thread_->task_runner());
}

std::unique_ptr<ServiceResolver>
ServiceDiscoveryClientMac::CreateServiceResolver(
    const std::string& service_name,
    ServiceResolver::ResolveCompleteCallback callback) {
  StartThreadIfNotStarted();
  VLOG(1) << "CreateServiceResolver: " << service_name;
  return std::make_unique<ServiceResolverImplMac>(
      service_name, std::move(callback),
      service_discovery_thread_->task_runner());
}

std::unique_ptr<LocalDomainResolver>
ServiceDiscoveryClientMac::CreateLocalDomainResolver(
    const std::string& domain,
    net::AddressFamily address_family,
    LocalDomainResolver::IPAddressCallback callback) {
  NOTIMPLEMENTED();  // TODO(noamsml): Implement.
  VLOG(1) << "CreateLocalDomainResolver: " << domain;
  return std::unique_ptr<LocalDomainResolver>();
}

void ServiceDiscoveryClientMac::StartThreadIfNotStarted() {
  if (!service_discovery_thread_) {
    service_discovery_thread_.reset(
        new base::Thread(kServiceDiscoveryThreadName));
    // Only TYPE_UI uses an NSRunLoop.
    base::Thread::Options options(base::MessagePumpType::UI, 0);
    service_discovery_thread_->StartWithOptions(options);
  }
}

ServiceWatcherImplMac::NetServiceBrowserContainer::NetServiceBrowserContainer(
    const std::string& service_type,
    ServiceWatcher::UpdatedCallback callback,
    scoped_refptr<base::SingleThreadTaskRunner> service_discovery_runner)
    : service_type_(service_type),
      callback_(std::move(callback)),
      callback_runner_(base::ThreadTaskRunnerHandle::Get()),
      service_discovery_runner_(service_discovery_runner),
      weak_factory_(this) {}

ServiceWatcherImplMac::NetServiceBrowserContainer::
    ~NetServiceBrowserContainer() {
  DCHECK(IsOnServiceDiscoveryThread());

  // Work around a 10.12 bug: NSNetServiceBrowser doesn't lose interest in its
  // weak delegate during deallocation, so a subsequently-deallocated delegate
  // attempts to clear the pointer to itself in an NSNetServiceBrowser that's
  // already gone.
  // https://crbug.com/657495, https://openradar.appspot.com/28943305
  [browser_ setDelegate:nil];

  // Ensure the delegate clears all references to itself, which it had added as
  // discovered services were reported to it.
  [delegate_ clearDiscoveredServices];
}

void ServiceWatcherImplMac::NetServiceBrowserContainer::Start() {
  service_discovery_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NetServiceBrowserContainer::StartOnDiscoveryThread,
                     weak_factory_.GetWeakPtr()));
}

void ServiceWatcherImplMac::NetServiceBrowserContainer::DiscoverNewServices() {
  service_discovery_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NetServiceBrowserContainer::DiscoverOnDiscoveryThread,
                     weak_factory_.GetWeakPtr()));
}

void
ServiceWatcherImplMac::NetServiceBrowserContainer::StartOnDiscoveryThread() {
  DCHECK(IsOnServiceDiscoveryThread());

  delegate_.reset([[NetServiceBrowserDelegate alloc] initWithContainer:this]);
  browser_.reset([[NSNetServiceBrowser alloc] init]);
  [browser_ setDelegate:delegate_];
}

void
ServiceWatcherImplMac::NetServiceBrowserContainer::DiscoverOnDiscoveryThread() {
  DCHECK(IsOnServiceDiscoveryThread());
  base::scoped_nsobject<NSString> instance, type, domain;

  if (!ExtractServiceInfo(service_type_, false, &instance, &type, &domain))
    return;

  DCHECK(![instance length]);
  DVLOG(1) << "Listening for service type '" << [type UTF8String]
           << "' on domain '" << [domain UTF8String] << "'";

  [browser_ searchForServicesOfType:type inDomain:domain];
}

void ServiceWatcherImplMac::NetServiceBrowserContainer::OnServicesUpdate(
    ServiceWatcher::UpdateType update,
    const std::string& service) {
  callback_runner_->PostTask(FROM_HERE,
                             base::BindOnce(callback_, update, service));
}

void ServiceWatcherImplMac::NetServiceBrowserContainer::DeleteSoon() {
  service_discovery_runner_->DeleteSoon(FROM_HERE, this);
}

ServiceWatcherImplMac::ServiceWatcherImplMac(
    const std::string& service_type,
    ServiceWatcher::UpdatedCallback callback,
    scoped_refptr<base::SingleThreadTaskRunner> service_discovery_runner)
    : service_type_(service_type),
      callback_(std::move(callback)),
      started_(false),
      weak_factory_(this) {
  container_.reset(new NetServiceBrowserContainer(
      service_type,
      base::BindRepeating(&ServiceWatcherImplMac::OnServicesUpdate,
                          weak_factory_.GetWeakPtr()),
      service_discovery_runner));
}

ServiceWatcherImplMac::~ServiceWatcherImplMac() {}

void ServiceWatcherImplMac::Start() {
  DCHECK(!started_);
  VLOG(1) << "ServiceWatcherImplMac::Start";
  container_->Start();
  started_ = true;
}

void ServiceWatcherImplMac::DiscoverNewServices() {
  DCHECK(started_);
  VLOG(1) << "ServiceWatcherImplMac::DiscoverNewServices";
  container_->DiscoverNewServices();
}

void ServiceWatcherImplMac::SetActivelyRefreshServices(
    bool actively_refresh_services) {
  DCHECK(started_);
  VLOG(1) << "ServiceWatcherImplMac::SetActivelyRefreshServices";
}

std::string ServiceWatcherImplMac::GetServiceType() const {
  return service_type_;
}

void ServiceWatcherImplMac::OnServicesUpdate(ServiceWatcher::UpdateType update,
                                             const std::string& service) {
  VLOG(1) << "ServiceWatcherImplMac::OnServicesUpdate: "
          << service + "." + service_type_;
  callback_.Run(update, service + "." + service_type_);
}

ServiceResolverImplMac::NetServiceContainer::NetServiceContainer(
    const std::string& service_name,
    ServiceResolver::ResolveCompleteCallback callback,
    scoped_refptr<base::SingleThreadTaskRunner> service_discovery_runner)
    : service_name_(service_name),
      callback_(std::move(callback)),
      callback_runner_(base::ThreadTaskRunnerHandle::Get()),
      service_discovery_runner_(service_discovery_runner),
      weak_factory_(this) {}

ServiceResolverImplMac::NetServiceContainer::~NetServiceContainer() {
  DCHECK(IsOnServiceDiscoveryThread());

  // Work around a 10.12 bug: NSNetService doesn't lose interest in its weak
  // delegate during deallocation, so a subsequently-deallocated delegate
  // attempts to clear the pointer to itself in an NSNetService that's already
  // gone.
  // https://crbug.com/657495, https://openradar.appspot.com/28943305
  [service_ setDelegate:nil];
}

void ServiceResolverImplMac::NetServiceContainer::StartResolving() {
  service_discovery_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NetServiceContainer::StartResolvingOnDiscoveryThread,
                     weak_factory_.GetWeakPtr()));
}

void ServiceResolverImplMac::NetServiceContainer::DeleteSoon() {
  service_discovery_runner_->DeleteSoon(FROM_HERE, this);
}

void
ServiceResolverImplMac::NetServiceContainer::StartResolvingOnDiscoveryThread() {
  DCHECK(IsOnServiceDiscoveryThread());
  base::scoped_nsobject<NSString> instance, type, domain;

  // The service object is set ahead of time by tests.
  if (service_)
    return;

  if (!ExtractServiceInfo(service_name_, true, &instance, &type, &domain))
    return OnResolveUpdate(ServiceResolver::STATUS_KNOWN_NONEXISTENT);

  VLOG(1) << "ServiceResolverImplMac::ServiceResolverImplMac::"
          << "StartResolvingOnDiscoveryThread: Success";
  delegate_.reset([[NetServiceDelegate alloc] initWithContainer:this]);
  service_.reset(
      [[NSNetService alloc] initWithDomain:domain type:type name:instance]);
  [service_ setDelegate:delegate_];

  [service_ resolveWithTimeout:kResolveTimeout];

  VLOG(1) << "ServiceResolverImplMac::NetServiceContainer::"
          << "StartResolvingOnDiscoveryThread: " << service_name_
          << ", instance: " << [instance UTF8String]
          << ", type: " << [type UTF8String]
          << ", domain: " << [domain UTF8String];
}

void ServiceResolverImplMac::NetServiceContainer::OnResolveUpdate(
    RequestStatus status) {
  if (callback_.is_null())
    return;

  if (status != STATUS_SUCCESS) {
    callback_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_), status, ServiceDescription()));
    return;
  }

  service_description_.service_name = service_name_;

  for (NSData* address in [service_ addresses]) {
    const void* bytes = [address bytes];
    int length = [address length];
    const sockaddr* socket = static_cast<const sockaddr*>(bytes);
    net::IPEndPoint end_point;
    if (end_point.FromSockAddr(socket, length)) {
      service_description_.address =
          net::HostPortPair::FromIPEndPoint(end_point);
      service_description_.ip_address = end_point.address();
      break;
    }
  }

  if (service_description_.address.host().empty()) {
    VLOG(1) << "Service IP is not resolved: " << service_name_;
    callback_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_), STATUS_KNOWN_NONEXISTENT,
                       ServiceDescription()));
    return;
  }

  ParseTxtRecord([service_ TXTRecordData], &service_description_.metadata);

  // TODO(justinlin): Implement last_seen.
  service_description_.last_seen = base::Time::Now();
  callback_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), status, service_description_));
}

void ServiceResolverImplMac::NetServiceContainer::SetServiceForTesting(
    base::scoped_nsobject<NSNetService> service) {
  service_ = service;
}

ServiceResolverImplMac::ServiceResolverImplMac(
    const std::string& service_name,
    ServiceResolver::ResolveCompleteCallback callback,
    scoped_refptr<base::SingleThreadTaskRunner> service_discovery_runner)
    : service_name_(service_name),
      callback_(std::move(callback)),
      has_resolved_(false),
      weak_factory_(this) {
  container_.reset(new NetServiceContainer(
      service_name,
      base::BindOnce(&ServiceResolverImplMac::OnResolveComplete,
                     weak_factory_.GetWeakPtr()),
      service_discovery_runner));
}

ServiceResolverImplMac::~ServiceResolverImplMac() {}

void ServiceResolverImplMac::StartResolving() {
  container_->StartResolving();

  VLOG(1) << "Resolving service " << service_name_;
}

std::string ServiceResolverImplMac::GetName() const { return service_name_; }

void ServiceResolverImplMac::OnResolveComplete(
    RequestStatus status,
    const ServiceDescription& description) {
  VLOG(1) << "ServiceResolverImplMac::OnResolveComplete: " << service_name_
          << ", " << status;

  has_resolved_ = true;

  if (!callback_.is_null())
    std::move(callback_).Run(status, description);
}

ServiceResolverImplMac::NetServiceContainer*
ServiceResolverImplMac::GetContainerForTesting() {
  return container_.get();
}

}  // namespace local_discovery

@implementation NetServiceBrowserDelegate

- (id)initWithContainer:
          (ServiceWatcherImplMac::NetServiceBrowserContainer*)container {
  if ((self = [super init])) {
    container_ = container;
    services_.reset([[NSMutableArray alloc] initWithCapacity:1]);
  }
  return self;
}

- (void)clearDiscoveredServices {
  for (NSNetService* netService in services_.get()) {
    [netService stopMonitoring];
    [netService setDelegate:nil];
  }
  [services_ removeAllObjects];
}

- (void)netServiceBrowser:(NSNetServiceBrowser*)netServiceBrowser
           didFindService:(NSNetService*)netService
               moreComing:(BOOL)moreServicesComing {
  // Start monitoring this service for updates.
  [netService setDelegate:self];
  [netService startMonitoring];
  [services_ addObject:netService];

  container_->OnServicesUpdate(local_discovery::ServiceWatcher::UPDATE_ADDED,
                               base::SysNSStringToUTF8([netService name]));
}

- (void)netServiceBrowser:(NSNetServiceBrowser*)netServiceBrowser
         didRemoveService:(NSNetService*)netService
               moreComing:(BOOL)moreServicesComing {
  NSUInteger index = [services_ indexOfObject:netService];
  if (index != NSNotFound) {
    container_->OnServicesUpdate(
        local_discovery::ServiceWatcher::UPDATE_REMOVED,
        base::SysNSStringToUTF8([netService name]));

    // Stop monitoring this service for updates.
    DCHECK_EQ(netService, [services_ objectAtIndex:index]);
    [netService stopMonitoring];
    [netService setDelegate:nil];
    [services_ removeObjectAtIndex:index];
  }
}

- (void)netService:(NSNetService*)sender
    didUpdateTXTRecordData:(NSData*)data {
  container_->OnServicesUpdate(local_discovery::ServiceWatcher::UPDATE_CHANGED,
                               base::SysNSStringToUTF8([sender name]));
}

@end

@implementation NetServiceDelegate

- (id)initWithContainer:
          (ServiceResolverImplMac::NetServiceContainer*)container {
  if ((self = [super init])) {
    container_ = container;
  }
  return self;
}

- (void)netServiceDidResolveAddress:(NSNetService*)sender {
  container_->OnResolveUpdate(local_discovery::ServiceResolver::STATUS_SUCCESS);
}

- (void)netService:(NSNetService*)sender
        didNotResolve:(NSDictionary*)errorDict {
  container_->OnResolveUpdate(
      local_discovery::ServiceResolver::STATUS_REQUEST_TIMEOUT);
}

@end
