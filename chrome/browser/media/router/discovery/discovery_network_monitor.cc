// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/discovery_network_monitor.h"

#include <memory>
#include <unordered_set>

#include "base/hash/sha1.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/media/router/discovery/discovery_network_list.h"
#include "chrome/browser/media/router/discovery/discovery_network_monitor_metric_observer.h"
#include "content/public/browser/network_service_instance.h"
#include "net/base/network_interfaces.h"

namespace media_router {
namespace {

std::string ComputeNetworkId(
    const std::vector<DiscoveryNetworkInfo>& network_info_list) {
  if (network_info_list.empty()) {
    return DiscoveryNetworkMonitor::kNetworkIdDisconnected;
  }
  if (std::find_if(network_info_list.begin(), network_info_list.end(),
                   [](const DiscoveryNetworkInfo& network_info) {
                     return !network_info.network_id.empty();
                   }) == network_info_list.end()) {
    return DiscoveryNetworkMonitor::kNetworkIdUnknown;
  }

  std::string combined_ids;
  for (const auto& network_info : network_info_list) {
    combined_ids = combined_ids + "!" + network_info.network_id;
  }

  std::string hash = base::SHA1HashString(combined_ids);
  return base::ToLowerASCII(base::HexEncode(hash.data(), hash.length()));
}

base::LazyInstance<DiscoveryNetworkMonitor>::Leaky g_discovery_monitor;

}  // namespace

// static
constexpr char const DiscoveryNetworkMonitor::kNetworkIdDisconnected[];
// static
constexpr char const DiscoveryNetworkMonitor::kNetworkIdUnknown[];

// static
DiscoveryNetworkMonitor* DiscoveryNetworkMonitor::GetInstance() {
  return g_discovery_monitor.Pointer();
}

// static
std::unique_ptr<DiscoveryNetworkMonitor>
DiscoveryNetworkMonitor::CreateInstanceForTest(NetworkInfoFunction strategy) {
  return base::WrapUnique(new DiscoveryNetworkMonitor(strategy));
}

void DiscoveryNetworkMonitor::AddObserver(Observer* const observer) {
  observers_->AddObserver(observer);
}

void DiscoveryNetworkMonitor::RemoveObserver(Observer* const observer) {
  observers_->RemoveObserver(observer);
}

void DiscoveryNetworkMonitor::Refresh(NetworkIdCallback callback) {
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&DiscoveryNetworkMonitor::UpdateNetworkInfo,
                     base::Unretained(this)),
      std::move(callback));
}

void DiscoveryNetworkMonitor::GetNetworkId(NetworkIdCallback callback) {
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&DiscoveryNetworkMonitor::GetNetworkIdOnSequence,
                     base::Unretained(this)),
      std::move(callback));
}

DiscoveryNetworkMonitor::DiscoveryNetworkMonitor()
    : DiscoveryNetworkMonitor(&GetDiscoveryNetworkInfoList) {}

DiscoveryNetworkMonitor::DiscoveryNetworkMonitor(NetworkInfoFunction strategy)
    : network_id_(kNetworkIdDisconnected),
      observers_(new base::ObserverListThreadSafe<Observer>(
          base::ObserverListPolicy::EXISTING_ONLY)),
      task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      network_info_function_(strategy),
      metric_observer_(std::make_unique<DiscoveryNetworkMonitorMetricObserver>(
          base::DefaultTickClock::GetInstance(),
          std::make_unique<DiscoveryNetworkMonitorMetrics>())) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  AddObserver(metric_observer_.get());

  content::GetNetworkConnectionTracker()
      ->AddLeakyNetworkConnectionObserver(this);

  // If the current connection type is available, call UpdateNetworkInfo,
  // otherwise let OnConnectionChanged call it when the connection type is
  // ready.
  auto connection_type = network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  if (content::GetNetworkConnectionTracker()->GetConnectionType(
          &connection_type,
          base::BindOnce(&DiscoveryNetworkMonitor::OnConnectionChanged,
                         base::Unretained(this)))) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            base::IgnoreResult(&DiscoveryNetworkMonitor::UpdateNetworkInfo),
            base::Unretained(this)));
  }
}

DiscoveryNetworkMonitor::~DiscoveryNetworkMonitor() {
  // Never gets called.
}

void DiscoveryNetworkMonitor::SetNetworkInfoFunctionForTest(
    NetworkInfoFunction strategy) {
  network_info_function_ = strategy;
}

void DiscoveryNetworkMonitor::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&DiscoveryNetworkMonitor::UpdateNetworkInfo),
          base::Unretained(this)));
}

std::string DiscoveryNetworkMonitor::GetNetworkIdOnSequence() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return network_id_;
}

std::string DiscoveryNetworkMonitor::UpdateNetworkInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto network_info_list = network_info_function_();
  auto network_id = ComputeNetworkId(network_info_list);

  // Although we are called with CONTINUE_ON_SHUTDOWN, none of these fields will
  // disappear out from under us since |g_discovery_monitor| is declared with
  // LeakyLazyInstanceTraits, and is therefore never deleted.
  network_id_.swap(network_id);

  if (network_id_ != network_id) {
    observers_->Notify(FROM_HERE, &Observer::OnNetworksChanged, network_id_);
  }

  return network_id_;
}

}  // namespace media_router
