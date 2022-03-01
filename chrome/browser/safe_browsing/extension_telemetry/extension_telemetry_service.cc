// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"

#include <sstream>
#include <vector>

#include "base/containers/contains.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_uploader.h"
#include "chrome/browser/safe_browsing/extension_telemetry/remote_host_contacted_signal_processor.h"
#include "chrome/browser/safe_browsing/extension_telemetry/tabs_execute_script_signal_processor.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

namespace {

using ::extensions::mojom::ManifestLocation;
using ::google::protobuf::RepeatedPtrField;
using ExtensionInfo =
    ::safe_browsing::ExtensionTelemetryReportRequest_ExtensionInfo;

void RecordSignalType(ExtensionSignalType signal_type) {
  base::UmaHistogramEnumeration(
      "SafeBrowsing.ExtensionTelemetry.Signals.SignalType", signal_type);
}

static_assert(extensions::Manifest::NUM_LOAD_TYPES == 10,
              "ExtensionTelemetryReportRequest::ExtensionInfo::Type "
              "needs to match extensions::Manifest::Type.");
ExtensionInfo::Type GetType(extensions::Manifest::Type type) {
  switch (type) {
    case extensions::Manifest::TYPE_UNKNOWN:
      return ExtensionInfo::UNKNOWN_TYPE;
    case extensions::Manifest::TYPE_EXTENSION:
      return ExtensionInfo::EXTENSION;
    case extensions::Manifest::TYPE_THEME:
      return ExtensionInfo::THEME;
    case extensions::Manifest::TYPE_USER_SCRIPT:
      return ExtensionInfo::USER_SCRIPT;
    case extensions::Manifest::TYPE_HOSTED_APP:
      return ExtensionInfo::HOSTED_APP;
    case extensions::Manifest::TYPE_LEGACY_PACKAGED_APP:
      return ExtensionInfo::LEGACY_PACKAGED_APP;
    case extensions::Manifest::TYPE_PLATFORM_APP:
      return ExtensionInfo::PLATFORM_APP;
    case extensions::Manifest::TYPE_SHARED_MODULE:
      return ExtensionInfo::SHARED_MODULE;
    case extensions::Manifest::TYPE_LOGIN_SCREEN_EXTENSION:
      return ExtensionInfo::LOGIN_SCREEN_EXTENSION;
    case extensions::Manifest::TYPE_CHROMEOS_SYSTEM_EXTENSION:
      // TODO(mgawad): introduce new CHROMEOS_SYSTEM_EXTENSION type.
      return ExtensionInfo::EXTENSION;
    default:
      return ExtensionInfo::UNKNOWN_TYPE;
  }
}

static_assert(static_cast<int>(ManifestLocation::kMaxValue) ==
                  static_cast<int>(ExtensionInfo::EXTERNAL_COMPONENT),
              "ExtensionTelemetryReportRequest::ExtensionInfo::InstallLocation "
              "needs to match extensions::mojom::ManifestLocation.");
ExtensionInfo::InstallLocation GetInstallLocation(ManifestLocation location) {
  switch (location) {
    case ManifestLocation::kInvalidLocation:
      return ExtensionInfo::UNKNOWN_LOCATION;
    case ManifestLocation::kInternal:
      return ExtensionInfo::INTERNAL;
    case ManifestLocation::kExternalPref:
      return ExtensionInfo::EXTERNAL_PREF;
    case ManifestLocation::kExternalRegistry:
      return ExtensionInfo::EXTERNAL_REGISTRY;
    case ManifestLocation::kUnpacked:
      return ExtensionInfo::UNPACKED;
    case ManifestLocation::kComponent:
      return ExtensionInfo::COMPONENT;
    case ManifestLocation::kExternalPrefDownload:
      return ExtensionInfo::EXTERNAL_PREF_DOWNLOAD;
    case ManifestLocation::kExternalPolicyDownload:
      return ExtensionInfo::EXTERNAL_POLICY_DOWNLOAD;
    case ManifestLocation::kCommandLine:
      return ExtensionInfo::COMMAND_LINE;
    case ManifestLocation::kExternalPolicy:
      return ExtensionInfo::EXTERNAL_POLICY;
    case ManifestLocation::kExternalComponent:
      return ExtensionInfo::EXTERNAL_COMPONENT;
  }
  return ExtensionInfo::UNKNOWN_LOCATION;
}

ExtensionInfo::BlocklistState GetBlocklistState(
    const extensions::ExtensionId extension_id,
    extensions::ExtensionPrefs* extension_prefs) {
  extensions::BitMapBlocklistState state =
      extensions::blocklist_prefs::GetExtensionBlocklistState(extension_id,
                                                              extension_prefs);
  switch (state) {
    case extensions::BitMapBlocklistState::NOT_BLOCKLISTED:
      return ExtensionInfo::NOT_BLOCKLISTED;
    case extensions::BitMapBlocklistState::BLOCKLISTED_MALWARE:
      return ExtensionInfo::BLOCKLISTED_MALWARE;
    case extensions::BitMapBlocklistState::BLOCKLISTED_SECURITY_VULNERABILITY:
      return ExtensionInfo::BLOCKLISTED_SECURITY_VULNERABILITY;
    case extensions::BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION:
      return ExtensionInfo::BLOCKLISTED_CWS_POLICY_VIOLATION;
    case extensions::BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED:
      return ExtensionInfo::BLOCKLISTED_POTENTIALLY_UNWANTED;
    default:
      return ExtensionInfo::BLOCKLISTED_UNKNOWN;
  }
}

}  // namespace

ExtensionTelemetryService::~ExtensionTelemetryService() = default;

ExtensionTelemetryService::ExtensionTelemetryService(
    Profile* profile,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    extensions::ExtensionRegistry* extension_registry,
    extensions::ExtensionPrefs* extension_prefs)
    : profile_(profile),
      url_loader_factory_(url_loader_factory),
      extension_registry_(extension_registry),
      extension_prefs_(extension_prefs),
      enabled_(false),
      current_reporting_interval_(
          base::Seconds(kExtensionTelemetryUploadIntervalSeconds.Get())) {
  // Register for SB preference change notifications.
  pref_service_ = profile_->GetPrefs();
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kSafeBrowsingEnhanced,
      base::BindRepeating(&ExtensionTelemetryService::OnPrefChanged,
                          base::Unretained(this)));

  // Set initial enable/disable state.
  SetEnabled(IsEnhancedProtectionEnabled(*pref_service_));
}

void ExtensionTelemetryService::OnPrefChanged() {
  SetEnabled(IsEnhancedProtectionEnabled(*pref_service_));
}

void ExtensionTelemetryService::SetEnabled(bool enable) {
  // Make call idempotent.
  if (enabled_ == enable)
    return;

  enabled_ = enable;

  if (enabled_) {
    // Create signal processors.
    signal_processors_.emplace(
        ExtensionSignalType::kTabsExecuteScript,
        std::make_unique<TabsExecuteScriptSignalProcessor>());
    signal_processors_.emplace(
        ExtensionSignalType::kRemoteHostContacted,
        std::make_unique<RemoteHostContactedSignalProcessor>());

    // Start timer for sending periodic telemetry reports if reporting interval
    // is not 0. An interval of 0 effectively turns off creation and uploading
    // of telemetry reports.
    if (current_reporting_interval_.is_positive()) {
      timer_.Start(FROM_HERE, current_reporting_interval_, this,
                   &ExtensionTelemetryService::CreateAndUploadReport);
    }
  } else {
    // Stop timer for periodic telemetry reports.
    timer_.Stop();
    // Clear all data stored by the service.
    extension_store_.clear();
    // Destruct signal processors.
    signal_processors_.clear();
  }
}

void ExtensionTelemetryService::Shutdown() {
  timer_.Stop();
  pref_change_registrar_.RemoveAll();
}

void ExtensionTelemetryService::AddSignal(
    std::unique_ptr<ExtensionSignal> signal) {
  ExtensionSignalType signal_type = signal->GetType();
  RecordSignalType(signal_type);

  DCHECK(base::Contains(signal_processors_, signal_type));
  ExtensionSignalProcessor& processor = *signal_processors_[signal_type];

  if (extension_store_.find(signal->extension_id()) == extension_store_.end()) {
    // This is the first signal triggered by this extension since the last
    // time a report was generated for it. Store its information.
    // Note: The extension information is cached at signal addition time
    // instead of being calculated at report generation. This approach handles
    // the case where the extension is uninstalled after generating the signal
    // but before a report is generated. The extension information is also
    // cleared after each telemetry report is sent to keep the data fresh.
    const extensions::Extension* extension =
        extension_registry_->GetInstalledExtension(signal->extension_id());
    // The signal is added synchronously and it should never be reported for
    // a non-existent extension.
    DCHECK(extension);
    extension_store_.emplace(signal->extension_id(),
                             GetExtensionInfoForReport(*extension));
  }

  processor.ProcessSignal(std::move(signal));
}

void ExtensionTelemetryService::CreateAndUploadReport() {
  DCHECK(enabled_);

  active_report_ = CreateReport();
  if (!active_report_)
    return;

  auto upload_data = std::make_unique<std::string>();
  if (!active_report_->SerializeToString(upload_data.get())) {
    active_report_.reset();
    return;
  }

  auto callback =
      base::BindOnce(&ExtensionTelemetryService::OnUploadComplete,
                     weak_factory_.GetWeakPtr(), active_report_.get());
  active_uploader_ = std::make_unique<ExtensionTelemetryUploader>(
      std::move(callback), url_loader_factory_, std::move(upload_data));
  active_uploader_->Start();
}

void ExtensionTelemetryService::OnUploadComplete(
    ExtensionTelemetryReportRequest* report,
    bool /* success */) {
  DCHECK(report && (report == active_report_.get()));

  // Clean up the upload resources.
  active_report_.reset();
  active_uploader_.reset();
}

std::unique_ptr<ExtensionTelemetryReportRequest>
ExtensionTelemetryService::CreateReport() {
  // Don't create a telemetry report if there were no signals generated (i.e.,
  // extension store is empty) AND there are no installed extensions currently.
  std::unique_ptr<extensions::ExtensionSet> installed_extensions =
      extension_registry_->GenerateInstalledExtensionsSet();
  if (extension_store_.empty() && installed_extensions->is_empty())
    return nullptr;

  auto telemetry_report_pb =
      std::make_unique<ExtensionTelemetryReportRequest>();
  RepeatedPtrField<ExtensionTelemetryReportRequest_Report>* reports_pb =
      telemetry_report_pb->mutable_reports();

  // Create per-extension reports for all the extensions in the extension store.
  // These represent extensions that have signal information to report.
  for (auto& extension_store_it : extension_store_) {
    auto report_entry_pb =
        std::make_unique<ExtensionTelemetryReportRequest_Report>();

    // Populate all signal info for the extension by querying the signal
    // processors.
    RepeatedPtrField<ExtensionTelemetryReportRequest_SignalInfo>* signals_pb =
        report_entry_pb->mutable_signals();
    for (auto& processor_it : signal_processors_) {
      std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
          signal_info_pb = processor_it.second->GetSignalInfoForReport(
              extension_store_it.first);
      if (signal_info_pb) {
        signals_pb->AddAllocated(signal_info_pb.release());
      }
    }

    report_entry_pb->set_allocated_extension(
        extension_store_it.second.release());
    reports_pb->AddAllocated(report_entry_pb.release());
  }

  // Create per-extension reports for all the installed extensions. Exclude
  // extension store extensions since reports have already been created for
  // them. Note that these installed extension reports will only contain
  // extension information (and no signal data).
  for (const auto& entry : extension_store_) {
    installed_extensions->Remove(entry.first /* extension_id */);
  }

  for (const scoped_refptr<const extensions::Extension> installed_entry :
       *installed_extensions) {
    auto report_entry_pb =
        std::make_unique<ExtensionTelemetryReportRequest_Report>();

    report_entry_pb->set_allocated_extension(
        GetExtensionInfoForReport(*installed_entry.get()).release());
    reports_pb->AddAllocated(report_entry_pb.release());
  }

  DCHECK(!reports_pb->empty());

  // Clear out the extension store data to ensure that:
  // - extension info is refreshed for every telemetry report.
  // - no stale extension entry is left over in the extension store.
  extension_store_.clear();

  telemetry_report_pb->set_creation_timestamp_msec(
      base::Time::Now().ToJavaTime());
  return telemetry_report_pb;
}

void ExtensionTelemetryService::DumpReportForTest(
    const ExtensionTelemetryReportRequest& report) {
  base::Time creation_time =
      base::Time::FromJavaTime(report.creation_timestamp_msec());
  std::stringstream ss;
  ss << "Report creation time: "
     << base::UTF16ToUTF8(TimeFormatShortDateAndTimeWithTimeZone(creation_time))
     << "\n";

  const RepeatedPtrField<ExtensionTelemetryReportRequest_Report>& reports =
      report.reports();

  for (const auto& report_pb : reports) {
    const auto& extension_pb = report_pb.extension();
    base::Time install_time =
        base::Time::FromJavaTime(extension_pb.install_timestamp_msec());
    ss << "\nExtensionId: " << extension_pb.id() << "\n"
       << "  Name: " << extension_pb.name() << "\n"
       << "  Version: " << extension_pb.version() << "\n"
       << "  InstallTime: "
       << base::UTF16ToUTF8(
              TimeFormatShortDateAndTimeWithTimeZone(install_time))
       << "\n"
       << "  InstalledByDefault: "
       << (extension_pb.is_default_installed() ? "Y" : "N")
       << "  InstalledByOEM: " << (extension_pb.is_oem_installed() ? "Y" : "N")
       << "\n"
       << "  InstalledFromCWS: " << (extension_pb.is_from_store() ? "Y" : "N")
       << "  UpdatesFromCWS: "
       << (extension_pb.updates_from_store() ? "Y" : "N") << "\n"
       << "  ConvertedFromUserScript: "
       << (extension_pb.is_converted_from_user_script() ? "Y" : "N") << "\n"
       << "  Type: " << extension_pb.type()
       << "  InstallLocation: " << extension_pb.install_location()
       << "  BlocklistState: " << extension_pb.blocklist_state() << "\n"
       << "  DisableReasons: 0x" << std::hex << extension_pb.disable_reasons()
       << "\n";

    const RepeatedPtrField<ExtensionTelemetryReportRequest_SignalInfo>&
        signals = report_pb.signals();
    for (const auto& signal_pb : signals) {
      const auto& tabs_execute_script_info_pb =
          signal_pb.tabs_execute_script_info();
      const RepeatedPtrField<
          ExtensionTelemetryReportRequest_SignalInfo_TabsExecuteScriptInfo_ScriptInfo>&
          scripts = tabs_execute_script_info_pb.scripts();
      if (!scripts.empty()) {
        ss << "  Signal: TabsExecuteScript\n";
        for (const auto& script_pb : scripts) {
          ss << "    Script hash: "
             << base::HexEncode(script_pb.hash().c_str(),
                                script_pb.hash().size())
             << " count: " << script_pb.execution_count() << "\n";
        }
      }
    }
  }

  DVLOG(1) << "Telemetry Report: " << ss.str();
}

std::unique_ptr<ExtensionInfo>
ExtensionTelemetryService::GetExtensionInfoForReport(
    const extensions::Extension& extension) {
  auto extension_info = std::make_unique<ExtensionInfo>();
  extension_info->set_id(extension.id());
  extension_info->set_name(extension.name());
  extension_info->set_version(extension.version().GetString());
  extension_info->set_install_timestamp_msec(
      extension_prefs_->GetInstallTime(extension.id()).ToJavaTime());
  extension_info->set_is_default_installed(
      extension.was_installed_by_default());
  extension_info->set_is_oem_installed(extension.was_installed_by_oem());
  extension_info->set_is_from_store(extension.from_webstore());
  extension_info->set_updates_from_store(
      extensions::ExtensionManagementFactory::GetForBrowserContext(profile_)
          ->UpdatesFromWebstore(extension));
  extension_info->set_is_converted_from_user_script(
      extension.converted_from_user_script());
  extension_info->set_type(GetType(extension.GetType()));
  extension_info->set_install_location(
      GetInstallLocation(extension.location()));
  extension_info->set_blocklist_state(
      GetBlocklistState(extension.id(), extension_prefs_));
  extension_info->set_disable_reasons(
      extension_prefs_->GetDisableReasons(extension.id()));

  return extension_info;
}

}  // namespace safe_browsing
