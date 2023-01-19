// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/cros_events_processor.h"

#include "base/logging.h"
#include "base/system/sys_info.h"
#include "chrome/browser/metrics/structured/structured_metric_prefs.h"

namespace metrics::structured::cros_event {

CrOSEventsProcessor::CrOSEventsProcessor(PrefService* pref_service)
    : pref_service_(pref_service) {}
CrOSEventsProcessor::~CrOSEventsProcessor() = default;

// static
void CrOSEventsProcessor::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  // These prefs are modified multiple times per minute and should be registered
  // as LOSSY_PREF since persisting prefs constantly is expensive.
  registry->RegisterIntegerPref(kEventSequenceResetCounter, 0,
                                PrefRegistry::LOSSY_PREF);
  registry->RegisterInt64Pref(kEventSequenceLastSystemUptime, 0,
                              PrefRegistry::LOSSY_PREF);
}

bool CrOSEventsProcessor::ShouldProcessOnEventRecord(const Event& event) {
  return event.IsEventSequenceType();
}

void CrOSEventsProcessor::SetCurrentUptimeForTesting(int64_t current_uptime) {
  current_uptime_for_testing_ = current_uptime;
}

void CrOSEventsProcessor::OnEventsRecord(Event* event) {
  auto current_reset_counter =
      pref_service_->GetInteger(kEventSequenceResetCounter);
  auto last_system_uptime =
      pref_service_->GetInt64(kEventSequenceLastSystemUptime);

  auto current_uptime = current_uptime_for_testing_
                            ? current_uptime_for_testing_
                            : base::SysInfo::Uptime().InMilliseconds();

  // If the last uptime is larger than the current uptime, a reset most likely
  // happened.
  if (last_system_uptime > current_uptime) {
    ++current_reset_counter;
    pref_service_->SetInteger(kEventSequenceResetCounter,
                              current_reset_counter);
  }

  pref_service_->SetInt64(kEventSequenceLastSystemUptime, current_uptime);

  event->SetEventSequenceMetadata(
      Event::EventSequenceMetadata(current_reset_counter));
}

}  // namespace metrics::structured::cros_event
