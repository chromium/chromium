// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"

#include <sstream>
#include <vector>

#include "base/containers/contains.h"
#include "base/i18n/time_formatting.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/extension_telemetry/cookies_get_all_signal_processor.h"
#include "chrome/browser/safe_browsing/extension_telemetry/cookies_get_signal_processor.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_config_manager.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_file_processor.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_persister.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_uploader.h"
#include "chrome/browser/safe_browsing/extension_telemetry/potential_password_theft_signal_processor.h"
#include "chrome/browser/safe_browsing/extension_telemetry/remote_host_contacted_signal_processor.h"
#include "chrome/browser/safe_browsing/extension_telemetry/tabs_execute_script_signal_processor.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

namespace {

using ::extensions::mojom::ManifestLocation;
using ::google::protobuf::RepeatedPtrField;
using ExtensionInfo =
    ::safe_browsing::ExtensionTelemetryReportRequest_ExtensionInfo;

// The ExtensionTelemetryService saves offstore extensions file data such as
// filenames and hashes in Prefs. This information is stored in the following
// dictionary format:
// {
//  ...
//  kExtensionTelemetryFileData : {
//    <extension_id_1> : {
//      "last_processed_timestamp" : <timestamp>,
//      "file_data" : {
//        <file_path_1> : <file_hash_1>,
//        <file_path_2> : <file_hash_2>,
//        ...
//        <manifest.json> : <file_contents>
//      }
//    },
//    <extension_id_2> : {
//      "last_processed_timestamp" : <timestamp>,
//      "file_data" : {
//        <file_path_1> : <file_hash_1>,
//        <file_path_2> : <file_hash_2>,
//        ...
//        <manifest.json> : <file_contents>
//      }
//    },
//    ...
//  },
//  ...
// }

constexpr char kFileDataProcessTimestampPref[] = "last_processed_timestamp";
constexpr char kFileDataDictPref[] = "file_data";
constexpr char kManifestFile[] = "manifest.json";

// Delay before the Telemetry Service checks its last upload time.
base::TimeDelta kStartupUploadCheckDelaySeconds = base::Seconds(15);

void RecordWhenFileWasPersisted(bool persisted_at_write_interval) {
  base::UmaHistogramBoolean(
      "SafeBrowsing.ExtensionTelemetry.FilePersistedAtWriteInterval",
      persisted_at_write_interval);
}

void RecordNumOffstoreExtensions(int num_extensions) {
  base::UmaHistogramCounts100(
      "SafeBrowsing.ExtensionTelemetry.FileData.NumOffstoreExtensions",
      num_extensions);
}

void RecordCollectionDuration(base::TimeDelta duration) {
  base::UmaHistogramMediumTimes(
      "SafeBrowsing.ExtensionTelemetry.FileData.CollectionDuration", duration);
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

void ExtensionTelemetryService::RecordSignalType(
    ExtensionSignalType signal_type) {
  base::UmaHistogramEnumeration(
      "SafeBrowsing.ExtensionTelemetry.Signals.SignalType", signal_type);
}

void ExtensionTelemetryService::RecordSignalDiscarded(
    ExtensionSignalType signal_type) {
  base::UmaHistogramEnumeration(
      "SafeBrowsing.ExtensionTelemetry.Signals.Discarded", signal_type);
}

void ExtensionTelemetryService::OnPrefChanged() {
  SetEnabled(IsEnhancedProtectionEnabled(*pref_service_));
}

void ExtensionTelemetryService::SetEnabled(bool enable) {
  // Make call idempotent.
  if (enabled_ == enable) {
    return;
  }

  enabled_ = enable;
  if (enabled_) {
    // Create signal processors.
    // Map the processors to the signals they eventually generate.
    signal_processors_.emplace(ExtensionSignalType::kCookiesGet,
                               std::make_unique<CookiesGetSignalProcessor>());
    signal_processors_.emplace(
        ExtensionSignalType::kCookiesGetAll,
        std::make_unique<CookiesGetAllSignalProcessor>());
    signal_processors_.emplace(
        ExtensionSignalType::kTabsExecuteScript,
        std::make_unique<TabsExecuteScriptSignalProcessor>());
    signal_processors_.emplace(
        ExtensionSignalType::kRemoteHostContacted,
        std::make_unique<RemoteHostContactedSignalProcessor>());
    signal_processors_.emplace(
        ExtensionSignalType::kPotentialPasswordTheft,
        std::make_unique<PotentialPasswordTheftSignalProcessor>());

    // Create subscriber lists for each telemetry signal type.
    // Map the signal processors to the signals that they consume.
    std::vector<ExtensionSignalProcessor*> subscribers_for_cookies_get = {
        signal_processors_[ExtensionSignalType::kCookiesGet].get()};
    std::vector<ExtensionSignalProcessor*> subscribers_for_cookies_get_all = {
        signal_processors_[ExtensionSignalType::kCookiesGetAll].get()};
    std::vector<ExtensionSignalProcessor*> subscribers_for_tabs_execute_script =
        {signal_processors_[ExtensionSignalType::kTabsExecuteScript].get()};
    std::vector<ExtensionSignalProcessor*>
        subscribers_for_remote_host_contacted = {
            signal_processors_[ExtensionSignalType::kRemoteHostContacted].get(),
            signal_processors_[ExtensionSignalType::kPotentialPasswordTheft]
                .get()};
    std::vector<ExtensionSignalProcessor*> subscribers_for_password_reuse = {
        signal_processors_[ExtensionSignalType::kPotentialPasswordTheft].get()};

    signal_subscribers_.emplace(ExtensionSignalType::kCookiesGet,
                                std::move(subscribers_for_cookies_get));
    signal_subscribers_.emplace(ExtensionSignalType::kCookiesGetAll,
                                std::move(subscribers_for_cookies_get_all));
    signal_subscribers_.emplace(ExtensionSignalType::kTabsExecuteScript,
                                std::move(subscribers_for_tabs_execute_script));
    signal_subscribers_.emplace(
        ExtensionSignalType::kRemoteHostContacted,
        std::move(subscribers_for_remote_host_contacted));
    signal_subscribers_.emplace(ExtensionSignalType::kPasswordReuse,
                                std::move(subscribers_for_password_reuse));
    if (base::FeatureList::IsEnabled(kExtensionTelemetryConfiguration)) {
      config_manager_ =
          std::make_unique<ExtensionTelemetryConfigManager>(pref_service_);
      config_manager_->LoadConfig();
    }

    if (base::FeatureList::IsEnabled(kExtensionTelemetryFileData)) {
      file_processor_ = base::SequenceBound<ExtensionTelemetryFileProcessor>(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
               base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));
      offstore_file_data_collection_timer_.Start(
          FROM_HERE,
          base::Seconds(kExtensionTelemetryFileDataStartupDelaySeconds.Get()),
          this, &ExtensionTelemetryService::StartOffstoreFileDataCollection);
    }

    if (current_reporting_interval_.is_positive()) {
      int max_files_supported =
          ExtensionTelemetryPersister::MaxFilesSupported();
      int writes_per_interval = std::min(
          max_files_supported, kExtensionTelemetryWritesPerInterval.Get());
      // Configure persister for the maximum number of reports to be persisted
      // on disk. This number is the sum of reports written at every
      // write interval plus an additional allocation (3) for files written at
      // browser/profile shutdown.
      int max_reports_to_persist =
          std::min(writes_per_interval + 3, max_files_supported);
      // Instantiate persister which is used to read/write telemetry reports to
      // disk and start timer for sending periodic telemetry reports.
      if (base::FeatureList::IsEnabled(kExtensionTelemetryPersistence)) {
        persister_ = base::SequenceBound<ExtensionTelemetryPersister>(
            base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                 base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
            max_reports_to_persist, profile_->GetPath());
        persister_.AsyncCall(&ExtensionTelemetryPersister::PersisterInit);
        timer_.Start(FROM_HERE,
                     current_reporting_interval_ / writes_per_interval, this,
                     &ExtensionTelemetryService::PersistOrUploadData);
      } else {
        // Start timer for sending periodic telemetry reports if the reporting
        // interval is not 0. An interval of 0 effectively turns off creation
        // and uploading of telemetry reports.
        timer_.Start(FROM_HERE, current_reporting_interval_, this,
                     &ExtensionTelemetryService::CreateAndUploadReport);
      }
    }
    // Post this task with a delay to avoid running right at Chrome startup
    // when a lot of other startup tasks are running.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ExtensionTelemetryService::StartUploadCheck,
                       weak_factory_.GetWeakPtr()),
        kStartupUploadCheckDelaySeconds);
  } else {
    // Stop timer for periodic telemetry reports.
    timer_.Stop();
    // Clear all data stored by the service.
    extension_store_.clear();
    // Destruct signal subscribers.
    signal_subscribers_.clear();
    // Destruct signal processors.
    signal_processors_.clear();
    // Delete persisted files.
    if (base::FeatureList::IsEnabled(kExtensionTelemetryPersistence) &&
        !persister_.is_null()) {
      persister_.AsyncCall(&ExtensionTelemetryPersister::ClearPersistedFiles);
    }
    if (!file_processor_.is_null()) {
      StopOffstoreFileDataCollection();
    }
  }
}

void ExtensionTelemetryService::Shutdown() {
  if (enabled_ &&
      base::FeatureList::IsEnabled(kExtensionTelemetryPersistence) &&
      SignalDataPresent() && !persister_.is_null()) {
    // Saving data to disk.
    active_report_ = CreateReport();
    std::string write_string;
    active_report_->SerializeToString(&write_string);
    persister_.AsyncCall(&ExtensionTelemetryPersister::WriteReport)
        .WithArgs(std::move(write_string));

    RecordWhenFileWasPersisted(/*persisted_at_write_interval=*/false);
  }
  if (!file_processor_.is_null()) {
    StopOffstoreFileDataCollection();
  }
  timer_.Stop();
  pref_change_registrar_.RemoveAll();
}

bool ExtensionTelemetryService::SignalDataPresent() const {
  return (extension_store_.size() > 0);
}

bool ExtensionTelemetryService::IsSignalEnabled(
    const extensions::ExtensionId& extension_id,
    ExtensionSignalType signal_type) const {
  return config_manager_->IsSignalEnabled(extension_id, signal_type);
}

void ExtensionTelemetryService::AddSignal(
    std::unique_ptr<ExtensionSignal> signal) {
  ExtensionSignalType signal_type = signal->GetType();
  RecordSignalType(signal_type);

  DCHECK(base::Contains(signal_subscribers_, signal_type));

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
    // Do a sanity check on the returned extension object and abort if it is
    // invalid.
    if (!extension) {
      RecordSignalDiscarded(signal_type);
      return;
    }
    extension_store_.emplace(signal->extension_id(),
                             GetExtensionInfoForReport(*extension));
  }

  for (auto* processor : signal_subscribers_[signal_type]) {
    // Pass the signal as reference instead of relinquishing ownership to the
    // signal processor.
    processor->ProcessSignal(*signal);
  }
}

void ExtensionTelemetryService::CreateAndUploadReport() {
  DCHECK(enabled_);
  active_report_ = CreateReport();
  if (!active_report_) {
    return;
  }

  auto upload_data = std::make_unique<std::string>();
  if (!active_report_->SerializeToString(upload_data.get())) {
    active_report_.reset();
    return;
  }
  UploadReport(std::move(upload_data));
}

void ExtensionTelemetryService::OnUploadComplete(bool success) {
  // TODO(https://crbug.com/1408126): Add `config_manager_` implementation
  // to check server response and update config.
  if (success) {
    SetLastUploadTimeForExtensionTelemetry(*pref_service_, base::Time::Now());
  }
  if (base::FeatureList::IsEnabled(kExtensionTelemetryPersistence) &&
      enabled_ && !persister_.is_null()) {
    // Upload saved report(s) if there are any.
    if (success) {
      // Bind the callback to our current thread.
      auto read_callback =
          base::BindOnce(&ExtensionTelemetryService::UploadPersistedFile,
                         weak_factory_.GetWeakPtr());
      persister_.AsyncCall(&ExtensionTelemetryPersister::ReadReport)
          .Then(std::move(read_callback));
    } else {
      // Save report to disk on a failed upload.
      std::string write_string;
      active_report_->SerializeToString(&write_string);
      persister_.AsyncCall(&ExtensionTelemetryPersister::WriteReport)
          .WithArgs(std::move(write_string));
    }
  }
  active_report_.reset();
  active_uploader_.reset();
}

void ExtensionTelemetryService::UploadPersistedFile(std::string report) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!report.empty()) {
    auto upload_data = std::make_unique<std::string>(report);
    // Ensure that `upload_data` is a properly formatted protobuf. If it
    // isn't, skip the upload and attempt to upload the next persisted file.
    active_report_ = std::make_unique<ExtensionTelemetryReportRequest>();
    if (active_report_->ParseFromString(*upload_data.get())) {
      UploadReport(std::move(upload_data));
    } else {
      active_report_.reset();
      auto read_callback =
          base::BindOnce(&ExtensionTelemetryService::UploadPersistedFile,
                         weak_factory_.GetWeakPtr());
      persister_.AsyncCall(&ExtensionTelemetryPersister::ReadReport)
          .Then(std::move(read_callback));
    }
  }
}

void ExtensionTelemetryService::UploadReport(
    std::unique_ptr<std::string> report) {
  auto callback = base::BindOnce(&ExtensionTelemetryService::OnUploadComplete,
                                 weak_factory_.GetWeakPtr());
  active_uploader_ = std::make_unique<ExtensionTelemetryUploader>(
      std::move(callback), url_loader_factory_, std::move(report),
      GetTokenFetcher());
  active_uploader_->Start();
}

void ExtensionTelemetryService::StartUploadCheck() {
  // This check is performed as a delayed task after enabling the service. The
  // service may become disabled between the time this task is scheduled and it
  // actually runs. So make sure service is enabled before performing the check.
  if (!enabled_) {
    return;
  }

  if ((GetLastUploadTimeForExtensionTelemetry(*pref_service_) +
       current_reporting_interval_) <= base::Time::Now()) {
    CreateAndUploadReport();
  }
}

void ExtensionTelemetryService::PersistOrUploadData() {
  // Check the `kExtensionTelemetryLastUploadTime` preference,
  // if enough time has passed, upload a report.
  DCHECK(base::FeatureList::IsEnabled(kExtensionTelemetryPersistence));
  if (GetLastUploadTimeForExtensionTelemetry(*pref_service_) +
          current_reporting_interval_ <=
      base::Time::Now()) {
    CreateAndUploadReport();
  } else {
    // Otherwise persist data gathered so far.
    active_report_ = CreateReport();
    if (!active_report_) {
      return;
    }
    std::string write_string;
    if (!active_report_->SerializeToString(&write_string)) {
      active_report_.reset();
      return;
    }
    persister_.AsyncCall(&ExtensionTelemetryPersister::WriteReport)
        .WithArgs(std::move(write_string));
    RecordWhenFileWasPersisted(/*persisted_at_write_interval=*/true);
  }
}

std::unique_ptr<ExtensionTelemetryReportRequest>
ExtensionTelemetryService::CreateReport() {
  // Don't create a telemetry report if there were no signals generated (i.e.,
  // extension store is empty) AND there are no installed extensions currently.
  extensions::ExtensionSet installed_extensions =
      extension_registry_->GenerateInstalledExtensionsSet();
  if (extension_store_.empty() && installed_extensions.empty()) {
    return nullptr;
  }

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
    installed_extensions.Remove(entry.first /* extension_id */);
  }

  for (const scoped_refptr<const extensions::Extension>& installed_entry :
       installed_extensions) {
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

std::unique_ptr<SafeBrowsingTokenFetcher>
ExtensionTelemetryService::GetTokenFetcher() {
  DCHECK(!profile_->IsOffTheRecord() &&
         IsEnhancedProtectionEnabled(*profile_->GetPrefs()));
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  if (identity_manager &&
      safe_browsing::SyncUtils::IsPrimaryAccountSignedIn(identity_manager)) {
    return std::make_unique<SafeBrowsingPrimaryAccountTokenFetcher>(
        identity_manager);
  }
  return nullptr;
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

    if (extension_pb.has_manifest_json()) {
      ss << "  ManifestJSON: " << extension_pb.manifest_json() << "\n";
    }

    const RepeatedPtrField<
        ExtensionTelemetryReportRequest_ExtensionInfo_FileInfo>& file_infos =
        extension_pb.file_infos();
    if (!file_infos.empty()) {
      ss << "  FileInfos: \n";
      for (const auto& file_info : file_infos) {
        ss << "    File name: " << file_info.name()
           << " File hash: " << file_info.hash() << "\n";
      }
    }

    const RepeatedPtrField<ExtensionTelemetryReportRequest_SignalInfo>&
        signals = report_pb.signals();
    for (const auto& signal_pb : signals) {
      // Tabs Execute Script
      if (signal_pb.has_tabs_execute_script_info()) {
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
        continue;
      }

      // Remote Host Contacted
      if (signal_pb.has_remote_host_contacted_info()) {
        const auto& remote_host_contacted_info_pb =
            signal_pb.remote_host_contacted_info();
        const RepeatedPtrField<
            ExtensionTelemetryReportRequest_SignalInfo_RemoteHostContactedInfo_RemoteHostInfo>&
            remote_host_infos = remote_host_contacted_info_pb.remote_host();
        if (!remote_host_infos.empty()) {
          ss << "  Signal: RemoteHostContacted\n";
          for (const auto& remote_host_info_pb : remote_host_infos) {
            ss << "    RemoteHostInfo:\n"
               << "      URL: " << remote_host_info_pb.url() << "\n"
               << "      ConnectionProtocal: "
               << remote_host_info_pb.connection_protocol() << "\n"
               << "      count: " << remote_host_info_pb.contact_count()
               << "\n";
          }
        }
        continue;
      }

      // Cookies Get All
      if (signal_pb.has_cookies_get_all_info()) {
        const auto& cookies_get_all_info_pb = signal_pb.cookies_get_all_info();
        const RepeatedPtrField<
            ExtensionTelemetryReportRequest_SignalInfo_CookiesGetAllInfo_GetAllArgsInfo>&
            get_all_args_infos = cookies_get_all_info_pb.get_all_args_info();
        if (!get_all_args_infos.empty()) {
          ss << "  Signal: CookiesGetAll\n";
          for (const auto& get_all_args_pb : get_all_args_infos) {
            ss << "    GetAllArgsInfo:\n"
               << "      Domain: " << get_all_args_pb.domain() << "\n"
               << "      Name: " << get_all_args_pb.name() << "\n"
               << "      Path: " << get_all_args_pb.path() << "\n"
               << "      Secure: " << get_all_args_pb.secure() << "\n"
               << "      StoreId: " << get_all_args_pb.store_id() << "\n"
               << "      URL: " << get_all_args_pb.url() << "\n"
               << "      IsSession: " << get_all_args_pb.is_session() << "\n"
               << "      count: " << get_all_args_pb.count() << "\n";
          }
        }
        continue;
      }

      // Cookies Get
      if (signal_pb.has_cookies_get_info()) {
        const auto& cookies_get_info_pb = signal_pb.cookies_get_info();
        const RepeatedPtrField<
            ExtensionTelemetryReportRequest_SignalInfo_CookiesGetInfo_GetArgsInfo>&
            get_args_infos = cookies_get_info_pb.get_args_info();
        if (!get_args_infos.empty()) {
          ss << "  Signal: CookiesGet\n";
          for (const auto& get_args_pb : get_args_infos) {
            ss << "    GetArgsInfo:\n"
               << "      Name: " << get_args_pb.name() << "\n"
               << "      URL: " << get_args_pb.url() << "\n"
               << "      StoreId: " << get_args_pb.store_id() << "\n"
               << "      count: " << get_args_pb.count() << "\n";
          }
        }
        continue;
      }
    }
  }

  DVLOG(1) << "Telemetry Report: " << ss.str();
}

ExtensionTelemetryService::OffstoreExtensionFileData::
    OffstoreExtensionFileData() = default;
ExtensionTelemetryService::OffstoreExtensionFileData::
    ~OffstoreExtensionFileData() = default;
ExtensionTelemetryService::OffstoreExtensionFileData::
    OffstoreExtensionFileData::OffstoreExtensionFileData(
        const OffstoreExtensionFileData& src) = default;

absl::optional<ExtensionTelemetryService::OffstoreExtensionFileData>
ExtensionTelemetryService::RetrieveOffstoreFileDataForReport(
    const extensions::ExtensionId& extension_id) {
  const auto& pref_dict = GetExtensionTelemetryFileData(*pref_service_);
  const base::Value::Dict* extension_dict = pref_dict.FindDict(extension_id);
  if (!extension_dict) {
    return absl::nullopt;
  }

  const base::Value::Dict* file_data_dict =
      extension_dict->FindDict(kFileDataDictPref);
  if (!file_data_dict || file_data_dict->empty()) {
    return absl::nullopt;
  }

  OffstoreExtensionFileData offstore_extension_file_data;
  base::Value::Dict dict = file_data_dict->Clone();
  absl::optional<base::Value> manifest_value = dict.Extract(kManifestFile);
  if (manifest_value.has_value()) {
    offstore_extension_file_data.manifest =
        std::move(manifest_value.value().GetString());
  }

  for (auto&& [file_name, file_hash] : dict) {
    ExtensionTelemetryReportRequest_ExtensionInfo_FileInfo file_info;
    file_info.set_name(std::move(file_name));
    file_info.set_hash(std::move(file_hash.GetString()));
    offstore_extension_file_data.file_infos.emplace_back(std::move(file_info));
  }
  return absl::make_optional(offstore_extension_file_data);
}

std::unique_ptr<ExtensionInfo>
ExtensionTelemetryService::GetExtensionInfoForReport(
    const extensions::Extension& extension) {
  auto extension_info = std::make_unique<ExtensionInfo>();
  extension_info->set_id(extension.id());
  extension_info->set_name(extension.name());
  extension_info->set_version(extension.version().GetString());
  extension_info->set_install_timestamp_msec(
      extension_prefs_->GetLastUpdateTime(extension.id()).ToJavaTime());
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

  if (base::FeatureList::IsEnabled(kExtensionTelemetryFileData)) {
    absl::optional<OffstoreExtensionFileData> offstore_file_data =
        RetrieveOffstoreFileDataForReport(extension.id());

    if (offstore_file_data.has_value()) {
      extension_info->set_manifest_json(
          std::move(offstore_file_data.value().manifest));
      for (auto& file_info : offstore_file_data.value().file_infos) {
        extension_info->mutable_file_infos()->Add(std::move(file_info));
      }
    }
  }

  return extension_info;
}

ExtensionTelemetryService::OffstoreExtensionFileDataContext::
    OffstoreExtensionFileDataContext(
        const extensions::ExtensionId& extension_id,
        const base::FilePath& root_dir)
    : extension_id(extension_id), root_dir(root_dir) {}

ExtensionTelemetryService::OffstoreExtensionFileDataContext::
    OffstoreExtensionFileDataContext(
        const extensions::ExtensionId& extension_id,
        const base::FilePath& root_dir,
        const base::Time& last_processed_time)
    : extension_id(extension_id),
      root_dir(root_dir),
      last_processed_time(last_processed_time) {}

bool ExtensionTelemetryService::OffstoreExtensionFileDataContext::operator<(
    const OffstoreExtensionFileDataContext& other) const {
  // Use extension_id to break ties.
  return std::tie(last_processed_time, extension_id) <
         std::tie(other.last_processed_time, other.extension_id);
}

void ExtensionTelemetryService::StartOffstoreFileDataCollection() {
  if (!enabled_) {
    return;
  }

  offstore_file_data_collection_start_time_ = base::TimeTicks::Now();
  offstore_extension_dirs_.clear();
  offstore_extension_file_data_contexts_.clear();
  GetOffstoreExtensionDirs();
  RemoveUninstalledExtensionsFileDataFromPref();

  // Gather context to process offstore extensions.
  const auto& pref_dict = GetExtensionTelemetryFileData(*pref_service_);
  for (const auto& [extension_id, root_dir] : offstore_extension_dirs_) {
    const base::Value::Dict* extension_dict = pref_dict.FindDict(extension_id);
    if (!extension_dict) {
      offstore_extension_file_data_contexts_.emplace(extension_id, root_dir);
      continue;
    }

    const base::Value* timestamp_value =
        extension_dict->Find(kFileDataProcessTimestampPref);
    if (!timestamp_value) {
      offstore_extension_file_data_contexts_.emplace(extension_id, root_dir);
      continue;
    }

    absl::optional<base::Time> timestamp = base::ValueToTime(timestamp_value);
    if (!timestamp.has_value()) {
      offstore_extension_file_data_contexts_.emplace(extension_id, root_dir);
    } else if (base::Time::Now() - timestamp.value() > base::Days(1)) {
      offstore_extension_file_data_contexts_.emplace(extension_id, root_dir,
                                                     timestamp.value());
    }
  }

  CollectOffstoreFileData();
}

void ExtensionTelemetryService::GetOffstoreExtensionDirs() {
  const extensions::ExtensionSet installed_extensions =
      extension_registry_->GenerateInstalledExtensionsSet();

  for (const auto& extension : installed_extensions) {
    if (!extension->from_webstore() &&
        !extensions::Manifest::IsComponentLocation(extension->location())) {
      offstore_extension_dirs_[extension->id()] = extension->path();
    }
  }
  RecordNumOffstoreExtensions(offstore_extension_dirs_.size());
}

void ExtensionTelemetryService::RemoveUninstalledExtensionsFileDataFromPref() {
  ScopedDictPrefUpdate pref_update(pref_service_,
                                   prefs::kExtensionTelemetryFileData);
  base::Value::Dict& pref_dict = pref_update.Get();

  std::vector<extensions::ExtensionId> uninstalled_extensions;
  for (auto&& offstore : pref_dict) {
    if (offstore_extension_dirs_.find(offstore.first) ==
        offstore_extension_dirs_.end()) {
      uninstalled_extensions.emplace_back(offstore.first);
    }
  }

  for (const auto& extension : uninstalled_extensions) {
    pref_dict.Remove(extension);
  }
}

void ExtensionTelemetryService::CollectOffstoreFileData() {
  if (!enabled_) {
    return;
  }

  // If data for all offstore extensions has been collected, start the timer
  // again to schedule the next pass of data collection.
  if (offstore_extension_file_data_contexts_.empty()) {
    offstore_file_data_collection_timer_.Start(
        FROM_HERE,
        base::Seconds(
            kExtensionTelemetryFileDataCollectionIntervalSeconds.Get()),
        this, &ExtensionTelemetryService::StartOffstoreFileDataCollection);

    // Record only if there are off-store extensions installed.
    if (!offstore_extension_dirs_.empty()) {
      RecordCollectionDuration(base::TimeTicks::Now() -
                               offstore_file_data_collection_start_time_);
    }
    return;
  }

  auto context = offstore_extension_file_data_contexts_.begin();
  auto process_extension_callback =
      base::BindOnce(&ExtensionTelemetryService::OnOffstoreFileDataCollected,
                     weak_factory_.GetWeakPtr(), context);
  file_processor_.AsyncCall(&ExtensionTelemetryFileProcessor::ProcessExtension)
      .WithArgs(context->root_dir)
      .Then(std::move(process_extension_callback));
}

void ExtensionTelemetryService::OnOffstoreFileDataCollected(
    base::flat_set<OffstoreExtensionFileDataContext>::iterator context,
    base::Value::Dict file_data) {
  // Save to Prefs
  base::Value::Dict extension_dict;
  extension_dict.Set(kFileDataProcessTimestampPref,
                     base::TimeToValue(base::Time::Now()));
  extension_dict.Set(kFileDataDictPref, std::move(file_data));

  ScopedDictPrefUpdate pref_update(pref_service_,
                                   prefs::kExtensionTelemetryFileData);
  pref_update->Set(context->extension_id, std::move(extension_dict));

  // Remove context and repeat.
  offstore_extension_file_data_contexts_.erase(context);
  CollectOffstoreFileData();
}

void ExtensionTelemetryService::StopOffstoreFileDataCollection() {
  offstore_file_data_collection_timer_.Stop();
  offstore_extension_dirs_.clear();
  offstore_extension_file_data_contexts_.clear();
}

}  // namespace safe_browsing
