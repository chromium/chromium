// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/stylus_metrics_recorder.h"

#include "ash/shell.h"
#include "base/logging.h"

/* Emit metrics related to stylus utilization:
 *   StylusDetachedFromGarageSession
 *   StylusDetachedFromDockSession
 *   StylusDetachedFromGarageOrDockSession
 *   TODO(kenalba):
 *    + Usage of stylus (interacting with screen) while stylus is docked (only
 *      once a day).
 *    + Usage of stylus (interacting with screen) while stylus is attached (only
 *      once a day).
 *    + Usage of stylus (interacting with screen) while stylus is undocked (only
 *      once a day).
 *    + Usage of stylus (interacting with screen) while stylus is unattached
 *      (only once a day).
 *    + Usage of stylus (interacting with screen) while stylus is
 *      undocked/unattached (only once a day).
 *
 * Not currently possible with SFUL:
 *   Length of 'failed' vs. 'successful', aka a session
 *   where the pen was used vs. one where it was not.
 *   This is not possible as failed sessions don't have
 *   usetime, and the failure/success of a session
 *   needs to be known at the beginning of the session.
 *
 *   Math between different metrics, hence several combinations
 *   need to be emitted in multiple metrics.
 */

namespace ash {

namespace {

bool IsStylusOnCharge(const PeripheralBatteryListener::BatteryInfo& battery) {
  return (
      battery.charge_status !=
          PeripheralBatteryListener::BatteryInfo::ChargeStatus::kUnknown &&
      battery.charge_status !=
          PeripheralBatteryListener::BatteryInfo::ChargeStatus::kDischarging);
}

}  // namespace

StylusSessionMetricsDelegate::StylusSessionMetricsDelegate(
    const std::string& feature_name)
    : metrics_(feature_name, this) {}

StylusSessionMetricsDelegate::~StylusSessionMetricsDelegate() = default;

bool StylusSessionMetricsDelegate::IsEligible() const {
  return capable_;
}

bool StylusSessionMetricsDelegate::IsEnabled() const {
  return capable_;
}

void StylusSessionMetricsDelegate::SetState(bool now_capable, bool in_session) {
  if (active_ && (!in_session || !now_capable)) {
    metrics_.StopSuccessfulUsage();
    active_ = false;
  }

  capable_ = now_capable;

  if (!active_ && in_session && now_capable) {
    metrics_.RecordUsage(true);
    metrics_.StartSuccessfulUsage();
    active_ = true;
  }
}

StylusMetricsRecorder::StylusMetricsRecorder() {
  UpdateStylusState();

  DCHECK(Shell::HasInstance());
  DCHECK(Shell::Get()->peripheral_battery_listener());

  Shell::Get()->peripheral_battery_listener()->AddObserver(this);
}

StylusMetricsRecorder::~StylusMetricsRecorder() {
  Shell::Get()->peripheral_battery_listener()->RemoveObserver(this);
}

void StylusMetricsRecorder::OnAddingBattery(
    const PeripheralBatteryListener::BatteryInfo& battery) {
  if (battery.type == PeripheralBatteryListener::BatteryInfo::PeripheralType::
                          kStylusViaCharger) {
    // Record the presence of the specific charger type; the API does
    // not imply they are exclusive.
    // TODO(kenalba): Avoid hard-coding this key
    if (battery.key == "garaged-stylus-charger")
      stylus_garage_present_ = true;
    else
      stylus_dock_present_ = true;
    UpdateStylusState();
  }
}

void StylusMetricsRecorder::OnRemovingBattery(
    const PeripheralBatteryListener::BatteryInfo& battery) {
  if (battery.type == PeripheralBatteryListener::BatteryInfo::PeripheralType::
                          kStylusViaCharger) {
    // TODO(kenalba): Avoid hard-coding this key
    if (battery.key == "garaged-stylus-charger")
      stylus_garage_present_ = false;
    else
      stylus_dock_present_ = false;
    stylus_on_charge_.reset();
    UpdateStylusState();
  }
}

void StylusMetricsRecorder::OnUpdatedBatteryLevel(
    const PeripheralBatteryListener::BatteryInfo& battery) {
  if (battery.type == PeripheralBatteryListener::BatteryInfo::PeripheralType::
                          kStylusViaCharger) {
    stylus_on_charge_ = IsStylusOnCharge(battery);
    UpdateStylusState();
  }
}

void StylusMetricsRecorder::UpdateStylusState() {
  /* Sessions are recorded when we know the device is capable of
   * having a stylus garaged or docked, and the stylus is not on charge,
   * and therefore not currently garaged or docked.
   */

  const bool stylus_off_charge =
      stylus_on_charge_.has_value() && *stylus_on_charge_ == false;
  const bool stylus_detached_from_garage =
      stylus_garage_present_ && stylus_off_charge;
  const bool stylus_detached_from_dock =
      stylus_dock_present_ && stylus_off_charge;

  stylus_detached_from_garage_session_metrics_delegate_.SetState(
      stylus_garage_present_, stylus_detached_from_garage);

  stylus_detached_from_dock_session_metrics_delegate_.SetState(
      stylus_dock_present_, stylus_detached_from_dock);

  stylus_detached_from_garage_or_dock_session_metrics_delegate_.SetState(
      stylus_garage_present_ || stylus_dock_present_,
      stylus_detached_from_garage || stylus_detached_from_dock);
}

}  // namespace ash
