// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/service_discovery_device_lister.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"

namespace local_discovery {

namespace {

class ServiceDiscoveryDeviceListerImpl : public ServiceDiscoveryDeviceLister {
 public:
  ServiceDiscoveryDeviceListerImpl(
      Delegate* delegate,
      ServiceDiscoveryClient* service_discovery_client,
      const std::string& service_type)
      : delegate_(delegate),
        service_discovery_client_(service_discovery_client),
        service_type_(service_type) {}

  ~ServiceDiscoveryDeviceListerImpl() override = default;

  // ServiceDiscoveryDeviceLister implementation.
  void Start() override {
    VLOG(1) << "DeviceListerStart: service_type: " << service_type_;
    CreateServiceWatcher();
  }

  // ServiceDiscoveryDeviceLister implementation.
  void DiscoverNewDevices() override {
    VLOG(1) << "DiscoverNewDevices: service_type: " << service_type_;
    service_watcher_->DiscoverNewServices();
  }

  const std::string& service_type() const override { return service_type_; }

 private:
  using ServiceResolverMap =
      std::map<std::string, std::unique_ptr<ServiceResolver>>;

  void OnServiceUpdated(ServiceWatcher::UpdateType update,
                        const std::string& service_name) {
    VLOG(1) << "OnServiceUpdated: service_type: " << service_type_
            << ", service_name: " << service_name << ", update: " << update;

    if (update == ServiceWatcher::UPDATE_PERMISSION_REJECTED) {
      resolvers_.clear();
      delegate_->OnPermissionRejected();
      return;
    }

    if (update == ServiceWatcher::UPDATE_INVALIDATED) {
      resolvers_.clear();
      CreateServiceWatcher();

      delegate_->OnDeviceCacheFlushed(service_type_);
      return;
    }

    if (update == ServiceWatcher::UPDATE_REMOVED) {
      delegate_->OnDeviceRemoved(service_type_, service_name);
      return;
    }

    // If there is already a resolver working on this service, don't add one.
    if (base::Contains(resolvers_, service_name)) {
      VLOG(1) << "Resolver already exists, service_name: " << service_name;
      return;
    }

    VLOG(1) << "Adding resolver for service_name: " << service_name;
    bool added = (update == ServiceWatcher::UPDATE_ADDED);
    std::unique_ptr<ServiceResolver> resolver =
        service_discovery_client_->CreateServiceResolver(
            service_name,
            base::BindOnce(&ServiceDiscoveryDeviceListerImpl::OnResolveComplete,
                           weak_factory_.GetWeakPtr(), added, service_name));
    resolver->StartResolving();
    resolvers_[service_name] = std::move(resolver);
  }

  // TODO(noamsml): Update ServiceDiscoveryClient interface to match this.
  void OnResolveComplete(bool added,
                         std::string service_name,
                         ServiceResolver::RequestStatus status,
                         const ServiceDescription& service_description) {
    VLOG(1) << "OnResolveComplete: service_type: " << service_type_
            << ", service_name: " << service_name << ", status: " << status;
    if (status == ServiceResolver::STATUS_SUCCESS) {
      delegate_->OnDeviceChanged(service_type_, added, service_description);
    } else {
      // TODO(noamsml): Add retry logic.
    }
    resolvers_.erase(service_name);
  }

  // Create or recreate the service watcher
  void CreateServiceWatcher() {
    service_watcher_ = service_discovery_client_->CreateServiceWatcher(
        service_type_,
        base::BindRepeating(&ServiceDiscoveryDeviceListerImpl::OnServiceUpdated,
                            weak_factory_.GetWeakPtr()));
    service_watcher_->Start();
    service_watcher_->SetActivelyRefreshServices(true);
  }

  const raw_ptr<Delegate> delegate_;
  const raw_ptr<ServiceDiscoveryClient> service_discovery_client_;
  const std::string service_type_;

  std::unique_ptr<ServiceWatcher> service_watcher_;
  ServiceResolverMap resolvers_;

  base::WeakPtrFactory<ServiceDiscoveryDeviceListerImpl> weak_factory_{this};
};
}  // namespace

// static
std::unique_ptr<ServiceDiscoveryDeviceLister>
ServiceDiscoveryDeviceLister::Create(
    Delegate* delegate,
    ServiceDiscoveryClient* service_discovery_client,
    const std::string& service_type) {
  return std::make_unique<ServiceDiscoveryDeviceListerImpl>(
      delegate, service_discovery_client, service_type);
}

}  // namespace local_discovery
