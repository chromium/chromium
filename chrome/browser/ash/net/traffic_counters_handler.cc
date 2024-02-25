// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/traffic_counters_handler.h"

#include <memory>
#include <string>

#include "ash/public/cpp/network_config_service.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace ash {

namespace {

// Interval duration to determine the auto reset check frequency.
constexpr base::TimeDelta kResetCheckInterval = base::Hours(6);

base::Time GetValidTime(base::Time::Exploded exploded_time) {
  base::Time time;
  while (!base::Time::FromLocalExploded(exploded_time, &time)) {
    if (exploded_time.day_of_month > 28)
      --exploded_time.day_of_month;
    else
      break;
  }
  return time;
}

// To avoid discrepancies between different times of the same day, set all times
// to 12:01:00 AM. This is safe to do so because traffic counters will never be
// automatically reset more than once on any given day.
void AdjustExplodedTimeValues(base::Time::Exploded* exploded_time) {
  exploded_time->hour = 0;
  exploded_time->minute = 1;
  exploded_time->second = 0;
  exploded_time->millisecond = 0;
}

}  // namespace

namespace traffic_counters {

TrafficCountersHandler::TrafficCountersHandler()
    : time_getter_(base::BindRepeating([]() { return base::Time::Now(); })),
      timer_(std::make_unique<base::RepeatingTimer>()) {
  GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  remote_cros_network_config_->AddObserver(
      cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());
}

TrafficCountersHandler::~TrafficCountersHandler() = default;

void TrafficCountersHandler::Start() {
  RunAll();
  timer_->Start(FROM_HERE, kResetCheckInterval, this,
                &TrafficCountersHandler::RunAll);
}

void TrafficCountersHandler::RunAll() {
  RunWithFilter(chromeos::network_config::mojom::FilterType::kAll);
}

void TrafficCountersHandler::RunWithFilter(
    chromeos::network_config::mojom::FilterType filter_type) {
  NET_LOG(EVENT) << "Starting run with filter type " << filter_type
                 << " at: " << time_getter_.Run();
  DCHECK(remote_cros_network_config_);
  remote_cros_network_config_->GetNetworkStateList(
      chromeos::network_config::mojom::NetworkFilter::New(
          filter_type, chromeos::network_config::mojom::NetworkType::kAll,
          chromeos::network_config::mojom::kNoLimit),
      base::BindOnce(&TrafficCountersHandler::OnNetworkStateListReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TrafficCountersHandler::OnActiveNetworksChanged(
    std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
        active_networks) {
  RunWithFilter(chromeos::network_config::mojom::FilterType::kActive);
}

void TrafficCountersHandler::OnNetworkStateListReceived(
    std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
        networks) {
  for (const auto& network : networks) {
    if (!GetAutoResetEnabled(network->guid)) {
      continue;
    }
    remote_cros_network_config_->GetManagedProperties(
        network->guid,
        base::BindOnce(&TrafficCountersHandler::OnManagedPropertiesReceived,
                       weak_ptr_factory_.GetWeakPtr(), network->guid));
  }
}

bool TrafficCountersHandler::GetAutoResetEnabled(std::string guid) {
  NetworkMetadataStore* metadata_store =
      NetworkHandler::Get()->network_metadata_store();
  DCHECK(metadata_store);
  const base::Value* enabled =
      metadata_store->GetEnableTrafficCountersAutoReset(guid);
  return enabled && enabled->GetBool();
}

void TrafficCountersHandler::OnManagedPropertiesReceived(
    std::string guid,
    chromeos::network_config::mojom::ManagedPropertiesPtr managed_properties) {
  if (!managed_properties) {
    NET_LOG(ERROR) << "Failed to retrive properties for: "
                   << NetworkGuidId(guid);
    return;
  }
  if (!managed_properties->traffic_counter_properties) {
    NET_LOG(ERROR) << "Failed to retrieve traffic counter properties for: "
                   << NetworkGuidId(guid);
  }
  bool should_reset;
  if (!managed_properties->traffic_counter_properties->last_reset_time
           .has_value()) {
    // No last reset time, trigger an initial reset.
    should_reset = true;
  } else {
    base::Time last_reset_time = base::Time::FromDeltaSinceWindowsEpoch(
        managed_properties->traffic_counter_properties->last_reset_time
            ->ToDeltaSinceWindowsEpoch());
    should_reset = ShouldReset(guid, last_reset_time);
  }
  if (should_reset) {
    NET_LOG(EVENT) << "Resetting traffic counters for network: "
                   << NetworkGuidId(guid);
    remote_cros_network_config_->ResetTrafficCounters(guid);
  }
}

// Note that if a user manually resets the traffic counters on the user
// specified reset day before TrafficCountersHandler runs,
// TrafficCountersHandler class will not automatically reset the counters until
// the reset day the following month.
bool TrafficCountersHandler::ShouldReset(std::string guid,
                                         base::Time last_reset_time) {
  NetworkMetadataStore* metadata_store =
      NetworkHandler::Get()->network_metadata_store();
  DCHECK(metadata_store);
  const base::Value* reset_day_ptr =
      metadata_store->GetDayOfTrafficCountersAutoReset(guid);
  if (!reset_day_ptr) {
    NET_LOG(ERROR) << "Failed to retrieve auto reset day for network: "
                   << NetworkGuidId(guid);
    return false;
  }
  auto user_specified_reset_day = reset_day_ptr->GetInt();

  base::Time::Exploded current_time_exploded;
  time_getter_.Run().LocalExplode(&current_time_exploded);
  AdjustExplodedTimeValues(&current_time_exploded);

  base::Time::Exploded last_reset_time_exploded;
  last_reset_time.LocalExplode(&last_reset_time_exploded);
  AdjustExplodedTimeValues(&last_reset_time_exploded);
  if (!base::Time::FromLocalExploded(last_reset_time_exploded,
                                     &last_reset_time)) {
    NET_LOG(ERROR) << "Failed to set last_reset_time to 12:01:00 AM";
    return false;
  }

  bool result = false;
  base::Time expected_last_reset_time =
      GetExpectedLastResetTime(current_time_exploded, user_specified_reset_day);
  if (expected_last_reset_time > last_reset_time) {
    // If the actual last auto reset occurred before our expected last
    // auto reset time, traffic counters should be reset.
    result = true;
  }

  VLOG(3) << "ShouldReset for: " << guid << " at: " << time_getter_.Run()
          << " last: " << last_reset_time
          << " expected_last: " << expected_last_reset_time
          << " day: " << user_specified_reset_day << " = " << result;

  // expected_last_reset_time.ToDeltaSinceWindowsEpoch() <=
  // actual_last_reset_time.ToDeltaSinceWindowsEpoch(). Don't reset traffic
  // counters.
  return result;
}

base::Time TrafficCountersHandler::GetExpectedLastResetTime(
    const base::Time::Exploded& current_time_exploded,
    int user_specified_reset_day) {
  base::Time::Exploded exploded = current_time_exploded;
  exploded.day_of_month = user_specified_reset_day;
  GetValidTime(exploded).LocalExplode(&exploded);

  // If the user specified reset day is greater than the current day, then the
  // expected last reset day is on the user specified day of the previous
  // month. Concretely, if e.g., user_specified_reset_day = 14 and current day
  // = 13, the last reset day is expected to be on the 14th of the previous
  // month. Otherwise, we expect that the last reset occurred in the current
  // month.
  if (exploded.day_of_month > current_time_exploded.day_of_month) {
    // "+ 11) % 12) + 1" wraps the month around if it goes outside 1..12.
    exploded.month = (((exploded.month - 1) + 11) % 12) + 1;
    exploded.year -= (exploded.month == 12);
  }
  return GetValidTime(exploded);
}

void TrafficCountersHandler::RunForTesting() {
  RunWithFilter(chromeos::network_config::mojom::FilterType::kAll);
}

void TrafficCountersHandler::SetTimeGetterForTest(TimeGetter time_getter) {
  time_getter_ = std::move(time_getter);
}

}  // namespace traffic_counters

}  // namespace ash
