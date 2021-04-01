// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_mode_detector.h"

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

const base::TimeDelta DemoModeDetector::kDerelictDetectionTimeout =
    base::TimeDelta::FromHours(8);
const base::TimeDelta DemoModeDetector::kDerelictIdleTimeout =
    base::TimeDelta::FromMinutes(5);
const base::TimeDelta DemoModeDetector::kOobeTimerUpdateInterval =
    base::TimeDelta::FromMinutes(5);

// static
void DemoModeDetector::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kTimeOnOobe, 0);
}

DemoModeDetector::DemoModeDetector(const base::TickClock* clock,
                                   Observer* observer)
    : observer_(observer), tick_clock_(clock) {
  DCHECK(observer_);
  SetupTimeouts();
  InitDetection();
}

DemoModeDetector::~DemoModeDetector() {}

void DemoModeDetector::InitDetection() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableDemoMode))
    return;

  const bool has_derelict_switch =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDerelictDetectionTimeout) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDerelictIdleTimeout);

  // Devices in retail won't be in dev mode, and DUTs (devices under test) often
  // sit unused at OOBE for a while.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSystemDevMode) &&
      !has_derelict_switch) {
    return;
  }

  if (base::SysInfo::IsRunningOnChromeOS() && !has_derelict_switch) {
    std::string track;
    // We're running on an actual device; if we cannot find our release track
    // value or if the track contains "testimage", don't start demo mode.
    if (!base::SysInfo::GetLsbReleaseValue("CHROMEOS_RELEASE_TRACK", &track) ||
        track.find("testimage") != std::string::npos)
      return;
  }

  if (IsDerelict())
    StartIdleDetection();
  else
    StartOobeTimer();
}

void DemoModeDetector::StartIdleDetection() {
  if (!idle_detector_) {
    auto callback = base::BindRepeating(&DemoModeDetector::OnIdle,
                                        weak_ptr_factory_.GetWeakPtr());
    idle_detector_ = std::make_unique<IdleDetector>(callback, tick_clock_);
  }
  idle_detector_->Start(derelict_idle_timeout_);
}

void DemoModeDetector::StartOobeTimer() {
  if (oobe_timer_.IsRunning())
    return;
  oobe_timer_.Start(FROM_HERE, oobe_timer_update_interval_, this,
                    &DemoModeDetector::OnOobeTimerUpdate);
}

void DemoModeDetector::OnIdle() {
  if (demo_launched_)
    return;
  demo_launched_ = true;
}

void DemoModeDetector::OnOobeTimerUpdate() {
  time_on_oobe_ += oobe_timer_update_interval_;

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetInt64(prefs::kTimeOnOobe, time_on_oobe_.InSeconds());

  if (IsDerelict()) {
    oobe_timer_.Stop();
    StartIdleDetection();
  }
}

void DemoModeDetector::SetupTimeouts() {
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  DCHECK(cmdline);

  PrefService* prefs = g_browser_process->local_state();
  time_on_oobe_ =
      base::TimeDelta::FromSeconds(prefs->GetInt64(prefs::kTimeOnOobe));

  int derelict_detection_timeout;
  if (cmdline->HasSwitch(switches::kDerelictDetectionTimeout) &&
      base::StringToInt(
          cmdline->GetSwitchValueASCII(switches::kDerelictDetectionTimeout),
          &derelict_detection_timeout)) {
    derelict_detection_timeout_ =
        base::TimeDelta::FromSeconds(derelict_detection_timeout);
  } else {
    derelict_detection_timeout_ = kDerelictDetectionTimeout;
  }

  int derelict_idle_timeout;
  if (cmdline->HasSwitch(switches::kDerelictIdleTimeout) &&
      base::StringToInt(
          cmdline->GetSwitchValueASCII(switches::kDerelictIdleTimeout),
          &derelict_idle_timeout)) {
    derelict_idle_timeout_ =
        base::TimeDelta::FromSeconds(derelict_idle_timeout);
  } else {
    derelict_idle_timeout_ = kDerelictIdleTimeout;
  }

  int oobe_timer_update_interval;
  if (cmdline->HasSwitch(switches::kOobeTimerInterval) &&
      base::StringToInt(
          cmdline->GetSwitchValueASCII(switches::kOobeTimerInterval),
          &oobe_timer_update_interval)) {
    oobe_timer_update_interval_ =
        base::TimeDelta::FromSeconds(oobe_timer_update_interval);
  } else {
    oobe_timer_update_interval_ = kOobeTimerUpdateInterval;
  }

  // In case we'd be derelict before our timer is set to trigger, reduce
  // the interval so we check again when we're scheduled to go derelict.
  oobe_timer_update_interval_ =
      std::max(std::min(oobe_timer_update_interval_,
                        derelict_detection_timeout_ - time_on_oobe_),
               base::TimeDelta::FromSeconds(0));
}

bool DemoModeDetector::IsDerelict() {
  return time_on_oobe_ >= derelict_detection_timeout_;
}

}  // namespace chromeos
