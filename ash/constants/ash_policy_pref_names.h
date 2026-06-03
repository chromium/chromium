// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_ASH_POLICY_PREF_NAMES_H_
#define ASH_CONSTANTS_ASH_POLICY_PREF_NAMES_H_

namespace ash::prefs {

// Maintain a list of last upload times of system logs in double type; this is
// for the purpose of throttling log uploads.
inline constexpr char kStoreLogStatesAcrossReboots[] =
    "policy_store_log_states_across_reboots";

// A preference to keep track of upload times of event based logs.
inline constexpr char kEventBasedLogLastUploadTimes[] =
    "ash.policy.event_based_log_last_upload_times";

// Int64 pref indicating the time in microseconds since Windows epoch when the
// timer for update required which will block user session was started. If the
// timer is not started the pref holds the default value base::Time().
inline constexpr char kUpdateRequiredTimerStartTime[] =
    "update_required_timer_start_time";

// Int64 pref indicating the waiting time in microseconds after which the update
// required timer will expire and block user session. If the timer is not
// started the pref holds the default value base::TimeDelta().
inline constexpr char kUpdateRequiredWarningPeriod[] =
    "update_required_warning_period";

// A dictionary containing server-provided device state pulled form the cloud
// after recovery.
inline constexpr char kServerBackedDeviceState[] = "server_backed_device_state";

// A boolean preference controlling Android status reporting.
inline constexpr char kReportArcStatusEnabled[] =
    "arc.status_reporting_enabled";

// Key for list of users that should be reported.
inline constexpr char kReportingUsers[] = "reporting_users";

// Whether to log events for Android app installs.
inline constexpr char kArcAppInstallEventLoggingEnabled[] =
    "arc.app_install_event_logging_enabled";

// Integer that specifies the policy refresh rate for device-policy in
// milliseconds. Not all values are meaningful, so it is clamped to a sane range
// by the cloud policy subsystem.
inline constexpr char kDevicePolicyRefreshRate[] = "policy.device_refresh_rate";

}  // namespace ash::prefs

#endif  // ASH_CONSTANTS_ASH_POLICY_PREF_NAMES_H_
