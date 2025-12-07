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

base::Value::Dict CreateExtensionInfoDict(const ExtensionInfo& extension_info) {
  base::Value::Dict dict;
  dict.Set(ExtensionTelemetryEventRouter::kKeyId, extension_info.id());
  dict.Set(ExtensionTelemetryEventRouter::kKeyName, extension_info.name());
  dict.Set(ExtensionTelemetryEventRouter::kKeyVersion,
           extension_info.version());
  dict.Set(
      ExtensionTelemetryEventRouter::kKeyInstallLocation,
      ExtensionInfo::InstallLocation_Name(extension_info.install_location()));
  dict.Set(ExtensionTelemetryEventRouter::kKeyIsFromStore,
           extension_info.is_from_store());
  if (extension_info.file_infos_size() > 0) {
    base::Value::List file_infos_list;
    for (const auto& file_info : extension_info.file_infos()) {
      base::Value::Dict file_info_dict;
      file_info_dict.Set(ExtensionTelemetryEventRouter::kKeyName,
                         file_info.name());
      file_info_dict.Set(ExtensionTelemetryEventRouter::kKeyHash,
                         file_info.hash());
      file_infos_list.Append(std::move(file_info_dict));
    }
    dict.Set(ExtensionTelemetryEventRouter::kKeyFileInfo,
             std::move(file_infos_list));
  }

  return dict;
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

base::Value::Dict CreateCookiesGetAllInfoDict(
    const CookiesGetAllInfo& cookies_get_all_info) {
  base::Value::List get_all_args_list;
  for (const auto& get_all_args_info :
       cookies_get_all_info.get_all_args_info()) {
    base::Value::Dict get_all_args_dict;
    get_all_args_dict.Set(ExtensionTelemetryEventRouter::kKeyDomain,
                          get_all_args_info.domain());
    get_all_args_dict.Set(ExtensionTelemetryEventRouter::kKeyName,
                          get_all_args_info.name());
    get_all_args_dict.Set(ExtensionTelemetryEventRouter::kKeyPath,
                          get_all_args_info.path());
    get_all_args_dict.Set(ExtensionTelemetryEventRouter::kKeySecure,
                          get_all_args_info.secure());
    get_all_args_dict.Set(ExtensionTelemetryEventRouter::kKeyStoreId,
                          get_all_args_info.store_id());
    get_all_args_dict.Set(ExtensionTelemetryEventRouter::kKeyUrl,
                          get_all_args_info.url());
    get_all_args_dict.Set(ExtensionTelemetryEventRouter::kKeyIsSession,
                          get_all_args_info.is_session());
    get_all_args_dict.Set(ExtensionTelemetryEventRouter::kKeyCount,
                          static_cast<int>(get_all_args_info.count()));

    get_all_args_list.Append(std::move(get_all_args_dict));
  }

  base::Value::Dict signal_dict;
  signal_dict.Set(ExtensionTelemetryEventRouter::kKeyGetAllArgsInfo,
                  std::move(get_all_args_list));
  return signal_dict;
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

base::Value::Dict CreateCookiesGetInfoDict(
    const CookiesGetInfo& cookies_get_info) {
  base::Value::List get_args_list;
  for (const auto& get_args_info : cookies_get_info.get_args_info()) {
    base::Value::Dict get_args_dict;
    get_args_dict.Set(ExtensionTelemetryEventRouter::kKeyName,
                      get_args_info.name());
    get_args_dict.Set(ExtensionTelemetryEventRouter::kKeyUrl,
                      get_args_info.url());
    get_args_dict.Set(ExtensionTelemetryEventRouter::kKeyStoreId,
                      get_args_info.store_id());
    get_args_dict.Set(ExtensionTelemetryEventRouter::kKeyCount,
                      static_cast<int>(get_args_info.count()));

    get_args_list.Append(std::move(get_args_dict));
  }

  base::Value::Dict signal_dict;
  signal_dict.Set(ExtensionTelemetryEventRouter::kKeyGetArgsInfo,
                  std::move(get_args_list));
  return signal_dict;
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

base::Value::Dict CreateRemoteHostContactedInfoDict(
    const RemoteHostContactedInfo& remote_host_contacted_info) {
  base::Value::List remote_host_info_list;
  for (const auto& remote_host_info :
       remote_host_contacted_info.remote_host()) {
    base::Value::Dict remote_host_info_dict;
    remote_host_info_dict.Set(ExtensionTelemetryEventRouter::kKeyUrl,
                              remote_host_info.url());
    remote_host_info_dict.Set(
        ExtensionTelemetryEventRouter::kKeyConnectionProtocol,
        RemoteHostInfo::ProtocolType_Name(
            remote_host_info.connection_protocol()));
    remote_host_info_dict.Set(
        ExtensionTelemetryEventRouter::kKeyContactedBy,
        RemoteHostInfo::ContactInitiator_Name(remote_host_info.contacted_by()));
    remote_host_info_dict.Set(
        ExtensionTelemetryEventRouter::kKeyContactCount,
        static_cast<int>(remote_host_info.contact_count()));

    remote_host_info_list.Append(std::move(remote_host_info_dict));
  }

  base::Value::Dict signal_dict;
  signal_dict.Set(ExtensionTelemetryEventRouter::kKeyRemoteHost,
                  std::move(remote_host_info_list));
  return signal_dict;
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

base::Value::Dict CreateTabsApiInfoDict(const TabsApiInfo& tabs_api_info) {
  base::Value::List tabs_api_info_list;
  for (const auto& call_detail : tabs_api_info.call_details()) {
    base::Value::Dict tabs_api_info_dict;
    tabs_api_info_dict.Set(ExtensionTelemetryEventRouter::kKeyMethod,
                           TabsApiInfo::ApiMethod_Name(call_detail.method()));
    tabs_api_info_dict.Set(ExtensionTelemetryEventRouter::kKeyNewUrl,
                           call_detail.new_url());
    tabs_api_info_dict.Set(ExtensionTelemetryEventRouter::kKeyCurrentUrl,
                           call_detail.current_url());
    tabs_api_info_dict.Set(ExtensionTelemetryEventRouter::kKeyCount,
                           static_cast<int>(call_detail.count()));

    tabs_api_info_list.Append(std::move(tabs_api_info_dict));
  }

  base::Value::Dict signal_dict;
  signal_dict.Set(ExtensionTelemetryEventRouter::kKeyCallDetails,
                  std::move(tabs_api_info_list));
  return signal_dict;
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
      }
    }
  }
  return redacted_request;
}

base::Value::Dict CreateExtensionTelemetryReportDict(
    const ExtensionTelemetryReportRequest::Report& report) {
  base::Value::Dict report_dict;
  report_dict.Set(ExtensionTelemetryEventRouter::kKeyExtension,
                  CreateExtensionInfoDict(report.extension()));

  base::Value::Dict signals_dict;
  for (const auto& signal : report.signals()) {
    if (signal.has_cookies_get_all_info()) {
      signals_dict.Set(
          ExtensionTelemetryEventRouter::kKeyCookiesGetAllInfo,
          CreateCookiesGetAllInfoDict(signal.cookies_get_all_info()));
    } else if (signal.has_cookies_get_info()) {
      signals_dict.Set(ExtensionTelemetryEventRouter::kKeyCookiesGetInfo,
                       CreateCookiesGetInfoDict(signal.cookies_get_info()));
    } else if (signal.has_remote_host_contacted_info()) {
      signals_dict.Set(
          ExtensionTelemetryEventRouter::kKeyRemoteHostContactedInfo,
          CreateRemoteHostContactedInfoDict(
              signal.remote_host_contacted_info()));
    } else if (signal.has_tabs_api_info()) {
      signals_dict.Set(ExtensionTelemetryEventRouter::kKeyTabsApiInfo,
                       CreateTabsApiInfoDict(signal.tabs_api_info()));
    }
  }

  report_dict.Set(ExtensionTelemetryEventRouter::kKeySignals,
                  std::move(signals_dict));
  return report_dict;
}

base::Value::Dict CreateExtensionTelemetryReportRequestDict(
    const ExtensionTelemetryReportRequest& request) {
  base::Value::List report_list;
  for (const auto& telemetry_report : request.reports()) {
    report_list.Append(CreateExtensionTelemetryReportDict(telemetry_report));
  }

  base::Value::Dict request_dict;
  request_dict.Set(ExtensionTelemetryEventRouter::kKeyReports,
                   std::move(report_list));
  request_dict.Set(ExtensionTelemetryEventRouter::kKeyCreationTimeMsec,
                   base::NumberToString(request.creation_timestamp_msec()));

  return base::Value::Dict().Set(
      ExtensionTelemetryEventRouter::kKeyExtensionTelemetryReport,
      std::move(request_dict));
}

}  // namespace

const char ExtensionTelemetryEventRouter::kKeyExtensionTelemetryReport[] =
    "extension_telemetry_report";
const char ExtensionTelemetryEventRouter::kKeyCreationTimeMsec[] =
    "creation_timestamp_msec";
const char ExtensionTelemetryEventRouter::kKeyReports[] = "reports";
const char ExtensionTelemetryEventRouter::kKeyExtension[] = "extension";
const char ExtensionTelemetryEventRouter::kKeySignals[] = "signals";
const char ExtensionTelemetryEventRouter::kKeyCookiesGetAllInfo[] =
    "cookies_get_all_info";
const char ExtensionTelemetryEventRouter::kKeyGetAllArgsInfo[] =
    "get_all_args_info";
const char ExtensionTelemetryEventRouter::kKeyCookiesGetInfo[] =
    "cookies_get_info";
const char ExtensionTelemetryEventRouter::kKeyGetArgsInfo[] = "get_args_info";
const char ExtensionTelemetryEventRouter::kKeyRemoteHostContactedInfo[] =
    "remote_host_contacted_info";
const char ExtensionTelemetryEventRouter::kKeyRemoteHost[] = "remote_host";
const char ExtensionTelemetryEventRouter::kKeyTabsApiInfo[] = "tabs_api_info";
const char ExtensionTelemetryEventRouter::kKeyCallDetails[] = "call_details";
const char ExtensionTelemetryEventRouter::kKeyId[] = "id";
const char ExtensionTelemetryEventRouter::kKeyVersion[] = "version";
const char ExtensionTelemetryEventRouter::kKeyName[] = "name";
const char ExtensionTelemetryEventRouter::kKeyInstallLocation[] =
    "install_location";
const char ExtensionTelemetryEventRouter::kKeyIsFromStore[] = "is_from_store";
const char ExtensionTelemetryEventRouter::kKeyUrl[] = "url";
const char ExtensionTelemetryEventRouter::kKeyConnectionProtocol[] =
    "connection_protocol";
const char ExtensionTelemetryEventRouter::kKeyContactedBy[] = "contacted_by";
const char ExtensionTelemetryEventRouter::kKeyContactCount[] = "contact_count";
const char ExtensionTelemetryEventRouter::kKeyDomain[] = "domain";
const char ExtensionTelemetryEventRouter::kKeyPath[] = "path";
const char ExtensionTelemetryEventRouter::kKeySecure[] = "secure";
const char ExtensionTelemetryEventRouter::kKeyStoreId[] = "store_id";
const char ExtensionTelemetryEventRouter::kKeyIsSession[] = "is_session";
const char ExtensionTelemetryEventRouter::kKeyCount[] = "count";
const char ExtensionTelemetryEventRouter::kKeyMethod[] = "method";
const char ExtensionTelemetryEventRouter::kKeyNewUrl[] = "new_url";
const char ExtensionTelemetryEventRouter::kKeyCurrentUrl[] = "current_url";
const char ExtensionTelemetryEventRouter::kKeyFileInfo[] = "file_info";
const char ExtensionTelemetryEventRouter::kKeyHash[] = "hash";

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

bool ExtensionTelemetryEventRouter::IsPolicyEnabled() {
  auto* reporting_client =
      RealtimeReportingClientFactory::GetForProfile(context_);
  if (!reporting_client) {
    return false;
  }

  std::optional<ReportingSettings> settings =
      reporting_client->GetReportingSettings();
  return settings.has_value() &&
         settings->enabled_opt_in_events.count(kExtensionTelemetryEvent) > 0;
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

  if (base::FeatureList::IsEnabled(
          policy::kUploadRealtimeReportingEventsUsingProto)) {
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
  } else {
    reporting_client->ReportRealtimeEvent(
        kExtensionTelemetryEvent, std::move(settings.value()),
        CreateExtensionTelemetryReportRequestDict(*telemetry_report_request));
  }
}

#undef COPY_IF_SET

}  // namespace enterprise_connectors
