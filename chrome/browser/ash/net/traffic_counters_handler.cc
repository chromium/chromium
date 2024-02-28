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

// Gets the last day of the month when the user specified reset day
// exceeds the number of day in that month. For example, if the user specified
// reset day is 31, this function would ensure that the returned time would
// represent Feb 28 (non-leap year), Apr 30, etc.
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

// Calculates when the traffic counters were expected to be reset last month.
base::Time CalculateLastMonthResetTime(base::Time::Exploded exploded,
                                       int user_specified_reset_day) {
  exploded.month -= 1;
  if (exploded.month < 1) {
    exploded.month = 12;
    exploded.year -= 1;
  }

  exploded.day_of_month = user_specified_reset_day;

  return GetValidTime(exploded);
}

// Calculates when the traffic counters were/are expected to be reset this
// month.
base::Time CalculateCurrentMonthResetTime(base::Time::Exploded exploded,
                                          int user_specified_reset_day) {
  exploded.day_of_month = user_specified_reset_day;

  return GetValidTime(exploded);
}

// To avoid discrepancies between different times of the same day, set all times
// to 12:00:00 AM. This is safe to do so because traffic counters will never be
// automatically reset more than once on any given day.
void AdjustExplodedTimeValues(base::Time::Exploded* exploded_time) {
  exploded_time->hour = 0;
  exploded_time->minute = 0;
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

TrafficCountersHandler::~TrafficCountersHandler() {
  network_metadata_store_->RemoveObserver(this);
  network_metadata_store_ = nullptr;
}

void TrafficCountersHandler::Start() {
  RunActive();
  timer_->Start(FROM_HERE, kResetCheckInterval, this,
                &TrafficCountersHandler::RunActive);
}

void TrafficCountersHandler::RunActive() {
  NET_LOG(EVENT) << "Starting run at: " << time_getter_.Run();
  DCHECK(remote_cros_network_config_);
  remote_cros_network_config_->GetNetworkStateList(
      chromeos::network_config::mojom::NetworkFilter::New(
          chromeos::network_config::mojom::FilterType::kActive,
          chromeos::network_config::mojom::NetworkType::kAll,
          chromeos::network_config::mojom::kNoLimit),
      base::BindOnce(&TrafficCountersHandler::OnNetworkStateListReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TrafficCountersHandler::OnActiveNetworksChanged(
    std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
        active_networks) {
  RunActive();
}

void TrafficCountersHandler::OnNetworkUpdate(
    const std::string& guid,
    const base::Value::Dict* set_properties) {
  // OnNetworkUpdate() is invoked when NetworkMetadataObserver sees a change to
  // a configuration property. This class is interested in reset day changes,
  // and changes to configuration properties might indicate a reset day change,
  // hence the RunActive() call. But if properties is null, there is nothing to
  // check.
  if (!set_properties) {
    return;
  }

  RunActive();
}

void TrafficCountersHandler::OnNetworkStateListReceived(
    std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
        networks) {
  for (const auto& network : networks) {
    remote_cros_network_config_->GetManagedProperties(
        network->guid,
        base::BindOnce(&TrafficCountersHandler::OnManagedPropertiesReceived,
                       weak_ptr_factory_.GetWeakPtr(), network->guid));
  }
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
  if (!managed_properties->traffic_counter_properties->last_reset_time
           .has_value()) {
    // No last reset time, trigger an initial reset.
    NET_LOG(EVENT) << "Resetting traffic counters for network: "
                   << NetworkGuidId(guid);
    remote_cros_network_config_->ResetTrafficCounters(guid);
    return;
  }

  // TODO(b/327244777): Currently, NetworkMetadataStore is destroyed and
  // re-created when prefs are loaded. Once this is fixed, move the
  // initialization of |network_metadata_store_| to the constructor.
  if (!network_metadata_store_) {
    network_metadata_store_ =
        NetworkHandler::Get()->network_metadata_store()->GetWeakPtr();
    network_metadata_store_->AddObserver(this);
  }

  DCHECK(network_metadata_store_);
  const base::Value* reset_day_ptr =
      network_metadata_store_->GetDayOfTrafficCountersAutoReset(guid);
  if (!reset_day_ptr) {
    NET_LOG(EVENT) << "Failed to retrieve auto reset day for network: "
                   << NetworkGuidId(guid);
    remote_cros_network_config_->ResetTrafficCounters(guid);
    network_metadata_store_->SetDayOfTrafficCountersAutoReset(guid, 1);
    return;
  }

  base::Time::Exploded current_time_exploded = CurrentDateExploded();
  auto user_specified_reset_day = reset_day_ptr->GetInt();

  base::Time last_month_reset = CalculateLastMonthResetTime(
      current_time_exploded, user_specified_reset_day);
  base::Time last_reset_time = base::Time::FromDeltaSinceWindowsEpoch(
      managed_properties->traffic_counter_properties->last_reset_time
          ->ToDeltaSinceWindowsEpoch());
  // If the last time traffic counters were reset (last_reset_time) was before
  // the time traffic counters were expected to be reset last month
  // (last_month_reset), then the traffic counters should be reset. This handles
  // the case where the traffic counters feature was disabled for over a month
  // and then re-enabled.
  if (last_reset_time < last_month_reset) {
    remote_cros_network_config_->ResetTrafficCounters(guid);
    return;
  }

  base::Time curr_month_reset = CalculateCurrentMonthResetTime(
      current_time_exploded, user_specified_reset_day);
  base::Time current_date = GetValidTime(current_time_exploded);
  // If the last time traffic counters were reset (last_reset_time) was before
  // the expected reset date on this month (curr_month_resset), and today
  // (current_date) is equal to or greater than the expected date of reset for
  // this month (curr_month_reset), then reset the counters. For example, let's
  // assume that traffic counters are the be reset on the 5th of every month and
  // were last reset on January 5th. If the current_date is between February
  // 1st-February 4th, then current_date (e.g, Feb 3rd) > curr_month_reset (Feb
  // 5th), so the counters are not reset. However, if the current date is Feb
  // 5th onwards, then current_date (Feb 5th) = curr_month_reset (Feb 5th), so
  // traffic counters are reset.
  if (last_reset_time < curr_month_reset && current_date >= curr_month_reset) {
    remote_cros_network_config_->ResetTrafficCounters(guid);
  }
}

base::Time::Exploded TrafficCountersHandler::CurrentDateExploded() {
  base::Time::Exploded current_time_exploded;
  time_getter_.Run().LocalExplode(&current_time_exploded);
  AdjustExplodedTimeValues(&current_time_exploded);

  return current_time_exploded;
}

void TrafficCountersHandler::RunForTesting() {
  RunActive();
}

void TrafficCountersHandler::SetTimeGetterForTest(TimeGetter time_getter) {
  time_getter_ = std::move(time_getter);
}

}  // namespace traffic_counters

}  // namespace ash
