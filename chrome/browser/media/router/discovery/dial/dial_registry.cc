// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_registry.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "chrome/browser/media/router/discovery/dial/dial_service_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"

using base::Time;
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

DialRegistry::DialRegistry(
    DialRegistry::Client& client,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : client_(client),
      task_runner_(task_runner),
      registry_generation_(0),
      last_event_registry_generation_(0),
      label_count_(0),
      refresh_interval_delta_(base::Seconds(kDialRefreshIntervalSecs)),
      expiration_delta_(base::Seconds(kDialExpirationSecs)),
      max_devices_(kDialMaxDevices),
      clock_(base::DefaultClock::GetInstance()) {
  DCHECK_GT(max_devices_, 0U);
}

DialRegistry::~DialRegistry() = default;

void DialRegistry::SetNetworkConnectionTracker(
    network::NetworkConnectionTracker* tracker) {
  network_connection_tracker_ = tracker;
  network_connection_tracker_->AddLeakyNetworkConnectionObserver(this);
  StartPeriodicDiscovery();
}

void DialRegistry::SetNetLog(net::NetLog* net_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!net_log_)
    net_log_ = net_log;
}

void DialRegistry::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // base::Unretained() is safe here because DialRegistry is (indirectly) owned
  // by a singleton and is never freed.
  content::GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&content::GetNetworkConnectionTracker),
      base::BindOnce(&DialRegistry::SetNetworkConnectionTracker,
                     base::Unretained(this)));
}

std::unique_ptr<DialService> DialRegistry::CreateDialService() {
  return std::make_unique<DialServiceImpl>(*this, task_runner_, net_log_);
}

void DialRegistry::ClearDialService() {
  dial_.reset();
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
  network::mojom::ConnectionType type;
  // base::Unretained() is safe here because DialRegistry is (indirectly) owned
  // by a singleton and is never freed.
  if (!network_connection_tracker_ ||
      !network_connection_tracker_->GetConnectionType(
          &type, base::BindOnce(&DialRegistry::OnConnectionChanged,
                                base::Unretained(this)))) {
    // If the ConnectionType is unknown, return false. We'll try to start
    // discovery again when we receive the OnConnectionChanged callback.
    client_->OnDialError(DIAL_UNKNOWN);
    return false;
  }
  if (type == network::mojom::ConnectionType::CONNECTION_NONE) {
    client_->OnDialError(DIAL_NETWORK_DISCONNECTED);
    return false;
  }
  if (network::NetworkConnectionTracker::IsConnectionCellular(type)) {
    client_->OnDialError(DIAL_CELLULAR_NETWORK);
    return false;
  }
  return true;
}

bool DialRegistry::DiscoverNow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ReadyToDiscover())
    return false;

  if (!dial_) {
    client_->OnDialError(DIAL_UNKNOWN);
    return false;
  }

  // Force increment |registry_generation_| to ensure the list is sent even if
  // it has not changed.
  bool started = dial_->Discover();
  if (started)
    ++registry_generation_;

  return started;
}

void DialRegistry::StartPeriodicDiscovery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ReadyToDiscover() || dial_)
    return;

  dial_ = CreateDialService();
  DoDiscovery();
  repeating_timer_ = std::make_unique<base::RepeatingTimer>();
  repeating_timer_->Start(FROM_HERE, refresh_interval_delta_, this,
                          &DialRegistry::DoDiscovery);
  // Always send the current device list with the next discovery request.  This
  // may not be necessary, but is done to match previous behavior.
  ++registry_generation_;
}

void DialRegistry::DoDiscovery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(dial_);
  dial_->Discover();
}

void DialRegistry::StopPeriodicDiscovery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!dial_)
    return;

  repeating_timer_->Stop();
  repeating_timer_.reset();
  ClearDialService();
}

bool DialRegistry::PruneExpiredDevices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool pruned_device = false;
  auto it = device_by_label_map_.begin();
  while (it != device_by_label_map_.end()) {
    auto* device = it->second.get();
    if (IsDeviceExpired(*device)) {
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
        device.response_time() + base::Seconds(device.max_age());
    if (now > max_age_expiration_time)
      return true;
  }
  return false;
}

void DialRegistry::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  device_by_id_map_.clear();
  device_by_label_map_.clear();
  registry_generation_++;
}

void DialRegistry::MaybeSendDeviceList() {
  // Send the device list to the client if it has changed since the last list
  // was sent.
  bool needs_event = last_event_registry_generation_ < registry_generation_;
  if (!needs_event)
    return;

  DeviceList device_list;
  for (DeviceByLabelMap::const_iterator it = device_by_label_map_.begin();
       it != device_by_label_map_.end(); ++it) {
    device_list.push_back(*(it->second));
  }
  client_->OnDialDeviceList(device_list);

  // Reset watermark.
  last_event_registry_generation_ = registry_generation_;
}

std::string DialRegistry::NextLabel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::NumberToString(++label_count_);
}

void DialRegistry::OnDiscoveryRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeSendDeviceList();
}

void DialRegistry::OnDeviceDiscovered(const DialDeviceData& device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Adds |device| to our list of devices or updates an existing device, unless
  // |device| is a duplicate. Returns true if the list was modified and
  // increments the list generation.
  auto device_data = std::make_unique<DialDeviceData>(device);
  DCHECK(!device_data->device_id().empty());
  DCHECK(device_data->label().empty());

  bool did_modify_list = false;
  auto lookup_result = device_by_id_map_.find(device_data->device_id());

  if (lookup_result != device_by_id_map_.end()) {
    // Already have previous response.  Merge in data from this response and
    // track if there were any API visible changes.
    did_modify_list = lookup_result->second->UpdateFrom(*device_data);
  } else {
    did_modify_list = MaybeAddDevice(std::move(device_data));
  }

  if (did_modify_list)
    registry_generation_++;
}

bool DialRegistry::MaybeAddDevice(std::unique_ptr<DialDeviceData> device_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (device_by_id_map_.size() == max_devices_) {
    return false;
  }
  device_data->set_label(NextLabel());
  DialDeviceData* device_data_ptr = device_data.get();
  device_by_id_map_[device_data_ptr->device_id()] = std::move(device_data);
  device_by_label_map_[device_data_ptr->label()] = device_data_ptr;
  return true;
}

void DialRegistry::OnDiscoveryFinished() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (PruneExpiredDevices())
    registry_generation_++;
  MaybeSendDeviceList();
}

void DialRegistry::OnError(DialService::DialServiceErrorCode code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (code) {
    case DialService::DIAL_SERVICE_SOCKET_ERROR:
      client_->OnDialError(DIAL_SOCKET_ERROR);
      break;
    case DialService::DIAL_SERVICE_NO_INTERFACES:
      client_->OnDialError(DIAL_NO_INTERFACES);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      client_->OnDialError(DIAL_UNKNOWN);
      break;
  }
}

void DialRegistry::OnConnectionChanged(network::mojom::ConnectionType type) {
  switch (type) {
    case network::mojom::ConnectionType::CONNECTION_NONE:
      if (dial_) {
        client_->OnDialError(DIAL_NETWORK_DISCONNECTED);
        StopPeriodicDiscovery();
        Clear();
        MaybeSendDeviceList();
      }
      break;
    case network::mojom::ConnectionType::CONNECTION_2G:
    case network::mojom::ConnectionType::CONNECTION_3G:
    case network::mojom::ConnectionType::CONNECTION_4G:
    case network::mojom::ConnectionType::CONNECTION_5G:
    case network::mojom::ConnectionType::CONNECTION_ETHERNET:
    case network::mojom::ConnectionType::CONNECTION_WIFI:
    case network::mojom::ConnectionType::CONNECTION_UNKNOWN:
    case network::mojom::ConnectionType::CONNECTION_BLUETOOTH:
      if (!dial_) {
        StartPeriodicDiscovery();
      }
      break;
  }
}

}  // namespace media_router
