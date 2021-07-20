// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_INSTALL_EVENT_LOG_UTIL_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_INSTALL_EVENT_LOG_UTIL_H_

#include "components/policy/proto/device_management_backend.pb.h"
#include "extensions/common/extension_id.h"

namespace base {
class Value;
}  // namespace base

namespace em = enterprise_management;

namespace policy {

// Return serial number of the device.
std::string GetSerialNumber();

// Converts ExtensionInstallReportRequest proto defined in
// components/policy/proto/device_management_backend.proto to a dictionary value
// that corresponds to the definition of Event defined in
// google3/google/internal/chrome/reporting/v1/chromereporting.proto. This is
// done because events to Chrome Reporting API are sent as json over HTTP, and
// has different proto definition compare to the proto used to store events
// locally.
base::Value ConvertExtensionProtoToValue(
    const em::ExtensionInstallReportRequest* extension_install_report_request,
    const base::Value& context);

// Converts ExtensionInstallReportLogEvent proto defined in
// components/policy/proto/device_management_backend.proto to a dictionary value
// that corresponds to the definition of ExtensionAppInstallEvent defined in
// google3/chrome/cros/reporting/proto/chrome_extension_install_events.proto.
// Appends event_id to the event by calculating hash of the (event,
// |context|) pair, so long as the calculation is possible.
base::Value ConvertExtensionEventToValue(
    const extensions::ExtensionId& extension_id,
    const em::ExtensionInstallReportLogEvent&
        extension_install_report_log_event,
    const base::Value& context);

// Converts AppInstallReportRequest proto defined in
// components/policy/proto/device_management_backend.proto to a dictionary value
// that corresponds to the definition of Event defined in
// google3/google/internal/chrome/reporting/v1/chromereporting.proto. This is
// done because events to Chrome Reporting API are sent as json over HTTP, and
// has different proto definition compare to the proto used to store events
// locally.
base::Value ConvertArcAppProtoToValue(
    const em::AppInstallReportRequest* app_install_report_request,
    const base::Value& context);

// Converts AppInstallReportLogEvent proto defined in
// components/policy/proto/device_management_backend.proto to a dictionary value
// that corresponds to the definition of AndroidAppInstallEvent defined in
// google3/chrome/cros/reporting/proto/chrome_app_install_events.proto.
// Appends event_id to the event by calculating hash of the (event,
// |context|) pair, so long as the calculation is possible.
base::Value ConvertArcAppEventToValue(
    const std::string& package,
    const em::AppInstallReportLogEvent& app_install_report_log_event,
    const base::Value& context);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_INSTALL_EVENT_LOG_UTIL_H_
