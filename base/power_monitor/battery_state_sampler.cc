// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/battery_state_sampler.h"

#include "base/power_monitor/power_monitor_buildflags.h"

#if !BUILDFLAG(IS_MAC)
#include "base/power_monitor/timer_sampling_event_source.h"
#endif

namespace base {

namespace {

// Singleton instance of the BatteryStateSampler.
BatteryStateSampler* g_battery_state_sampler = nullptr;
bool g_test_instance_installed = false;

}  // namespace

BatteryStateSampler::BatteryStateSampler(
    std::unique_ptr<SamplingEventSource> sampling_event_source,
    std::unique_ptr<BatteryLevelProvider> battery_level_provider)
    : sampling_event_source_(std::move(sampling_event_source)),
      battery_level_provider_(std::move(battery_level_provider)) {
  DCHECK(sampling_event_source_);
  DCHECK(battery_level_provider_);

  DCHECK(!g_battery_state_sampler);
  g_battery_state_sampler = this;

  // Get an initial sample.
  battery_level_provider_->GetBatteryState(
      base::BindOnce(&BatteryStateSampler::OnInitialBatteryStateSampled,
                     base::Unretained(this)));

  // Start the periodic sampling.
  sampling_event_source_->Start(base::BindRepeating(
      &BatteryStateSampler::OnSamplingEvent, base::Unretained(this)));
}

BatteryStateSampler::~BatteryStateSampler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(g_battery_state_sampler, this);
  g_battery_state_sampler = nullptr;
  g_test_instance_installed = false;
}

// static
BatteryStateSampler* BatteryStateSampler::Get() {
  // On a platform with a BatteryLevelProvider implementation, the global
  // instance must be created before accessing it.
  // TODO(crbug.com/40871810): ChromeOS currently doesn't define
  // `HAS_BATTERY_LEVEL_PROVIDER_IMPL` but it should once the locations of the
  // providers and sampling sources are consolidated.
#if BUILDFLAG(HAS_BATTERY_LEVEL_PROVIDER_IMPL) || BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(g_battery_state_sampler);
#endif
  return g_battery_state_sampler;
}

void BatteryStateSampler::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observer_list_.AddObserver(observer);

  // Send the last sample available.
  if (has_last_battery_state_)
    observer->OnBatteryStateSampled(last_battery_state_);
}

void BatteryStateSampler::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(observer);
}

void BatteryStateSampler::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sampling_event_source_.reset();
  battery_level_provider_.reset();
}

// static
std::unique_ptr<base::BatteryStateSampler>
BatteryStateSampler::CreateInstanceForTesting(
    std::unique_ptr<SamplingEventSource> sampling_event_source,
    std::unique_ptr<BatteryLevelProvider> battery_level_provider) {
  g_test_instance_installed = true;
  return std::make_unique<BatteryStateSampler>(
      std::move(sampling_event_source), std::move(battery_level_provider));
}

// static
bool BatteryStateSampler::HasTestingInstance() {
  return g_test_instance_installed;
}

#if !BUILDFLAG(IS_MAC)
// static
std::unique_ptr<SamplingEventSource>
BatteryStateSampler::CreateSamplingEventSource() {
  // On platforms where the OS does not provide a notification when an updated
  // battery level is available, simply sample on a regular 1 minute interval.
  return std::make_unique<TimerSamplingEventSource>(Minutes(1));
}
#endif  // !BUILDFLAG(IS_MAC)

void BatteryStateSampler::OnInitialBatteryStateSampled(
    const std::optional<BatteryLevelProvider::BatteryState>& battery_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!has_last_battery_state_);
  has_last_battery_state_ = true;
  last_battery_state_ = battery_state;

  for (auto& observer : observer_list_)
    observer.OnBatteryStateSampled(battery_state);
}

void BatteryStateSampler::OnSamplingEvent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(battery_level_provider_);

  battery_level_provider_->GetBatteryState(base::BindOnce(
      &BatteryStateSampler::OnBatteryStateSampled, base::Unretained(this)));
}

void BatteryStateSampler::OnBatteryStateSampled(
    const std::optional<BatteryLevelProvider::BatteryState>& battery_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(has_last_battery_state_);
  last_battery_state_ = battery_state;

  for (auto& observer : observer_list_)
    observer.OnBatteryStateSampled(battery_state);
}

}  // namespace base
