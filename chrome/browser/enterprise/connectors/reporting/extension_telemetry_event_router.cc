// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/extension_telemetry_event_router.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/enterprise/connectors/reporting/extension_telemetry_event_router_factory.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/reporting_service_settings.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/safe_browsing/core/common/features.h"

namespace enterprise_connectors {

namespace {

using ::google::protobuf::RepeatedPtrField;
using ExtensionInfo =
    safe_browsing::ExtensionTelemetryReportRequest_ExtensionInfo;
using ExtensionTelemetryReportRequest =
    safe_browsing::ExtensionTelemetryReportRequest;
using RemoteHostInfo = safe_browsing::
    ExtensionTelemetryReportRequest_SignalInfo_RemoteHostContactedInfo_RemoteHostInfo;
using CookiesGetAllInfo =
    safe_browsing::ExtensionTelemetryReportRequest_SignalInfo_CookiesGetAllInfo;
using CookiesGetInfo =
    safe_browsing::ExtensionTelemetryReportRequest_SignalInfo_CookiesGetInfo;
using RemoteHostContactedInfo = safe_browsing::
    ExtensionTelemetryReportRequest_SignalInfo_RemoteHostContactedInfo;
using TabsApiInfo =
    safe_browsing::ExtensionTelemetryReportRequest_SignalInfo_TabsApiInfo;
using Report = safe_browsing::ExtensionTelemetryReportRequest_Report;
using DOMAccessInfo =
    safe_browsing::ExtensionTelemetryReportRequest_SignalInfo_DOMAccessInfo;
using ScriptInjectionInfo = safe_browsing::
    ExtensionTelemetryReportRequest_SignalInfo_ScriptInjectionInfo;

#ifndef COPY_IF_SET
#define COPY_IF_SET(source, dest_ptr, field)   \
  if ((source).has_##field()) {                \
    (dest_ptr)->set_##field((source).field()); \
  }
#endif  // COPY_IF_SET

void CopyExtensionInfo(const ExtensionInfo& original_extension,
                       Report* redacted_report) {
  ExtensionInfo* redacted_extension = redacted_report->mutable_extension();
  COPY_IF_SET(original_extension, redacted_extension, id);
  COPY_IF_SET(original_extension, redacted_extension, name);
  COPY_IF_SET(original_extension, redacted_extension, version);
  COPY_IF_SET(original_extension, redacted_extension, install_location);
  COPY_IF_SET(original_extension, redacted_extension, is_from_store);
  if (original_extension.file_infos_size() > 0) {
    for (const auto& file_info : original_extension.file_infos()) {
      auto* redacted_file_info = redacted_extension->add_file_infos();
      COPY_IF_SET(file_info, redacted_file_info, name);
      COPY_IF_SET(file_info, redacted_file_info, hash);
    }
  }
}


void CopyCookiesGetAllArgsInfo(const CookiesGetAllInfo& cookies_get_all_info,
                               Report* redacted_report) {
  CookiesGetAllInfo* redacted_cookies_get_all_info =
      redacted_report->add_signals()->mutable_cookies_get_all_info();

  for (const auto& get_all_args_info :
       cookies_get_all_info.get_all_args_info()) {
    CookiesGetAllInfo::GetAllArgsInfo* redacted_get_all_args_info =
        redacted_cookies_get_all_info->add_get_all_args_info();
    COPY_IF_SET(get_all_args_info, redacted_get_all_args_info, domain);
    COPY_IF_SET(get_all_args_info, redacted_get_all_args_info, name);
    COPY_IF_SET(get_all_args_info, redacted_get_all_args_info, path);
    COPY_IF_SET(get_all_args_info, redacted_get_all_args_info, secure);
    COPY_IF_SET(get_all_args_info, redacted_get_all_args_info, store_id);
    COPY_IF_SET(get_all_args_info, redacted_get_all_args_info, url);
    COPY_IF_SET(get_all_args_info, redacted_get_all_args_info, is_session);
    COPY_IF_SET(get_all_args_info, redacted_get_all_args_info, count);
  }
}


void CopyCookiesGetInfo(const CookiesGetInfo& cookies_get_info,
                        Report* redacted_report) {
  CookiesGetInfo* redacted_cookies_get_info =
      redacted_report->add_signals()->mutable_cookies_get_info();
  for (const auto& get_args_info : cookies_get_info.get_args_info()) {
    CookiesGetInfo::GetArgsInfo* redacted_get_args_info =
        redacted_cookies_get_info->add_get_args_info();
    COPY_IF_SET(get_args_info, redacted_get_args_info, name);
    COPY_IF_SET(get_args_info, redacted_get_args_info, url);
    COPY_IF_SET(get_args_info, redacted_get_args_info, store_id);
    COPY_IF_SET(get_args_info, redacted_get_args_info, count);
  }
}


void CopyRemoteHostContactedInfo(
    const RemoteHostContactedInfo& remote_host_contacted_info,
    Report* redacted_report) {
  RemoteHostContactedInfo* redacted_remote_host_contacted_info =
      redacted_report->add_signals()->mutable_remote_host_contacted_info();
  for (const auto& remote_host_info :
       remote_host_contacted_info.remote_host()) {
    RemoteHostContactedInfo::RemoteHostInfo* redacted_remote_host_info =
        redacted_remote_host_contacted_info->add_remote_host();
    COPY_IF_SET(remote_host_info, redacted_remote_host_info, url);
    COPY_IF_SET(remote_host_info, redacted_remote_host_info,
                connection_protocol);
    COPY_IF_SET(remote_host_info, redacted_remote_host_info, contacted_by);
    COPY_IF_SET(remote_host_info, redacted_remote_host_info, contact_count);
  }
}

void CopyTabsApiInfo(const TabsApiInfo& tabs_api_info,
                     Report* redacted_report) {
  TabsApiInfo* redacted_tabs_api_info =
      redacted_report->add_signals()->mutable_tabs_api_info();
  for (const auto& call_detail : tabs_api_info.call_details()) {
    TabsApiInfo::CallDetails* redacted_call_details =
        redacted_tabs_api_info->add_call_details();
    COPY_IF_SET(call_detail, redacted_call_details, method);
    COPY_IF_SET(call_detail, redacted_call_details, new_url);
    COPY_IF_SET(call_detail, redacted_call_details, current_url);
    COPY_IF_SET(call_detail, redacted_call_details, count);
  }
}


void CopyDOMAccessInfo(const DOMAccessInfo& dom_access_info,
                       Report* redacted_report) {
  DOMAccessInfo* redacted_dom_access_info =
      redacted_report->add_signals()->mutable_dom_access_info();
  for (const auto& dom_access : dom_access_info.dom_accesses()) {
    DOMAccessInfo::DOMAccess* redacted_dom_access =
        redacted_dom_access_info->add_dom_accesses();
    COPY_IF_SET(dom_access, redacted_dom_access, api_name);
    COPY_IF_SET(dom_access, redacted_dom_access, url);
    COPY_IF_SET(dom_access, redacted_dom_access, access_type);
    COPY_IF_SET(dom_access, redacted_dom_access, count);
    COPY_IF_SET(dom_access, redacted_dom_access, timestamp_ms);
  }
}

void CopyScriptInjectionInfo(const ScriptInjectionInfo& script_injection_info,
                             Report* redacted_report) {
  ScriptInjectionInfo* redacted_script_injection_info =
      redacted_report->add_signals()->mutable_script_injection_info();
  for (const auto& script_injection :
       script_injection_info.script_injections()) {
    ScriptInjectionInfo::ScriptInjection* redacted_script_injection =
        redacted_script_injection_info->add_script_injections();
    COPY_IF_SET(script_injection, redacted_script_injection, api_name);
    COPY_IF_SET(script_injection, redacted_script_injection, url);
    COPY_IF_SET(script_injection, redacted_script_injection, count);
    COPY_IF_SET(script_injection, redacted_script_injection, timestamp_ms);
    if (script_injection.args_list_size() > 0) {
      *redacted_script_injection->mutable_args_list() =
          script_injection.args_list();
    }
    COPY_IF_SET(script_injection, redacted_script_injection, arg_url);
  }
}


std::unique_ptr<ExtensionTelemetryReportRequest>
CreateRedactedExtensionTelemetryReportRequestProto(
    const ExtensionTelemetryReportRequest* request) {
  auto redacted_request = std::make_unique<ExtensionTelemetryReportRequest>();

  redacted_request->set_creation_timestamp_msec(
      request->creation_timestamp_msec());

  for (const auto& report : request->reports()) {
    Report* redacted_report = redacted_request->add_reports();

    CopyExtensionInfo(report.extension(), redacted_report);

    // Copy select subset of signals.
    for (const auto& signal : report.signals()) {
      if (signal.has_cookies_get_all_info()) {
        CopyCookiesGetAllArgsInfo(signal.cookies_get_all_info(),
                                  redacted_report);
      } else if (signal.has_cookies_get_info()) {
        CopyCookiesGetInfo(signal.cookies_get_info(), redacted_report);
      } else if (signal.has_remote_host_contacted_info()) {
        CopyRemoteHostContactedInfo(signal.remote_host_contacted_info(),
                                    redacted_report);
      } else if (signal.has_tabs_api_info()) {
        CopyTabsApiInfo(signal.tabs_api_info(), redacted_report);
      } else if (signal.has_dom_access_info()) {
        CopyDOMAccessInfo(signal.dom_access_info(), redacted_report);
      } else if (signal.has_script_injection_info()) {
        CopyScriptInjectionInfo(signal.script_injection_info(),
                                redacted_report);
      }
    }
  }
  return redacted_request;
}


}  // namespace

// static
ExtensionTelemetryEventRouter* ExtensionTelemetryEventRouter::Get(
    Profile* profile) {
  return ExtensionTelemetryEventRouterFactory::GetInstance()->GetForProfile(
      profile);
}

ExtensionTelemetryEventRouter::ExtensionTelemetryEventRouter(
    content::BrowserContext* context)
    : context_(context) {}

ExtensionTelemetryEventRouter::~ExtensionTelemetryEventRouter() = default;

bool ExtensionTelemetryEventRouter::IsReportingEnabledForEvent(
    const char* event_name) {
  auto* reporting_client =
      RealtimeReportingClientFactory::GetForProfile(context_);
  if (!reporting_client) {
    return false;
  }

  std::optional<ReportingSettings> settings =
      reporting_client->GetReportingSettings();
  return settings.has_value() &&
         settings->enabled_opt_in_events.count(event_name) > 0;
}

bool ExtensionTelemetryEventRouter::IsPolicyEnabled() {
  return IsReportingEnabledForEvent(kExtensionTelemetryEvent);
}

bool ExtensionTelemetryEventRouter::IsDOMActivityTelemetryEnabled() {
  return IsReportingEnabledForEvent(kExtensionDOMActivityEvent);
}

void ExtensionTelemetryEventRouter::UploadTelemetryReport(
    std::unique_ptr<safe_browsing::ExtensionTelemetryReportRequest>
        telemetry_report_request) {
  if (!IsPolicyEnabled()) {
    return;
  }

  auto* reporting_client =
      RealtimeReportingClientFactory::GetForProfile(context_);
  CHECK(reporting_client);
  std::optional<ReportingSettings> settings =
      reporting_client->GetReportingSettings();

  chrome::cros::reporting::proto::ExtensionTelemetryEvent
      extension_telemetry_event;
  *extension_telemetry_event.mutable_extension_telemetry_report() =
      *CreateRedactedExtensionTelemetryReportRequestProto(
          telemetry_report_request.get());
  extension_telemetry_event.set_profile_identifier(
      reporting_client->GetProfileIdentifier());
  extension_telemetry_event.set_profile_user_name(
      reporting_client->GetProfileUserName());

  chrome::cros::reporting::proto::Event event;
  *event.mutable_extension_telemetry_event() = extension_telemetry_event;

  reporting_client->ReportEvent(std::move(event), settings.value());
}

#undef COPY_IF_SET

}  // namespace enterprise_connectors
