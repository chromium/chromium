// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_APP_INSTALL_EVENT_LOG_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_APP_INSTALL_EVENT_LOG_UTIL_H_

#include "components/policy/proto/device_management_backend.pb.h"

namespace base {
class Value;
}  // namespace base

class Profile;

namespace em = enterprise_management;

namespace policy {

// Returns true if GAIA ID can be fetched for a given |profile|, and it can be
// converted to a number. If GAIA ID can be fetched, it gets written to
// |gaia_id|, otherwise returns false.
bool GetGaiaId(Profile* profile, int* gaia_id);

// Return serial number of the device.
std::string GetSerialNumber();

// Converts AppInstallReportRequest proto defined in
// components/policy/proto/device_management_backend.proto to a dictionary value
// that corresponds to the definition of Event defined in
// google3/google/internal/chrome/reporting/v1/chromereporting.proto. This is
// done because events to Chrome Reporting API are sent as json over HTTP, and
// has different proto definition compare to the proto used to store events
// locally.
base::Value ConvertProtoToValue(
    const em::AppInstallReportRequest* app_install_report_request,
    Profile* profile);

// Converts AppInstallReportLogEvent proto defined in
// components/policy/proto/device_management_backend.proto to a dictionary value
// that corresponds to the definition of AndroidAppInstallEvent defined in
// google3/chrome/cros/reporting/proto/chrome_app_install_events.proto.
base::Value ConvertEventToValue(
    const std::string& package,
    const em::AppInstallReportLogEvent& app_install_report_log_event,
    Profile* profile);

// Appends event_id to events in |event_list| by calculating hash of the (event,
// |context|) pair. If calculating hash is not possible for an event in
// |event_list|, event_id for that event will not be populated. event_id is used
// by "Chrome Reporting API" to deduplicate events.
void AppendEventId(base::Value* event_list, const base::Value& context);

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_APP_INSTALL_EVENT_LOG_UTIL_H_
