// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_FAKE_SERVICE_DISCOVERY_DEVICE_LISTER_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_FAKE_SERVICE_DISCOVERY_DEVICE_LISTER_H_

#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/task_runner.h"
#include "chrome/browser/local_discovery/service_discovery_client.h"
#include "chrome/browser/local_discovery/service_discovery_device_lister.h"

namespace local_discovery {

// This is a thin wrapper around Delegate that defers callbacks until the actual
// delegate is initialized and then calls all deferred callbacks. Once the
// actual delegate is initialized, this just becomes a simple passthrough.
class DeferringDelegate : public ServiceDiscoveryDeviceLister::Delegate {
 public:
  DeferringDelegate();
  ~DeferringDelegate();

  // ServiceDiscoveryDeviceLister::Delegate:
  void OnDeviceChanged(const std::string& service_type,
                       bool added,
                       const ServiceDescription& service_description) override;
  void OnDeviceRemoved(const std::string& service_type,
                       const std::string& service_name) override;
  void OnDeviceCacheFlushed(const std::string& service_type) override;
  void OnPermissionRejected() override {}

  // Sets the delegate that callbacks should be called on.
  void SetActual(ServiceDiscoveryDeviceLister::Delegate* actual);

 private:
  std::vector<base::OnceCallback<void()>> deferred_callbacks_;
  raw_ptr<ServiceDiscoveryDeviceLister::Delegate> actual_ = nullptr;
};

// A fake ServiceDiscoveryDeviceLister. This provides an implementation of
// ServiceDiscoveryDeviceLister that tests can use to trigger the addition and
// removal of devices.
//
// There's some hackery here to handle constructor order constraints. There's a
// circular dependency in that device lister delegate implementations need their
// device lister set to be supplied at construction time, and each device lister
// needs to know about its delegate for callbacks. Thus, a DeferringDelegate is
// used to queue callbacks triggered before the class has the delegate
// reference, and those queued callbacks are invoked when the delegate is set.
class FakeServiceDiscoveryDeviceLister final
    : public ServiceDiscoveryDeviceLister {
 public:
  FakeServiceDiscoveryDeviceLister(base::TaskRunner* task_runner,
                                   const std::string& service_type);
  ~FakeServiceDiscoveryDeviceLister() override;

  // ServiceDiscoveryDeviceLister:
  void Start() override;
  void DiscoverNewDevices() override;
  const std::string& service_type() const override;

  // Sets the delegate of this lister.
  void SetDelegate(ServiceDiscoveryDeviceLister::Delegate* delegate);

  // Announces a new service or updates it if it's been seen before and already
  // announced. If discovery hasn't started yet, the description is queued to be
  // sent when discovery is started. If the description's service type does not
  // match the lister's service type, the service is not announced.
  void Announce(const ServiceDescription& description);

  // Removes the service specified by |name|.
  void Remove(const std::string& name);

  // Simulates an event that clears downstream caches and the lister.
  void Clear();

  // Indicates whether discovery has started.
  bool discovery_started();

 private:
  // Helper function that sends an update to the delegate via the
  // OnDeviceChanged callback.
  void SendUpdate(const ServiceDescription& description);

  // Used to post tasks for the delegate callbacks.
  raw_ptr<base::TaskRunner> task_runner_;

  // Services which have previously posted an update and therefore are no
  // longer 'new' for the purposes of the OnDeviceChanged callback.
  std::set<std::string> announced_services_;

  // Updates added to the class before discovery started.
  std::vector<ServiceDescription> queued_updates_;

  // Has Start() been called?
  bool start_called_ = false;

  // Has DiscoverNewDevices() been called?
  bool discovery_started_ = false;

  // The service type of this lister.
  std::string service_type_;

  // The delegate of this lister.
  DeferringDelegate deferring_delegate_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_FAKE_SERVICE_DISCOVERY_DEVICE_LISTER_H_
