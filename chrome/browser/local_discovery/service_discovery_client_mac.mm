// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/local_discovery/service_discovery_client_mac.h"

#include <Foundation/Foundation.h>
#include <Network/Network.h>
#include <arpa/inet.h>
#include <net/if_dl.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/apple/foundation_util.h"
#include "base/functional/bind.h"
#include "base/mac/mac_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chrome/browser/local_discovery/service_discovery_client_mac_util.h"
#include "chrome/browser/media/router/media_router_feature.h"
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

// Duration of time to wait for the users to responds to the permission dialog
// before the permission state metric is recorded.
constexpr base::TimeDelta kPermissionsMetricsDelay = base::Seconds(60);

void SetUpServiceBrowser(
    nw_browser_t browser,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    ServiceWatcher::UpdatedCallback services_update_callback,
    base::RepeatingCallback<void(bool)> metrics_callback) {
  nw_browser_set_queue(browser, dispatch_get_main_queue());

  nw_browser_set_browse_results_changed_handler(
      browser, ^(nw_browse_result_t old_result, nw_browse_result_t new_result,
                 bool batch_complete) {
        nw_browse_result_change_t change =
            nw_browse_result_get_changes(old_result, new_result);
        nw_endpoint_t new_endpoint = nw_browse_result_copy_endpoint(new_result);
        nw_endpoint_t old_endpoint = nw_browse_result_copy_endpoint(old_result);
        const char* new_service_name =
            nw_endpoint_get_bonjour_service_name(new_endpoint);
        const char* old_service_name =
            nw_endpoint_get_bonjour_service_name(old_endpoint);

        switch (change) {
          case nw_browse_result_change_result_added:
            CHECK(new_service_name);
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(services_update_callback,
                               ServiceWatcher::UpdateType::UPDATE_ADDED,
                               std::string(new_service_name)));
            break;
          case nw_browse_result_change_result_removed:
            CHECK(old_service_name);
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(services_update_callback,
                               ServiceWatcher::UpdateType::UPDATE_REMOVED,
                               std::string(old_service_name)));
            break;
          case nw_browse_result_change_txt_record_changed:
            CHECK(new_service_name);
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(services_update_callback,
                               ServiceWatcher::UpdateType::UPDATE_CHANGED,
                               std::string(new_service_name)));
            break;
          default:
            break;
        }
      });

  // Local Network Permission is available on macOS 15 or later.
  if (base::mac::MacOSMajorVersion() < 15) {
    return;
  }

  nw_browser_set_state_changed_handler(browser, ^(nw_browser_state_t state,
                                                  nw_error_t error) {
    // nw_browser always starts in the 'ready' state, but this doesn't mean
    // permission is granted.
    // Permission granted -> no change in the browser state.
    // Permission denied -> transitions to the 'waiting' state with an error.
    // Permission pending -> no change in the browser state.
    // We use a delayed task to handle this: If permission is
    // denied before the task runs, it's a no-op (permission is recorded once
    // per session). Otherwise, we record the permission as granted. Note that
    // it's possible that users deny the permission after the timer expires so
    // there will be a few false positives.
    switch (state) {
      case nw_browser_state_ready:
        task_runner->PostDelayedTask(FROM_HERE,
                                     base::BindOnce(metrics_callback, true),
                                     kPermissionsMetricsDelay);
        break;
      case nw_browser_state_waiting:
        if (nw_error_get_error_code(error) == kDNSServiceErr_PolicyDenied) {
          task_runner->PostTask(
              FROM_HERE,
              base::BindOnce(
                  services_update_callback,
                  ServiceWatcher::UpdateType::UPDATE_PERMISSION_REJECTED, ""));
          metrics_callback.Run(/*permission_granted*/ false);
        }
        break;
      default:
        break;
    }
  });
}

// These functions are used to PostTask with ObjC objects, without needing to
// manage the lifetime of a C++ pointer for either the Watcher or Resolver.
// Clients of those classes can delete the C++ object while operations on the
// ObjC objects are still in flight. Because the ObjC objects are reference
// counted, the strong references passed to these functions ensure the object
// remains alive until for the duration of the operation.

void StartServiceBrowser(
    nw_browser_t browser,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(task_runner->RunsTasksInCurrentSequence());
  nw_browser_start(browser);
}
void StopServiceBrowser(
    nw_browser_t browser,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(task_runner->RunsTasksInCurrentSequence());
  nw_browser_cancel(browser);
}

// DEPRECATED.
void StartNetServiceBrowser(NetServiceBrowser* browser) {
  [browser discoverServices];
}

// DEPRECATED.
void StopNetServiceBrowser(NetServiceBrowser* browser) {
  [browser stop];
}

void StartServiceResolver(NetServiceResolver* resolver) {
  [resolver resolveService];
}

void StopServiceResolver(NetServiceResolver* resolver) {
  [resolver stop];
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
  if (base::FeatureList::IsEnabled(
          media_router::kUseNetworkFrameworkForLocalDiscovery)) {
    service_discovery_runner_->PostTask(
        FROM_HERE, base::BindOnce(&StopServiceBrowser, nw_browser_,
                                  service_discovery_runner_));
    nw_browser_ = nil;
  } else {
    service_discovery_runner_->PostTask(
        FROM_HERE, base::BindOnce(&StopNetServiceBrowser, browser_));
    browser_ = nil;
  }
}

void ServiceWatcherImplMac::Start() {
  DCHECK(!started_);
  VLOG(1) << "ServiceWatcherImplMac::Start";

  if (base::FeatureList::IsEnabled(
          media_router::kUseNetworkFrameworkForLocalDiscovery)) {
    std::optional<local_discovery::ServiceInfo> service_info =
        local_discovery::ExtractServiceInfo(service_type_, false);
    if (!service_info) {
      VLOG(1) << "Failed to start discovery. Invalid service_type: '"
              << service_type_ << "'";
      return;
    }
    VLOG(1) << "Listening for service" << service_info.value();

    nw_browse_descriptor_t descriptor =
        nw_browse_descriptor_create_bonjour_service(
            service_info->service_type.c_str(), service_info->domain.c_str());
    nw_parameters_t parameters = nw_parameters_create_secure_tcp(
        NW_PARAMETERS_DISABLE_PROTOCOL, NW_PARAMETERS_DEFAULT_CONFIGURATION);
    nw_browser_ = nw_browser_create(descriptor, parameters);

    SetUpServiceBrowser(
        nw_browser_, base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::BindRepeating(&ServiceWatcherImplMac::OnServicesUpdate,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(&ServiceWatcherImplMac::RecordPermissionState,
                            weak_factory_.GetWeakPtr()));
  } else {
    browser_ = [[NetServiceBrowser alloc]
        initWithServiceType:service_type_
                   callback:base::BindRepeating(
                                &ServiceWatcherImplMac::OnServicesUpdate,
                                weak_factory_.GetWeakPtr())
             callbackRunner:base::SingleThreadTaskRunner::GetCurrentDefault()];
  }
  started_ = true;
}

void ServiceWatcherImplMac::DiscoverNewServices() {
  DCHECK(started_);
  VLOG(1) << "ServiceWatcherImplMac::DiscoverNewServices";
  if (base::FeatureList::IsEnabled(
          media_router::kUseNetworkFrameworkForLocalDiscovery)) {
    service_discovery_runner_->PostTask(
        FROM_HERE, base::BindOnce(&StartServiceBrowser, nw_browser_,
                                  service_discovery_runner_));
  } else {
    service_discovery_runner_->PostTask(
        FROM_HERE, base::BindOnce(&StartNetServiceBrowser, browser_));
  }
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

void ServiceWatcherImplMac::RecordPermissionState(bool permission_granted) {
  static bool permission_state_recorded_ = false;
  if (permission_state_recorded_) {
    return;
  }
  base::UmaHistogramBoolean(
      "MediaRouter.Discovery.LocalNetworkAccessPermissionGranted",
      permission_granted);
  permission_state_recorded_ = true;
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

  std::optional<local_discovery::ServiceInfo> service_info =
      local_discovery::ExtractServiceInfo(_serviceType, false);
  if (!service_info) {
    VLOG(1) << "Failed to start discovery. Invalid service_type: '"
            << _serviceType << "'";
    return;
  }
  VLOG(1) << "Listening for " << service_info.value();

  NSString* service_type = base::SysUTF8ToNSString(service_info->service_type);
  NSString* domain = base::SysUTF8ToNSString(service_info->domain);

  [_browser searchForServicesOfType:service_type inDomain:domain];
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
  std::optional<local_discovery::ServiceInfo> service_info =
      local_discovery::ExtractServiceInfo(_serviceName, true);

  if (!service_info) {
    VLOG(1) << "Failed to resolve service. Invalid service name:'"
            << _serviceName << "'";
    [self updateServiceDescription:ServiceResolver::STATUS_KNOWN_NONEXISTENT];
    return;
  }
  VLOG(1) << "-[ServiceResolver resolveService] " << _serviceName << ", "
          << service_info.value();

  CHECK(service_info->instance);
  NSString* instance = base::SysUTF8ToNSString(service_info->instance.value());
  NSString* type = base::SysUTF8ToNSString(service_info->service_type);
  NSString* domain = base::SysUTF8ToNSString(service_info->domain);
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
