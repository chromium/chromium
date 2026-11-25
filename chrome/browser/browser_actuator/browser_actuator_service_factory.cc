// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/browser_actuator_service_factory.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/stream_transport/transport_service_factory.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "components/browser_actuator/internal/browser_actuator_service_impl.h"
#include "components/browser_actuator/internal/browser_actuator_task_client_impl.h"
#include "components/browser_actuator/internal/task_ingress_transport_handler_factory.h"
#include "components/browser_actuator/public/browser_actuator_service.h"
#include "components/browser_actuator/public/browser_actuator_task_client.h"
#include "components/browser_actuator/public/features.h"
#include "components/stream_transport/public/transport_channel.h"
#include "components/stream_transport/public/transport_handler_factory_registry.h"
#include "components/stream_transport/public/transport_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "content/public/browser/browser_context.h"

namespace browser_actuator {
namespace {

std::string GetDeviceGuid(base::WeakPtr<Profile> profile) {
  if (!profile) {
    return std::string();
  }
  syncer::DeviceInfoSyncService* sync =
      DeviceInfoSyncServiceFactory::GetForProfile(profile.get());
  if (!sync || !sync->GetLocalDeviceInfoProvider()) {
    return std::string();
  }
  const syncer::DeviceInfo* info =
      sync->GetLocalDeviceInfoProvider()->GetLocalDeviceInfo();
  return info ? info->guid() : std::string();
}

// Owns the profile's `TaskIngressTransportHandlerFactory` and manages its
// registration with `TransportChannel` for the lifetime of the task client
// factory callback held by `BrowserActuatorServiceImpl`. When
// `BrowserActuatorServiceImpl::Shutdown()` resets the callback, this owner is
// destroyed before `TransportService` shuts down.
class ScopedTaskIngressHandlerFactory {
 public:
  explicit ScopedTaskIngressHandlerFactory(Profile* profile);
  ScopedTaskIngressHandlerFactory(const ScopedTaskIngressHandlerFactory&) =
      delete;
  ScopedTaskIngressHandlerFactory& operator=(
      const ScopedTaskIngressHandlerFactory&) = delete;
  ~ScopedTaskIngressHandlerFactory();

  std::unique_ptr<BrowserActuatorTaskClient> CreateTaskClient();

 private:
  stream_transport::TransportService* GetTransportService();

  base::WeakPtr<Profile> profile_;
  TaskIngressTransportHandlerFactory handler_factory_;
};

ScopedTaskIngressHandlerFactory::ScopedTaskIngressHandlerFactory(
    Profile* profile)
    : profile_(profile ? profile->GetWeakPtr() : nullptr) {
  if (stream_transport::TransportService* transport_service =
          GetTransportService();
      transport_service && transport_service->GetChannel()) {
    transport_service->GetChannel()
        ->GetHandlerFactoryRegistry()
        ->RegisterFactory(&handler_factory_);
  }
}

ScopedTaskIngressHandlerFactory::~ScopedTaskIngressHandlerFactory() {
  handler_factory_.DetachAllClients();
  if (stream_transport::TransportService* transport_service =
          GetTransportService();
      transport_service && transport_service->GetChannel()) {
    transport_service->GetChannel()
        ->GetHandlerFactoryRegistry()
        ->UnregisterFactory(&handler_factory_);
  }
}

std::unique_ptr<BrowserActuatorTaskClient>
ScopedTaskIngressHandlerFactory::CreateTaskClient() {
  if (!profile_) {
    return nullptr;
  }
  stream_transport::TransportService* transport_service = GetTransportService();
  if (!transport_service) {
    return nullptr;
  }
  return std::make_unique<BrowserActuatorTaskClientImpl>(
      transport_service, handler_factory_.GetWeakPtr(),
      base::BindRepeating(&GetDeviceGuid, profile_));
}

stream_transport::TransportService*
ScopedTaskIngressHandlerFactory::GetTransportService() {
  return profile_ ? stream_transport::TransportServiceFactory::GetForProfile(
                        profile_.get())
                  : nullptr;
}

}  // namespace

// static
BrowserActuatorServiceFactory* BrowserActuatorServiceFactory::GetInstance() {
  static base::NoDestructor<BrowserActuatorServiceFactory> instance;
  return instance.get();
}

// static
BrowserActuatorService* BrowserActuatorServiceFactory::GetForProfile(
    Profile* profile) {
  CHECK(profile);
  return static_cast<BrowserActuatorService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

BrowserActuatorServiceFactory::BrowserActuatorServiceFactory()
    : ProfileKeyedServiceFactory("BrowserActuatorService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(stream_transport::TransportServiceFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
}

BrowserActuatorServiceFactory::~BrowserActuatorServiceFactory() = default;

std::unique_ptr<KeyedService>
BrowserActuatorServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(kBrowserActuator)) {
    return nullptr;
  }

  auto service = std::make_unique<BrowserActuatorServiceImpl>();
  if (!base::FeatureList::IsEnabled(kBrowserActuatorTaskIngress)) {
    return service;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  service->SetTaskClientFactory(base::BindRepeating(
      &ScopedTaskIngressHandlerFactory::CreateTaskClient,
      std::make_unique<ScopedTaskIngressHandlerFactory>(profile)));
  return service;
}

}  // namespace browser_actuator
