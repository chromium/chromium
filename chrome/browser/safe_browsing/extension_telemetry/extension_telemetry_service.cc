// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/tabs_execute_script_signal_processor.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"

namespace safe_browsing {

ExtensionTelemetryService::~ExtensionTelemetryService() = default;

ExtensionTelemetryService::ExtensionTelemetryService(
    Profile* profile,
    extensions::ExtensionRegistry* extension_registry,
    extensions::ExtensionPrefs* extension_prefs)
    : profile_(profile),
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
    // Start timer for sending periodic telemetry reports.
    timer_.Start(FROM_HERE, current_reporting_interval_, this,
                 &ExtensionTelemetryService::CreateAndUploadReports);
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

void ExtensionTelemetryService::CreateAndUploadReports() {
  DCHECK(enabled_);

  std::unique_ptr<ExtensionTelemetryReportRequest> telemetry_report_pb =
      CreateReport();

  if (telemetry_report_pb) {
    // TODO(anunoy): add upload, retry, backoff logic.
  }
}

std::unique_ptr<ExtensionTelemetryReportRequest>
ExtensionTelemetryService::CreateReport() {
  // Don't create a telemetry report if there were no signals generated (i.e.,
  // extension store is empty) AND there are no installed extensions currently.
  std::unique_ptr<extensions::ExtensionSet> installed_extensions =
      extension_registry_->GenerateInstalledExtensionsSet();
  if (extension_store_.empty() && installed_extensions->is_empty())
    return nullptr;

  using google::protobuf::RepeatedPtrField;
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

  DCHECK(reports_pb->size() > 0);

  // Clear out the extension store data to ensure that:
  // - extension info is refreshed for every telemetry report.
  // - no stale extension entry is left over in the extension store.
  extension_store_.clear();

  telemetry_report_pb->set_creation_timestamp_msec(
      base::Time::Now().ToJavaTime());
  return telemetry_report_pb;
}

std::unique_ptr<ExtensionTelemetryReportRequest_ExtensionInfo>
ExtensionTelemetryService::GetExtensionInfoForReport(
    const extensions::Extension& extension) {
  auto extension_info =
      std::make_unique<ExtensionTelemetryReportRequest_ExtensionInfo>();
  extension_info->set_id(extension.id());
  extension_info->set_name(extension.name());
  extension_info->set_version(extension.version().GetString());
  extension_info->set_install_timestamp_msec(
      extension_prefs_->GetInstallTime(extension.id()).ToJavaTime());

  return extension_info;
}

void ExtensionTelemetryService::RecordSignalType(
    ExtensionSignalType signal_type) {
  base::UmaHistogramEnumeration(
      "SafeBrowsing.ExtensionTelemetry.Signals.SignalType", signal_type);
}

}  // namespace safe_browsing
