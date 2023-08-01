// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/service_discovery_client_mac.h"

#import <arpa/inet.h>
#import <Foundation/Foundation.h>
#import <net/if_dl.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/mac/foundation_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/strings/sys_string_conversions.h"
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"

using local_discovery::ServiceWatcher;
using local_discovery::ServiceResolver;
using local_discovery::ServiceDescription;

@interface NetServiceBrowser
    : NSObject <NSNetServiceBrowserDelegate, NSNetServiceDelegate>
// Creates a new Browser instance for |serviceType|, which will call
// |callback| on |callbackRunner| when changes are detected. This does NOT
// start listening, as that must be done on the discovery thread via
// -discoverServices.
- (instancetype)initWithServiceType:(const std::string&)serviceType
                           callback:(ServiceWatcher::UpdatedCallback)callback
                     callbackRunner:
                         (scoped_refptr<base::SingleThreadTaskRunner>)
                             callbackRunner;

// Creates a new NSNetServiceBrowser and starts listening for discovery
// notifications.
- (void)discoverServices;

// Stops listening for discovery notifications.
- (void)stop;
@end

@interface NetServiceResolver : NSObject <NSNetServiceDelegate>
// Creates a new resolver instance for service named |name|. Calls the
// |callback| on the |callbackRunner| when done or an error occurs.
- (instancetype)
    initWithServiceName:(const std::string&)name
       resolvedCallback:(ServiceResolver::ResolveCompleteCallback)callback
         callbackRunner:
             (scoped_refptr<base::SingleThreadTaskRunner>)callbackRunner;

// Begins a resolve request for the service.
- (void)resolveService;

// Stops any in-flight resolve operation.
- (void)stop;
@end

namespace local_discovery {

namespace {

const char kServiceDiscoveryThreadName[] = "Service Discovery Thread";

const NSTimeInterval kResolveTimeout = 10.0;

// These functions are used to PostTask with ObjC objects, without needing to
// manage the lifetime of a C++ pointer for either the Watcher or Resolver.
// Clients of those classes can delete the C++ object while operations on the
// ObjC objects are still in flight. Because the ObjC objects are reference
// counted, the strong references passed to these functions ensure the object
// remains alive until for the duration of the operation.

void StartServiceBrowser(NetServiceBrowser* browser) {
  [browser discoverServices];
}

void StopServiceBrowser(NetServiceBrowser* browser) {
  [browser stop];
}

void StartServiceResolver(NetServiceResolver* resolver) {
  [resolver resolveService];
}

void StopServiceResolver(NetServiceResolver* resolver) {
  [resolver stop];
}

// Extracts the instance name, name type and domain from a full service name or
// the service type and domain from a service type. Returns true if successful.
// TODO(justinlin): This current only handles service names with format
// <name>._<protocol2>._<protocol1>.<domain>. Service names with
// subtypes will not parse correctly:
// <name>._<type>._<sub>._<protocol2>._<protocol1>.<domain>.
bool ExtractServiceInfo(const std::string& service,
                        bool is_service_name,
                        NSString** instance,
                        NSString** type,
                        NSString** domain) {
  if (service.empty())
    return false;

  const size_t last_period = service.find_last_of('.');
  if (last_period == std::string::npos || service.length() <= last_period)
    return false;

  if (!is_service_name) {
    *type = base::SysUTF8ToNSString(service.substr(0, last_period) + ".");
  } else {
    // Find third last period that delimits type and instance name.
    size_t type_period = last_period;
    for (int i = 0; i < 2; ++i) {
      type_period = service.find_last_of('.', type_period - 1);
      if (type_period == std::string::npos) {
        return false;
      }
    }

    *instance = base::SysUTF8ToNSString(service.substr(0, type_period));
    *type = base::SysUTF8ToNSString(
        service.substr(type_period + 1, last_period - type_period));
  }
  *domain = base::SysUTF8ToNSString(service.substr(last_period + 1) + ".");

  return [*domain length] > 0 &&
         [*type length] > 0 &&
         (!is_service_name || [*instance length] > 0);
}

void ParseTxtRecord(NSData* record, std::vector<std::string>* output) {
  if (record.length <= 1) {
    return;
  }

  VLOG(1) << "ParseTxtRecord: " << record.length;

  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(record.bytes);
  size_t size = record.length;
  size_t offset = 0;
  while (offset < size) {
    uint8_t record_size = bytes[offset++];
    if (offset > size - record_size)
      break;

    NSString* txt_record =
        [[NSString alloc] initWithBytes:&bytes[offset]
                                 length:record_size
                               encoding:NSUTF8StringEncoding];
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

ServiceDiscoveryClientMac::ServiceDiscoveryClientMac() = default;
ServiceDiscoveryClientMac::~ServiceDiscoveryClientMac() = default;

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
  return nullptr;
}

void ServiceDiscoveryClientMac::StartThreadIfNotStarted() {
  if (!service_discovery_thread_) {
    service_discovery_thread_ =
        std::make_unique<base::Thread>(kServiceDiscoveryThreadName);
    // Only TYPE_UI uses an NSRunLoop.
    base::Thread::Options options(base::MessagePumpType::UI, 0);
    service_discovery_thread_->StartWithOptions(std::move(options));
  }
}

// Service Watcher /////////////////////////////////////////////////////////////

ServiceWatcherImplMac::ServiceWatcherImplMac(
    const std::string& service_type,
    ServiceWatcher::UpdatedCallback callback,
    scoped_refptr<base::SingleThreadTaskRunner> service_discovery_runner)
    : service_type_(service_type),
      callback_(std::move(callback)),
      service_discovery_runner_(service_discovery_runner) {}

ServiceWatcherImplMac::~ServiceWatcherImplMac() {
  service_discovery_runner_->PostTask(
      FROM_HERE, base::BindOnce(&StopServiceBrowser, std::move(browser_)));
}

void ServiceWatcherImplMac::Start() {
  DCHECK(!started_);
  VLOG(1) << "ServiceWatcherImplMac::Start";

  browser_ = [[NetServiceBrowser alloc]
      initWithServiceType:service_type_
                 callback:base::BindRepeating(
                              &ServiceWatcherImplMac::OnServicesUpdate,
                              weak_factory_.GetWeakPtr())
           callbackRunner:base::SingleThreadTaskRunner::GetCurrentDefault()];
  started_ = true;
}

void ServiceWatcherImplMac::DiscoverNewServices() {
  DCHECK(started_);
  VLOG(1) << "ServiceWatcherImplMac::DiscoverNewServices";
  service_discovery_runner_->PostTask(
      FROM_HERE, base::BindOnce(&StartServiceBrowser, browser_));
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

// Service Resolver ////////////////////////////////////////////////////////////

ServiceResolverImplMac::ServiceResolverImplMac(
    const std::string& service_name,
    ServiceResolver::ResolveCompleteCallback callback,
    scoped_refptr<base::SingleThreadTaskRunner> service_discovery_runner)
    : service_name_(service_name),
      callback_(std::move(callback)),
      service_discovery_runner_(service_discovery_runner) {}

ServiceResolverImplMac::~ServiceResolverImplMac() {
  StopResolving();
}

void ServiceResolverImplMac::StartResolving() {
  VLOG(1) << "Resolving service " << service_name_;
  resolver_ = [[NetServiceResolver alloc]
      initWithServiceName:service_name_
         resolvedCallback:base::BindOnce(
                              &ServiceResolverImplMac::OnResolveComplete,
                              weak_factory_.GetWeakPtr())
           callbackRunner:base::SingleThreadTaskRunner::GetCurrentDefault()];
  service_discovery_runner_->PostTask(
      FROM_HERE, base::BindOnce(&StartServiceResolver, resolver_));
}

std::string ServiceResolverImplMac::GetName() const {
  return service_name_;
}

void ServiceResolverImplMac::OnResolveComplete(
    RequestStatus status,
    const ServiceDescription& description) {
  VLOG(1) << "ServiceResolverImplMac::OnResolveComplete: " << service_name_
          << ", " << status;

  has_resolved_ = true;

  StopResolving();

  // The |callback_| can delete this.
  if (!callback_.is_null())
    std::move(callback_).Run(status, description);
}

void ServiceResolverImplMac::StopResolving() {
  service_discovery_runner_->PostTask(
      FROM_HERE, base::BindOnce(&StopServiceResolver, std::move(resolver_)));
}

void ParseNetService(NSNetService* service, ServiceDescription& description) {
  for (NSData* address in [service addresses]) {
    const void* bytes = [address bytes];
    int length = [address length];
    const sockaddr* socket = static_cast<const sockaddr*>(bytes);
    net::IPEndPoint end_point;
    if (end_point.FromSockAddr(socket, length)) {
      description.address = net::HostPortPair::FromIPEndPoint(end_point);
      description.ip_address = end_point.address();
      break;
    }
  }

  ParseTxtRecord([service TXTRecordData], &description.metadata);
}

}  // namespace local_discovery

// Service Watcher /////////////////////////////////////////////////////////////

@implementation NetServiceBrowser {
  std::string _serviceType;

  ServiceWatcher::UpdatedCallback _callback;
  scoped_refptr<base::SingleThreadTaskRunner> _callbackRunner;

  NSNetServiceBrowser* __strong _browser;
  NSMutableArray<NSNetService*>* __strong _services;
}

- (instancetype)initWithServiceType:(const std::string&)serviceType
                           callback:(ServiceWatcher::UpdatedCallback)callback
                     callbackRunner:
                         (scoped_refptr<base::SingleThreadTaskRunner>)
                             callbackRunner {
  if ((self = [super init])) {
    _serviceType = serviceType;

    _callback = std::move(callback);
    _callbackRunner = callbackRunner;

    _services = [[NSMutableArray alloc] initWithCapacity:1];
  }
  return self;
}

- (void)dealloc {
  [self stop];
}

- (void)discoverServices {
  if (!_browser) {
    _browser = [[NSNetServiceBrowser alloc] init];
    [_browser setDelegate:self];
  }

  NSString* instance;
  NSString* type;
  NSString* domain;
  if (!local_discovery::ExtractServiceInfo(_serviceType, false, &instance,
                                           &type, &domain)) {
    return;
  }

  DCHECK(![instance length]);
  DVLOG(1) << "Listening for service type '" << type << "' on domain '"
           << domain << "'";

  [_browser searchForServicesOfType:type inDomain:domain];
}

- (void)stop {
  [_browser stop];

  // Work around a 10.12 bug: NSNetServiceBrowser doesn't lose interest in its
  // weak delegate during deallocation, so a subsequently-deallocated delegate
  // attempts to clear the pointer to itself in an NSNetServiceBrowser that's
  // already gone.
  // https://crbug.com/657495, https://openradar.appspot.com/28943305
  _browser.delegate = nil;

  // Ensure the delegate clears all references to itself, which it had added as
  // discovered services were reported to it.
  for (NSNetService* netService in _services) {
    [netService stopMonitoring];
    [netService setDelegate:nil];
  }
  [_services removeAllObjects];

  _browser = nil;
}

- (void)netServiceBrowser:(NSNetServiceBrowser*)netServiceBrowser
           didFindService:(NSNetService*)netService
               moreComing:(BOOL)moreServicesComing {
  [netService setDelegate:self];
  [netService startMonitoring];
  [_services addObject:netService];

  _callbackRunner->PostTask(
      FROM_HERE, base::BindOnce(_callback, ServiceWatcher::UPDATE_ADDED,
                                base::SysNSStringToUTF8([netService name])));
}

- (void)netServiceBrowser:(NSNetServiceBrowser*)netServiceBrowser
         didRemoveService:(NSNetService*)netService
               moreComing:(BOOL)moreServicesComing {
  NSUInteger index = [_services indexOfObject:netService];
  if (index != NSNotFound) {
    _callbackRunner->PostTask(
        FROM_HERE, base::BindOnce(_callback, ServiceWatcher::UPDATE_REMOVED,
                                  base::SysNSStringToUTF8([netService name])));

    // Stop monitoring this service for updates. The |netService| object may be
    // different than the one stored in |_services|, even though they represent
    // the same service. Stop monitoring and clear the delegate on both.
    [netService stopMonitoring];
    [netService setDelegate:nil];

    netService = _services[index];
    [netService stopMonitoring];
    [netService setDelegate:nil];

    [_services removeObjectAtIndex:index];
  }
}

- (void)netService:(NSNetService*)sender
    didUpdateTXTRecordData:(NSData*)data {
  _callbackRunner->PostTask(
      FROM_HERE, base::BindOnce(_callback, ServiceWatcher::UPDATE_CHANGED,
                                base::SysNSStringToUTF8([sender name])));
}

@end

// Service Resolver ////////////////////////////////////////////////////////////

@implementation NetServiceResolver {
  std::string _serviceName;

  ServiceResolver::ResolveCompleteCallback _callback;
  scoped_refptr<base::SingleThreadTaskRunner> _callbackRunner;

  ServiceDescription _serviceDescription;
  NSNetService* __strong _service;
}

- (instancetype)
    initWithServiceName:(const std::string&)serviceName
       resolvedCallback:(ServiceResolver::ResolveCompleteCallback)callback
         callbackRunner:
             (scoped_refptr<base::SingleThreadTaskRunner>)callbackRunner {
  if ((self = [super init])) {
    _serviceName = serviceName;
    _callback = std::move(callback);
    _callbackRunner = callbackRunner;
  }
  return self;
}

- (void)dealloc {
  [self stop];
}

- (void)resolveService {
  NSString* instance;
  NSString* type;
  NSString* domain;
  if (!local_discovery::ExtractServiceInfo(_serviceName, true, &instance, &type,
                                           &domain)) {
    [self updateServiceDescription:ServiceResolver::STATUS_KNOWN_NONEXISTENT];
    return;
  }

  VLOG(1) << "-[ServiceResolver resolveService] " << _serviceName
          << ", instance: " << instance << ", type: " << type
          << ", domain: " << domain;

  _service = [[NSNetService alloc] initWithDomain:domain
                                             type:type
                                             name:instance];
  [_service setDelegate:self];
  [_service resolveWithTimeout:local_discovery::kResolveTimeout];
}

- (void)stop {
  [_service stop];

  // Work around a 10.12 bug: NSNetService doesn't lose interest in its weak
  // delegate during deallocation, so a subsequently-deallocated delegate
  // attempts to clear the pointer to itself in an NSNetService that's already
  // gone.
  // https://crbug.com/657495, https://openradar.appspot.com/28943305
  _service.delegate = nil;
  _service = nil;
}

- (void)netServiceDidResolveAddress:(NSNetService*)sender {
  [self updateServiceDescription:ServiceResolver::STATUS_SUCCESS];
}

- (void)netService:(NSNetService*)sender
        didNotResolve:(NSDictionary*)errorDict {
  [self updateServiceDescription:ServiceResolver::STATUS_REQUEST_TIMEOUT];
}

- (void)updateServiceDescription:(ServiceResolver::RequestStatus)status {
  if (_callback.is_null())
    return;

  if (status != ServiceResolver::STATUS_SUCCESS) {
    _callbackRunner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(_callback), status, ServiceDescription()));
    return;
  }

  _serviceDescription.service_name = _serviceName;
  ParseNetService(_service, _serviceDescription);

  if (_serviceDescription.address.host().empty()) {
    VLOG(1) << "Service IP is not resolved: " << _serviceName;
    _callbackRunner->PostTask(
        FROM_HERE, base::BindOnce(std::move(_callback),
                                  ServiceResolver::STATUS_KNOWN_NONEXISTENT,
                                  ServiceDescription()));
    return;
  }

  // TODO(justinlin): Implement last_seen.
  _serviceDescription.last_seen = base::Time::Now();
  _callbackRunner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(_callback), status, _serviceDescription));
}

@end
