// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_registry.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "chrome/browser/media/router/discovery/dial/dial_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"

using base::Time;
using base::TimeDelta;
using content::BrowserThread;

namespace {

// How often to poll for devices.
const int kDialRefreshIntervalSecs = 120;

// We prune a device if it does not respond after this time.
const int kDialExpirationSecs = 240;

// The maximum number of devices retained at once in the registry.
const size_t kDialMaxDevices = 256;

}  // namespace

namespace media_router {

DialRegistry::DialRegistry()
    : num_listeners_(0),
      registry_generation_(0),
      last_event_registry_generation_(0),
      label_count_(0),
      refresh_interval_delta_(TimeDelta::FromSeconds(kDialRefreshIntervalSecs)),
      expiration_delta_(TimeDelta::FromSeconds(kDialExpirationSecs)),
      max_devices_(kDialMaxDevices),
      clock_(base::DefaultClock::GetInstance()) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GT(max_devices_, 0U);
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&content::GetNetworkConnectionTracker),
      base::BindOnce(&DialRegistry::SetNetworkConnectionTracker,
                     base::Unretained(this)));
}

// This is a leaky singleton and the dtor won't be called.
DialRegistry::~DialRegistry() = default;

// static
DialRegistry* DialRegistry::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return base::Singleton<DialRegistry,
                         base::LeakySingletonTraits<DialRegistry>>::get();
}

void DialRegistry::SetNetworkConnectionTracker(
    network::NetworkConnectionTracker* tracker) {
  network_connection_tracker_ = tracker;
  network_connection_tracker_->AddLeakyNetworkConnectionObserver(this);
  // If there are no observers yet, it won't actually start.
  StartPeriodicDiscovery();
}

void DialRegistry::SetNetLog(net::NetLog* net_log) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!net_log_)
    net_log_ = net_log;
}

std::unique_ptr<DialService> DialRegistry::CreateDialService() {
  return std::make_unique<DialServiceImpl>(net_log_);
}

void DialRegistry::ClearDialService() {
  dial_.reset();
}

void DialRegistry::OnListenerAdded() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (++num_listeners_ == 1) {
    VLOG(2) << "Listener added; starting periodic discovery.";
    StartPeriodicDiscovery();
  }
  // Event listeners with the current device list.
  // TODO(crbug.com/576817): Rework the DIAL API so we don't need to have extra
  // behaviors when adding listeners.
  SendEvent();
}

void DialRegistry::OnListenerRemoved() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GT(num_listeners_, 0);
  if (--num_listeners_ == 0) {
    VLOG(2) << "Listeners removed; stopping periodic discovery.";
    StopPeriodicDiscovery();
  }
}

void DialRegistry::RegisterObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  observers_.AddObserver(observer);
}

void DialRegistry::UnregisterObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  observers_.RemoveObserver(observer);
}

GURL DialRegistry::GetDeviceDescriptionURL(const std::string& label) const {
  const auto device_it = device_by_label_map_.find(label);
  if (device_it != device_by_label_map_.end())
    return device_it->second->device_description_url();

  return GURL();
}

void DialRegistry::AddDeviceForTest(const DialDeviceData& device_data) {
  std::unique_ptr<DialDeviceData> test_data =
      std::make_unique<DialDeviceData>(device_data);
  device_by_label_map_.insert(
      std::make_pair(device_data.label(), test_data.get()));
  device_by_id_map_.insert(
      std::make_pair(device_data.device_id(), std::move(test_data)));
}

void DialRegistry::SetClockForTest(base::Clock* clock) {
  clock_ = clock;
}

bool DialRegistry::ReadyToDiscover() {
  if (num_listeners_ == 0) {
    OnDialError(DIAL_NO_LISTENERS);
    return false;
  }
  network::mojom::ConnectionType type;
  if (!network_connection_tracker_ ||
      !network_connection_tracker_->GetConnectionType(
          &type, base::BindOnce(&DialRegistry::OnConnectionChanged,
                                base::Unretained(this)))) {
    // If the ConnectionType is unknown, return false. We'll try to start
    // discovery again when we receive the OnConnectionChanged callback.
    OnDialError(DIAL_UNKNOWN);
    return false;
  }
  if (type == network::mojom::ConnectionType::CONNECTION_NONE) {
    OnDialError(DIAL_NETWORK_DISCONNECTED);
    return false;
  }
  if (network::NetworkConnectionTracker::IsConnectionCellular(type)) {
    OnDialError(DIAL_CELLULAR_NETWORK);
    return false;
  }
  return true;
}

bool DialRegistry::DiscoverNow() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!ReadyToDiscover()) {
    return false;
  }
  if (!dial_) {
    OnDialError(DIAL_UNKNOWN);
    return false;
  }

  if (!dial_->HasObserver(this))
    NOTREACHED() << "DiscoverNow() called without observing dial_";

  // Force increment |registry_generation_| to ensure an event is sent even if
  // the device list did not change.
  bool started = dial_->Discover();
  if (started)
    ++registry_generation_;

  return started;
}

void DialRegistry::StartPeriodicDiscovery() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!ReadyToDiscover() || dial_)
    return;

  dial_ = CreateDialService();
  dial_->AddObserver(this);
  DoDiscovery();
  repeating_timer_.reset(new base::RepeatingTimer());
  repeating_timer_->Start(FROM_HERE, refresh_interval_delta_, this,
                          &DialRegistry::DoDiscovery);
}

void DialRegistry::DoDiscovery() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(dial_);
  VLOG(2) << "About to discover!";
  dial_->Discover();
}

void DialRegistry::StopPeriodicDiscovery() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!dial_)
    return;

  repeating_timer_->Stop();
  repeating_timer_.reset();
  dial_->RemoveObserver(this);
  ClearDialService();
}

bool DialRegistry::PruneExpiredDevices() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bool pruned_device = false;
  auto it = device_by_label_map_.begin();
  while (it != device_by_label_map_.end()) {
    auto* device = it->second;
    if (IsDeviceExpired(*device)) {
      VLOG(2) << "Device " << device->label() << " expired, removing";

      // Make a copy of the device ID here since |device| will be destroyed
      // during erase().
      std::string device_id = device->device_id();
      const size_t num_erased_by_id = device_by_id_map_.erase(device_id);
      DCHECK_EQ(1U, num_erased_by_id);
      device_by_label_map_.erase(it++);
      pruned_device = true;
    } else {
      ++it;
    }
  }
  return pruned_device;
}

bool DialRegistry::IsDeviceExpired(const DialDeviceData& device) const {
  Time now = clock_->Now();

  // Check against our default expiration timeout.
  Time default_expiration_time = device.response_time() + expiration_delta_;
  if (now > default_expiration_time)
    return true;

  // Check against the device's cache-control header, if set.
  if (device.has_max_age()) {
    Time max_age_expiration_time =
        device.response_time() + TimeDelta::FromSeconds(device.max_age());
    if (now > max_age_expiration_time)
      return true;
  }
  return false;
}

void DialRegistry::Clear() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  device_by_id_map_.clear();
  device_by_label_map_.clear();
  registry_generation_++;
}

void DialRegistry::MaybeSendEvent() {
  // Send an event if the device list has changed since the last event.
  bool needs_event = last_event_registry_generation_ < registry_generation_;
  VLOG(2) << "lerg = " << last_event_registry_generation_
          << ", rg = " << registry_generation_
          << ", needs_event = " << needs_event;
  if (needs_event)
    SendEvent();
}

void DialRegistry::SendEvent() {
  DeviceList device_list;
  for (DeviceByLabelMap::const_iterator it = device_by_label_map_.begin();
       it != device_by_label_map_.end(); ++it) {
    device_list.push_back(*(it->second));
  }
  OnDialDeviceEvent(device_list);

  // Reset watermark.
  last_event_registry_generation_ = registry_generation_;
}

std::string DialRegistry::NextLabel() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return base::NumberToString(++label_count_);
}

void DialRegistry::OnDiscoveryRequest(DialService* service) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MaybeSendEvent();
}

void DialRegistry::OnDeviceDiscovered(DialService* service,
                                      const DialDeviceData& device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Adds |device| to our list of devices or updates an existing device, unless
  // |device| is a duplicate. Returns true if the list was modified and
  // increments the list generation.
  auto device_data = std::make_unique<DialDeviceData>(device);
  DCHECK(!device_data->device_id().empty());
  DCHECK(device_data->label().empty());

  bool did_modify_list = false;
  auto lookup_result = device_by_id_map_.find(device_data->device_id());

  if (lookup_result != device_by_id_map_.end()) {
    VLOG(2) << "Found device " << device_data->device_id() << ", merging";

    // Already have previous response.  Merge in data from this response and
    // track if there were any API visible changes.
    did_modify_list = lookup_result->second->UpdateFrom(*device_data);
  } else {
    did_modify_list = MaybeAddDevice(std::move(device_data));
  }

  if (did_modify_list)
    registry_generation_++;

  VLOG(2) << "did_modify_list = " << did_modify_list
          << ", generation = " << registry_generation_;
}

bool DialRegistry::MaybeAddDevice(std::unique_ptr<DialDeviceData> device_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (device_by_id_map_.size() == max_devices_) {
    VLOG(1) << "Maximum registry size reached.  Cannot add device.";
    return false;
  }
  device_data->set_label(NextLabel());
  DialDeviceData* device_data_ptr = device_data.get();
  device_by_id_map_[device_data_ptr->device_id()] = std::move(device_data);
  device_by_label_map_[device_data_ptr->label()] = device_data_ptr;
  VLOG(2) << "Added device, id = " << device_data_ptr->device_id()
          << ", label = " << device_data_ptr->label();
  return true;
}

void DialRegistry::OnDiscoveryFinished(DialService* service) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (PruneExpiredDevices())
    registry_generation_++;
  MaybeSendEvent();
}

void DialRegistry::OnError(DialService* service,
                           const DialService::DialServiceErrorCode& code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  switch (code) {
    case DialService::DIAL_SERVICE_SOCKET_ERROR:
      OnDialError(DIAL_SOCKET_ERROR);
      break;
    case DialService::DIAL_SERVICE_NO_INTERFACES:
      OnDialError(DIAL_NO_INTERFACES);
      break;
    default:
      NOTREACHED();
      OnDialError(DIAL_UNKNOWN);
      break;
  }
}

void DialRegistry::OnConnectionChanged(network::mojom::ConnectionType type) {
  switch (type) {
    case network::mojom::ConnectionType::CONNECTION_NONE:
      if (dial_) {
        VLOG(2) << "Lost connection, shutting down discovery and clearing"
                << " list.";
        OnDialError(DIAL_NETWORK_DISCONNECTED);

        StopPeriodicDiscovery();
        // TODO(justinlin): As an optimization, we can probably keep our device
        // list around and restore it if we reconnected to the exact same
        // network.
        Clear();
        MaybeSendEvent();
      }
      break;
    case network::mojom::ConnectionType::CONNECTION_2G:
    case network::mojom::ConnectionType::CONNECTION_3G:
    case network::mojom::ConnectionType::CONNECTION_4G:
    case network::mojom::ConnectionType::CONNECTION_ETHERNET:
    case network::mojom::ConnectionType::CONNECTION_WIFI:
    case network::mojom::ConnectionType::CONNECTION_UNKNOWN:
    case network::mojom::ConnectionType::CONNECTION_BLUETOOTH:
      if (!dial_) {
        VLOG(2) << "Connection detected, restarting discovery.";
        StartPeriodicDiscovery();
      }
      break;
  }
}

void DialRegistry::OnDialDeviceEvent(const DeviceList& devices) {
  for (auto& observer : observers_)
    observer.OnDialDeviceEvent(devices);
}

void DialRegistry::OnDialError(DialErrorCode type) {
  for (auto& observer : observers_)
    observer.OnDialError(type);
}

}  // namespace media_router
