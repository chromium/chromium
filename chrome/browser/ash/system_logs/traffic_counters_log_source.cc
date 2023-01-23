// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/traffic_counters_log_source.h"

#include <sstream>
#include <string>
#include <utility>

#include "ash/public/cpp/network_config_service.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/net/network_health/network_health_manager.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "content/public/browser/browser_thread.h"

namespace system_logs {

namespace {

constexpr char kTrafficCountersEntry[] = "traffic-counters";

constexpr char kSource[] = "source";
constexpr char kRxBytes[] = "rx_bytes";
constexpr char kTxBytes[] = "tx_bytes";

constexpr char kTrafficCountersKey[] = "traffic_counters";
constexpr char kLastResetTimeKey[] = "last_reset_time";
constexpr char kNotAvailable[] = "Not Available";

std::string GetSourceString(
    chromeos::network_config::mojom::TrafficCounterSource source) {
  std::stringstream ss;
  ss << source;
  return ss.str();
}

base::Value::List ParseTrafficCounters(
    const std::vector<chromeos::network_config::mojom::TrafficCounterPtr>&
        traffic_counters) {
  base::Value::List traffic_counters_list;
  for (const auto& tc : traffic_counters) {
    base::Value::Dict traffic_counter;
    traffic_counter.Set(kSource, GetSourceString(tc->source));
    traffic_counter.Set(kRxBytes, static_cast<double>(tc->rx_bytes));
    traffic_counter.Set(kTxBytes, static_cast<double>(tc->tx_bytes));
    traffic_counters_list.Append(std::move(traffic_counter));
  }
  return traffic_counters_list;
}

}  // namespace

TrafficCountersLogSource::TrafficCountersLogSource()
    : SystemLogsSource("TrafficCountersLog") {
  ash::GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  ash::network_health::NetworkHealthManager::GetInstance()->BindHealthReceiver(
      network_health_service_.BindNewPipeAndPassReceiver());
}

TrafficCountersLogSource::~TrafficCountersLogSource() {}

void TrafficCountersLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());
  callback_ = std::move(callback);
  traffic_counters_.clear();
  network_health_service_->GetRecentlyActiveNetworks(
      base::BindOnce(&TrafficCountersLogSource::OnGetRecentlyActiveNetworks,
                     weak_factory_.GetWeakPtr()));
}

void TrafficCountersLogSource::OnGetRecentlyActiveNetworks(
    const std::vector<std::string>& guids) {
  total_guids_ = guids.size();
  for (const std::string& guid : guids) {
    remote_cros_network_config_->RequestTrafficCounters(
        guid,
        base::BindOnce(&TrafficCountersLogSource::OnTrafficCountersReceived,
                       weak_factory_.GetWeakPtr(), guid));
  }
}

void TrafficCountersLogSource::OnTrafficCountersReceived(
    const std::string& guid,
    std::vector<chromeos::network_config::mojom::TrafficCounterPtr>
        traffic_counters) {
  remote_cros_network_config_->GetManagedProperties(
      guid, base::BindOnce(&TrafficCountersLogSource::OnGetManagedProperties,
                           weak_factory_.GetWeakPtr(), guid,
                           std::move(traffic_counters)));
}

void TrafficCountersLogSource::OnGetManagedProperties(
    const std::string& guid,
    std::vector<chromeos::network_config::mojom::TrafficCounterPtr>
        traffic_counters,
    chromeos::network_config::mojom::ManagedPropertiesPtr managed_properties) {
  base::Value::Dict tc_dict;
  tc_dict.Set(kTrafficCountersKey, ParseTrafficCounters(traffic_counters));
  if (managed_properties && managed_properties->traffic_counter_properties &&
      managed_properties->traffic_counter_properties->friendly_date
          .has_value()) {
    tc_dict.Set(
        kLastResetTimeKey,
        managed_properties->traffic_counter_properties->friendly_date.value());
  } else {
    tc_dict.Set(kLastResetTimeKey, kNotAvailable);
  }
  traffic_counters_.Set(guid, std::move(tc_dict));
  SendResponseIfDone();
}

void TrafficCountersLogSource::SendResponseIfDone() {
  total_guids_--;
  if (total_guids_ > 0) {
    return;
  }

  std::map<std::string, std::string> response;
  std::string json;
  base::JSONWriter::WriteWithOptions(
      traffic_counters_, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  response[kTrafficCountersEntry] = std::move(json);
  std::move(callback_).Run(
      std::make_unique<SystemLogsResponse>(std::move(response)));
}

}  // namespace system_logs
