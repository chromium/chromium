// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_INSTALL_EVENT_LOG_UTIL_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_INSTALL_EVENT_LOG_UTIL_H_

#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/app_install_events.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "extensions/common/extension_id.h"

namespace em = enterprise_management;

namespace policy {

// Return serial number of the device.
std::string GetSerialNumber();

// Converts ExtensionInstallReportRequest proto defined in
// components/policy/proto/device_management_backend.proto to a list of
// dictionaries, each corresponding to the Event message defined in
// google3/google/internal/chrome/reporting/v1/chrome_reporting_entity.proto.
// This is done because events to Chrome Reporting API are sent as json over
// HTTP, and has different proto definition compare to the proto used to store
// events locally.
base::ListValue ConvertExtensionProtoToValue(
    const em::ExtensionInstallReportRequest* extension_install_report_request,
    const base::DictValue& context);

// Converts ExtensionInstallReportLogEvent proto defined in
// components/policy/proto/device_management_backend.proto to a dictionary value
// that corresponds to the definition of ExtensionAppInstallEvent defined in
// google3/chrome/cros/reporting/proto/chrome_extension_install_events.proto.
// Appends event_id to the event by calculating hash of the (event,
// |context|) pair, so long as the calculation is possible.
base::DictValue ConvertExtensionEventToValue(
    const extensions::ExtensionId& extension_id,
    const em::ExtensionInstallReportLogEvent&
        extension_install_report_log_event,
    const base::DictValue& context);

// Converts AppInstallReportRequest proto defined in
// components/policy/proto/device_management_backend.proto to a list of
// dictionaries, each corresponding to the Event message defined in
// google3/google/internal/chrome/reporting/v1/chrome_reporting_entity.proto.
// This is done because events to Chrome Reporting API are sent as json over
// HTTP, and has different proto definition compare to the proto used to store
// events locally.
base::ListValue ConvertArcAppProtoToValue(
    const em::AppInstallReportRequest* app_install_report_request,
    const base::DictValue& context);

// Converts AppInstallReportLogEvent proto defined in
// components/policy/proto/device_management_backend.proto to a dictionary value
// that corresponds to the definition of AndroidAppInstallEvent defined in
// google3/chrome/cros/reporting/proto/chrome_app_install_events.proto.
// Appends event_id to the event by calculating hash of the (event,
// |context|) pair, so long as the calculation is possible.
base::DictValue ConvertArcAppEventToValue(
    const std::string& package,
    const em::AppInstallReportLogEvent& app_install_report_log_event,
    const base::DictValue& context);

reporting::AndroidAppInstallEvent CreateAndroidAppInstallEvent(
    const std::string& package,
    const enterprise_management::AppInstallReportLogEvent& event);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_INSTALL_EVENT_LOG_UTIL_H_
