// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"

#include <sstream>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/i18n/time_formatting.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/extension_telemetry_event_router.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/extension_telemetry/cookies_get_all_signal_processor.h"
#include "chrome/browser/safe_browsing/extension_telemetry/cookies_get_signal_processor.h"
#include "chrome/browser/safe_browsing/extension_telemetry/declarative_net_request_action_signal_processor.h"
#include "chrome/browser/safe_browsing/extension_telemetry/declarative_net_request_signal_processor.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_js_callstacks.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_config_manager.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_file_processor.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_persister.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service_factory.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_uploader.h"
#include "chrome/browser/safe_browsing/extension_telemetry/potential_password_theft_signal_processor.h"
#include "chrome/browser/safe_browsing/extension_telemetry/remote_host_contacted_signal_processor.h"
#include "chrome/browser/safe_browsing/extension_telemetry/tabs_api_signal_processor.h"
#include "chrome/browser/safe_browsing/extension_telemetry/tabs_execute_script_signal_processor.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/path_util.h"
#include "extensions/common/file_util.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/common/switches.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

namespace {

using ::extensions::ExtensionManagement;
using ::extensions::mojom::ManifestLocation;
using ::google::protobuf::RepeatedPtrField;
using ExtensionInfo =
    ::safe_browsing::ExtensionTelemetryReportRequest_ExtensionInfo;
using OffstoreExtensionVerdict =
    ::safe_browsing::ExtensionTelemetryReportResponse_OffstoreExtensionVerdict;

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

// Specifies the default for `num_checks_per_upload_interval_`
// NOTE: With a value of 1, the telemetry service will perform checks at the
// upload interval itself and will only write a report to disk if the upload
// fails.
constexpr int kNumChecksPerUploadInterval = 1;

// Specifies the upload interval for ESB telemetry reports.
base::TimeDelta kUploadIntervalSeconds = base::Seconds(3600);

// Delay before the Telemetry Service checks its last upload time.
base::TimeDelta kStartupUploadCheckDelaySeconds = base::Seconds(15);

// Interval for extension telemetry to start another off-store extension
// file data collection process.
base::TimeDelta kOffstoreFileDataCollectionIntervalSeconds =
    base::Seconds(7200);

// Initial delay for extension telemetry to start collecting
// off-store extension file data.
base::TimeDelta kOffstoreFileDataCollectionStartupDelaySeconds =
    base::Seconds(300);

// Limit the off-store file data collection duration.
base::TimeDelta kOffstoreFileDataCollectionDurationLimitSeconds =
    base::Seconds(60);

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

void RecordSignalTypeForEnterprise(ExtensionSignalType signal_type) {
  base::UmaHistogramEnumeration(
      "SafeBrowsing.ExtensionTelemetry.Enterprise.Signals.SignalType",
      signal_type);
}

void RecordEnterpriseReportSize(size_t size) {
  base::UmaHistogramCounts1M(
      "SafeBrowsing.ExtensionTelemetry.Enterprise.ReportSize", size);
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

// Converts a policy::ManagementAuthorityTrustworthiness to
// ExtensionTelemetryReportRequest::ManagementAuthority.
ExtensionTelemetryReportRequest::ManagementAuthority GetManagementAuthority(
    policy::ManagementAuthorityTrustworthiness
        management_authority_trustworthiness) {
  switch (management_authority_trustworthiness) {
    case policy::ManagementAuthorityTrustworthiness::NONE:
      return ExtensionTelemetryReportRequest::MANAGEMENT_AUTHORITY_NONE;
    case policy::ManagementAuthorityTrustworthiness::LOW:
      return ExtensionTelemetryReportRequest::MANAGEMENT_AUTHORITY_LOW;
    case policy::ManagementAuthorityTrustworthiness::TRUSTED:
      return ExtensionTelemetryReportRequest::MANAGEMENT_AUTHORITY_TRUSTED;
    case policy::ManagementAuthorityTrustworthiness::FULLY_TRUSTED:
      return ExtensionTelemetryReportRequest::
          MANAGEMENT_AUTHORITY_FULLY_TRUSTED;
    default:
      return ExtensionTelemetryReportRequest::MANAGEMENT_AUTHORITY_UNSPECIFIED;
  }
}

ExtensionInfo::InstallationPolicy
ExtensionManagementInstallationModeToExtensionInfoInstallationPolicy(
    const ExtensionManagement::InstallationMode& installation_mode) {
  switch (installation_mode) {
    case ExtensionManagement::InstallationMode::INSTALLATION_ALLOWED:
      return ExtensionInfo::INSTALLATION_ALLOWED;
    case ExtensionManagement::InstallationMode::INSTALLATION_BLOCKED:
      return ExtensionInfo::INSTALLATION_BLOCKED;
    case ExtensionManagement::InstallationMode::INSTALLATION_FORCED:
      return ExtensionInfo::INSTALLATION_FORCED;
    case ExtensionManagement::InstallationMode::INSTALLATION_RECOMMENDED:
      return ExtensionInfo::INSTALLATION_RECOMMENDED;
    case ExtensionManagement::InstallationMode::INSTALLATION_REMOVED:
      return ExtensionInfo::INSTALLATION_REMOVED;
    default:
      return ExtensionInfo::NO_POLICY;
  }
}

ExtensionInfo::BlocklistState BitMapBlocklistStateToExtensionInfoBlocklistState(
    const extensions::BitMapBlocklistState& state) {
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

ExtensionInfo::BlocklistState GetBlocklistState(
    const extensions::ExtensionId extension_id,
    extensions::ExtensionPrefs* extension_prefs) {
  extensions::BitMapBlocklistState state =
      extensions::blocklist_prefs::GetExtensionBlocklistState(extension_id,
                                                              extension_prefs);
  return BitMapBlocklistStateToExtensionInfoBlocklistState(state);
}

ExtensionInfo::BlocklistState GetExtensionTelemetryServiceBlocklistState(
    const extensions::ExtensionId extension_id,
    extensions::ExtensionPrefs* extension_prefs) {
  extensions::BitMapBlocklistState state =
      extensions::blocklist_prefs::GetExtensionTelemetryServiceBlocklistState(
          extension_id, extension_prefs);
  return BitMapBlocklistStateToExtensionInfoBlocklistState(state);
}

extensions::BlocklistState ConvertTelemetryResponseVerdictToBlocklistState(
    OffstoreExtensionVerdict::OffstoreExtensionVerdictType type) {
  switch (type) {
    case OffstoreExtensionVerdict::NONE:
      return extensions::BlocklistState::NOT_BLOCKLISTED;
    case OffstoreExtensionVerdict::MALWARE:
      return extensions::BlocklistState::BLOCKLISTED_MALWARE;
    default:
      return extensions::BlocklistState::BLOCKLISTED_UNKNOWN;
  }
}

// Checks for extensions specified in the -load-extension commandline switch.
// Creates and returns a set of extension objects for such extensions.
// NOTE: These extensions are not installed - the information is only collected
// for telemetry purposes.
// This function is executed on a separate thread from the UI thread since it
// involves reading the manifest files from disk.
extensions::ExtensionSet CollectCommandLineExtensionInfo() {
  extensions::ExtensionSet commandline_extensions;

  // If there are no commandline extensions, return an empty set.
  base::CommandLine* cmdline(base::CommandLine::ForCurrentProcess());
  if (!cmdline || !cmdline->HasSwitch(extensions::switches::kLoadExtension)) {
    return commandline_extensions;
  }

  // Otherwise, create an extension object for each extension specified in the
  // commandline. Note that the extension is not installed.
  base::CommandLine::StringType path_list =
      cmdline->GetSwitchValueNative(extensions::switches::kLoadExtension);
  base::StringTokenizerT<base::CommandLine::StringType,
                         base::CommandLine::StringType::const_iterator>
      t(path_list, FILE_PATH_LITERAL(","));

  while (t.GetNext()) {
    auto tmp_path = extensions::path_util::ResolveHomeDirectory(
        base::FilePath(t.token_piece()));
    auto extension_path = base::MakeAbsoluteFilePath(tmp_path);
    std::string error;
    // Use default creation flags. Since we are not installing the extension,
    // it doesn't really mattter.
    int flags = extensions::Extension::FOLLOW_SYMLINKS_ANYWHERE |
                extensions::Extension::ALLOW_FILE_ACCESS |
                extensions::Extension::REQUIRE_MODERN_MANIFEST_VERSION;
    scoped_refptr<extensions::Extension> extension =
        extensions::file_util::LoadExtension(
            extension_path, ManifestLocation::kCommandLine, flags, &error);
    if (error.empty()) {
      commandline_extensions.Insert(extension);
    }
  }

  return commandline_extensions;
}

// Retrieves the ExtensionTelemetryEventRouter associated with the profile.
enterprise_connectors::ExtensionTelemetryEventRouter*
GetExtensionTelemetryEventRouter(Profile* profile) {
  return enterprise_connectors::ExtensionTelemetryEventRouter::Get(profile);
}

// Returns true if the signal type should be collected for enterprise telemetry.
bool CollectForEnterprise(ExtensionSignalType type) {
  return type == ExtensionSignalType::kCookiesGet ||
         type == ExtensionSignalType::kCookiesGetAll ||
         type == ExtensionSignalType::kRemoteHostContacted ||
         type == ExtensionSignalType::kTabsApi;
}

}  // namespace

// Adds extension installation mode and managed status to extension telemetry
// reports.
BASE_FEATURE(kExtensionTelemetryIncludePolicyData,
             "SafeBrowsingExtensionTelemetryIncludePolicyData",
             base::FEATURE_ENABLED_BY_DEFAULT);

ExtensionTelemetryService::~ExtensionTelemetryService() = default;

// static
ExtensionTelemetryService* ExtensionTelemetryService::Get(Profile* profile) {
  return ExtensionTelemetryServiceFactory::GetInstance()->GetForProfile(
      profile);
}

ExtensionTelemetryService::ExtensionTelemetryService(
    Profile* profile,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : profile_(profile),
      extension_registry_(extensions::ExtensionRegistry::Get(profile)),
      extension_prefs_(extensions::ExtensionPrefs::Get(profile)),
      offstore_file_data_collection_duration_limit_(
          kOffstoreFileDataCollectionDurationLimitSeconds),
      current_reporting_interval_(kUploadIntervalSeconds),
      url_loader_factory_(url_loader_factory),
      num_checks_per_upload_interval_(kNumChecksPerUploadInterval) {
  // Register for SB preference change notifications.
  pref_service_ = profile_->GetPrefs();
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kSafeBrowsingEnhanced,
      base::BindRepeating(&ExtensionTelemetryService::OnESBPrefChanged,
                          base::Unretained(this)));

  // Set initial enable/disable state for ESB.
  SetEnabledForESB(IsEnhancedProtectionEnabled(*pref_service_));

  if (base::FeatureList::IsEnabled(kExtensionTelemetryForEnterprise)) {
    // Register for enterprise policy changes.
    auto* connector_service =
        enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
            profile);
    connector_service->ObserveTelemetryReporting(base::BindRepeating(
        &ExtensionTelemetryService::OnEnterprisePolicyChanged,
        base::Unretained(this)));

    // Set initial enable/disable state for enterprise.
    SetEnabledForEnterprise(
        GetExtensionTelemetryEventRouter(profile_)->IsPolicyEnabled());
  }
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

void ExtensionTelemetryService::OnESBPrefChanged() {
  SetEnabledForESB(IsEnhancedProtectionEnabled(*pref_service_));
}

void ExtensionTelemetryService::OnEnterprisePolicyChanged() {
  if (is_shutdown_) {
    return;
  }

  SetEnabledForEnterprise(
      GetExtensionTelemetryEventRouter(profile_)->IsPolicyEnabled());
}

// Telemetry features for ESB include:
// - ESB signals
// - Off-store data collection
// - Command line extensions file data
// - Telemetry Configuration
// - Persister
void ExtensionTelemetryService::SetEnabledForESB(bool enable) {
  // Make call idempotent.
  if (esb_enabled_ == enable) {
    return;
  }

  esb_enabled_ = enable;
  if (esb_enabled_) {
    SetUpSignalProcessorsAndSubscribersForESB();
    SetUpOffstoreFileDataCollection();

    // File data for Command Line extensions.
    if (base::FeatureList::IsEnabled(
            kExtensionTelemetryFileDataForCommandLineExtensions)) {
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
          base::BindOnce(CollectCommandLineExtensionInfo),
          base::BindOnce(
              &ExtensionTelemetryService::OnCommandLineExtensionsInfoCollected,
              weak_factory_.GetWeakPtr()));
    }

    // Telemetry Configuration
    if (base::FeatureList::IsEnabled(kExtensionTelemetryConfiguration)) {
      config_manager_ =
          std::make_unique<ExtensionTelemetryConfigManager>(pref_service_);
      config_manager_->LoadConfig();
    }

    if (current_reporting_interval_.is_positive()) {
      int max_files_supported =
          ExtensionTelemetryPersister::MaxFilesSupported();
      int checks_per_interval =
          std::min(max_files_supported, num_checks_per_upload_interval_);
      // Configure persister for the maximum number of reports to be persisted
      // on disk. This number is the sum of reports written at every
      // write interval plus an additional allocation (3) for files written at
      // browser/profile shutdown.
      int max_reports_to_persist =
          std::min(checks_per_interval + 3, max_files_supported);
      // Instantiate persister which is used to read/write telemetry reports to
      // disk and start timer for sending periodic telemetry reports.
      persister_ = base::SequenceBound<ExtensionTelemetryPersister>(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
               base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
          max_reports_to_persist, profile_->GetPath());
      persister_.AsyncCall(&ExtensionTelemetryPersister::PersisterInit);
      timer_.Start(FROM_HERE, current_reporting_interval_ / checks_per_interval,
                   this, &ExtensionTelemetryService::PersistOrUploadData);
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
    if (!persister_.is_null()) {
      persister_.AsyncCall(&ExtensionTelemetryPersister::ClearPersistedFiles);
    }
    StopOffstoreFileDataCollection();
  }
}

// Telemetry features for enterprise include:
// - Enterprise signals
// - Off-store data collection
void ExtensionTelemetryService::SetEnabledForEnterprise(bool enable) {
  // Make call idempotent.
  if (enterprise_enabled_ == enable) {
    return;
  }

  enterprise_enabled_ = enable;
  if (enterprise_enabled_) {
    SetUpSignalProcessorsAndSubscribersForEnterprise();
    SetUpOffstoreFileDataCollection();

    enterprise_timer_.Start(
        FROM_HERE,
        base::Seconds(
            kExtensionTelemetryEnterpriseReportingIntervalSeconds.Get()),
        this, &ExtensionTelemetryService::CreateAndSendEnterpriseReport);
  } else {
    // Stop enterprise timer for periodic telemetry reports.
    enterprise_timer_.Stop();
    // Clear all enterprise data stored by the service.
    enterprise_extension_store_.clear();
    // Destruct signal subscribers.
    enterprise_signal_subscribers_.clear();
    // Destruct signal processors.
    enterprise_signal_processors_.clear();
    StopOffstoreFileDataCollection();
  }
}

bool ExtensionTelemetryService::enabled() const {
  return esb_enabled_ || enterprise_enabled_;
}

void ExtensionTelemetryService::Shutdown() {
  is_shutdown_ = true;
  if (esb_enabled_ && SignalDataPresent() && !persister_.is_null()) {
    // Saving data to disk.
    active_report_ = CreateReport();
    std::string write_string;
    active_report_->SerializeToString(&write_string);
    persister_.AsyncCall(&ExtensionTelemetryPersister::WriteReport)
        .WithArgs(std::move(write_string),
                  ExtensionTelemetryPersister::WriteReportTrigger::kAtShutdown);

    RecordWhenFileWasPersisted(/*persisted_at_write_interval=*/false);
  }
  timer_.Stop();
  enterprise_timer_.Stop();
  pref_change_registrar_.RemoveAll();
  StopOffstoreFileDataCollection();
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

  if (esb_enabled_) {
    RecordSignalType(signal_type);
    AddSignalHelper(*signal, extension_store_, signal_subscribers_);
  }

  if (enterprise_enabled_ && CollectForEnterprise(signal_type)) {
    RecordSignalTypeForEnterprise(signal_type);
    AddSignalHelper(*signal, enterprise_extension_store_,
                    enterprise_signal_subscribers_);
  }
}

void ExtensionTelemetryService::AddSignalHelper(
    const ExtensionSignal& signal,
    ExtensionStore& store,
    SignalSubscribers& subscribers) {
  ExtensionSignalType signal_type = signal.GetType();
  DCHECK(base::Contains(subscribers, signal_type));

  if (!store.contains(signal.extension_id())) {
    // This is the first signal triggered by this extension since the last
    // time a report was generated for it. Store its information.
    // Note: The extension information is cached at signal addition time
    // instead of being calculated at report generation. This approach handles
    // the case where the extension is uninstalled after generating the signal
    // but before a report is generated. The extension information is also
    // cleared after each telemetry report is sent to keep the data fresh.
    const extensions::Extension* extension =
        extension_registry_->GetInstalledExtension(signal.extension_id());
    // Do a sanity check on the returned extension object and abort if it is
    // invalid.
    if (!extension) {
      RecordSignalDiscarded(signal_type);
      return;
    }

    store.emplace(signal.extension_id(), GetExtensionInfoForReport(*extension));
  }

  for (safe_browsing::ExtensionSignalProcessor* processor :
       subscribers[signal_type]) {
    processor->ProcessSignal(signal);
  }
}

std::unique_ptr<ExtensionTelemetryReportRequest>
ExtensionTelemetryService::CreateReportWithCommonFieldsPopulated() {
  auto telemetry_report_pb =
      std::make_unique<ExtensionTelemetryReportRequest>();
  telemetry_report_pb->set_developer_mode_enabled(
      profile_->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode));
  telemetry_report_pb->set_creation_timestamp_msec(
      base::Time::Now().InMillisecondsSinceUnixEpoch());

  // Only collect if `is_shutdown_` is false, since BrowserContextHelper can be
  // destroyed already and cause a crash on ChromeOS.
  // TODO(crbug.com/367327319): Investigate keyed service dependency order to
  // guarantee BrowserContextHelper lifetime during shutdown.
  if (base::FeatureList::IsEnabled(kExtensionTelemetryIncludePolicyData) &&
      !is_shutdown_) {
    // The highest level of ManagementAuthorityTrustworthiness of either
    // platform or browser are taken into account.
    policy::ManagementAuthorityTrustworthiness platform_trustworthiness =
        policy::ManagementServiceFactory::GetForPlatform()
            ->GetManagementAuthorityTrustworthiness();
    policy::ManagementAuthorityTrustworthiness browser_trustworthiness =
        policy::ManagementServiceFactory::GetForProfile(profile_)
            ->GetManagementAuthorityTrustworthiness();
    policy::ManagementAuthorityTrustworthiness highest_trustworthiness =
        std::max(platform_trustworthiness, browser_trustworthiness);
    telemetry_report_pb->set_management_authority(
        GetManagementAuthority(highest_trustworthiness));
  }

  return telemetry_report_pb;
}

void ExtensionTelemetryService::CreateAndUploadReport() {
  if (!esb_enabled_) {
    return;
  }

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

void ExtensionTelemetryService::CreateAndSendEnterpriseReport() {
  if (!enterprise_enabled_) {
    return;
  }

  std::unique_ptr<ExtensionTelemetryReportRequest> enterprise_report =
      CreateReportForEnterprise();
  if (enterprise_report) {
    RecordEnterpriseReportSize(enterprise_report->ByteSizeLong());
    GetExtensionTelemetryEventRouter(profile_)->UploadTelemetryReport(
        std::move(enterprise_report));
  } else {
    DLOG(WARNING) << "Upload skipped due to empty enterprise report.";
  }
}

void ExtensionTelemetryService::OnUploadComplete(
    bool success,
    const std::string& response_data) {
  // TODO(crbug.com/40253384): Add `config_manager_` implementation
  // to check server response and update config.
  if (success) {
    SetLastUploadTimeForExtensionTelemetry(*pref_service_, base::Time::Now());

    ExtensionTelemetryReportResponse response;
    if (base::FeatureList::IsEnabled(
            kExtensionTelemetryDisableOffstoreExtensions) &&
        response.ParseFromString(response_data)) {
      ProcessOffstoreExtensionVerdicts(response);
    }
  }
  if (esb_enabled_ && !persister_.is_null()) {
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
          .WithArgs(std::move(write_string),
                    ExtensionTelemetryPersister::WriteReportTrigger::
                        kAtWriteInterval);
      RecordWhenFileWasPersisted(/*persisted_at_write_interval=*/true);
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
  if (!esb_enabled_) {
    return;
  }

  if ((GetLastUploadTimeForExtensionTelemetry(*pref_service_) +
       current_reporting_interval_) <= base::Time::Now()) {
    CreateAndUploadReport();
  }
}

void ExtensionTelemetryService::PersistOrUploadData() {
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
        .WithArgs(
            std::move(write_string),
            ExtensionTelemetryPersister::WriteReportTrigger::kAtWriteInterval);
    RecordWhenFileWasPersisted(/*persisted_at_write_interval=*/true);
  }
}

std::unique_ptr<ExtensionTelemetryReportRequest>
ExtensionTelemetryService::CreateReport() {
  if (!esb_enabled_) {
    return nullptr;
  }

  // Don't create a telemetry report if there were no signals generated (i.e.,
  // extension store is empty) AND there are no installed or command-line
  // extensions present.
  extensions::ExtensionSet extensions_to_report =
      extension_registry_->GenerateInstalledExtensionsSet();
  extensions_to_report.InsertAll(commandline_extensions_);
  if (extension_store_.empty() && extensions_to_report.empty()) {
    return nullptr;
  }

  std::unique_ptr<ExtensionTelemetryReportRequest> telemetry_report_pb =
      CreateReportWithCommonFieldsPopulated();
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

  // Create per-extension reports for the remaining extensions, i.e., exclude
  // extension store extensions since reports have already been created for
  // them. Note that these remaining extension reports will only contain
  // extension information (and no signal data).
  for (const auto& entry : extension_store_) {
    extensions_to_report.Remove(/*extension_id=*/entry.first);
  }

  for (const scoped_refptr<const extensions::Extension>& installed_entry :
       extensions_to_report) {
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

  return telemetry_report_pb;
}

std::unique_ptr<ExtensionTelemetryReportRequest>
ExtensionTelemetryService::CreateReportForEnterprise() {
  // Don't create a telemetry report if there were no signals generated (i.e.,
  // enterprise extension store is empty).
  if (!enterprise_enabled_ || enterprise_extension_store_.empty()) {
    return nullptr;
  }

  std::unique_ptr<ExtensionTelemetryReportRequest> telemetry_report_pb =
      CreateReportWithCommonFieldsPopulated();
  RepeatedPtrField<ExtensionTelemetryReportRequest_Report>* reports_pb =
      telemetry_report_pb->mutable_reports();

  // Create per-extension reports for all the extensions in the enterprise
  // extension store. These represent extensions that have signal information to
  // report.
  for (auto& extension_store_it : enterprise_extension_store_) {
    auto report_entry_pb =
        std::make_unique<ExtensionTelemetryReportRequest_Report>();

    // Populate all signal info for the extension by querying the signal
    // processors.
    RepeatedPtrField<ExtensionTelemetryReportRequest_SignalInfo>* signals_pb =
        report_entry_pb->mutable_signals();
    for (auto& processor_it : enterprise_signal_processors_) {
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

  DCHECK(!reports_pb->empty());

  // Clear out the enterprise extension store data.
  enterprise_extension_store_.clear();

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

void ExtensionTelemetryService::DumpReportForTesting(
    const ExtensionTelemetryReportRequest& report) {
  base::Time creation_time = base::Time::FromMillisecondsSinceUnixEpoch(
      report.creation_timestamp_msec());
  std::stringstream ss;
  ss << "Report creation time: "
     << base::UTF16ToUTF8(TimeFormatShortDateAndTimeWithTimeZone(creation_time))
     << "\n";
  ss << "Developer mode enabled: " << report.developer_mode_enabled() << "\n";
  ss << "Management authority: " << report.management_authority() << "\n";

  const RepeatedPtrField<ExtensionTelemetryReportRequest_Report>& reports =
      report.reports();

  for (const auto& report_pb : reports) {
    const auto& extension_pb = report_pb.extension();
    base::Time install_time = base::Time::FromMillisecondsSinceUnixEpoch(
        extension_pb.install_timestamp_msec());
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
       << "\n"
       << "  InstallationPolicy: " << extension_pb.installation_policy()
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
      // Tabs Api
      if (signal_pb.has_tabs_api_info()) {
        const auto& tabs_api_info_pb = signal_pb.tabs_api_info();
        const RepeatedPtrField<
            ExtensionTelemetryReportRequest_SignalInfo_TabsApiInfo_CallDetails>&
            call_details = tabs_api_info_pb.call_details();
        if (!call_details.empty()) {
          ss << "  Signal: TabsApi\n";
          for (const auto& entry : call_details) {
            ss << "    Call Details:\n"
               << "      API method: "
               << base::NumberToString(static_cast<int>(entry.method())) << "\n"
               << "      current URL: " << entry.current_url() << "\n"
               << "      new URL: " << entry.new_url() << "\n"
               << "      count: " << entry.count() << "\n";
            const auto& js_callstacks = entry.js_callstacks();
            int stack_idx = 0;
            for (const auto& stack : js_callstacks) {
              ss << "      JS callstack " << stack_idx++ << " :";
              ss << ExtensionJSCallStacks::SignalInfoJSCallStackAsString(stack);
            }
          }
        }
        continue;
      }

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
            ss << "    Script hash: " << base::HexEncode(script_pb.hash())
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
               << "      ConnectionProtocol: "
               << remote_host_info_pb.connection_protocol() << "\n"
               << "      ContactedBy: " << remote_host_info_pb.contacted_by()
               << "\n"
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
               << "      Secure: " << (get_all_args_pb.secure() ? "Y" : "N")
               << "\n"
               << "      StoreId: " << get_all_args_pb.store_id() << "\n"
               << "      URL: " << get_all_args_pb.url() << "\n"
               << "      IsSession: "
               << (get_all_args_pb.is_session() ? "Y" : "N") << "\n"
               << "      count: " << get_all_args_pb.count() << "\n";
            const auto& js_callstacks = get_all_args_pb.js_callstacks();
            int stack_idx = 0;
            for (const auto& stack : js_callstacks) {
              ss << "      JS callstack " << stack_idx++ << " :";
              ss << ExtensionJSCallStacks::SignalInfoJSCallStackAsString(stack);
            }
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
            const auto& js_callstacks = get_args_pb.js_callstacks();
            int stack_idx = 0;
            for (const auto& stack : js_callstacks) {
              ss << "      JS callstack " << stack_idx++ << " :";
              ss << ExtensionJSCallStacks::SignalInfoJSCallStackAsString(stack);
            }
          }
        }
        continue;
      }

      // Potential Password Theft
      if (signal_pb.has_potential_password_theft_info()) {
        const auto& potential_password_theft_info_pb =
            signal_pb.potential_password_theft_info();
        const RepeatedPtrField<
            ExtensionTelemetryReportRequest_SignalInfo_PotentialPasswordTheftInfo_PasswordReuseInfo>&
            reused_password_infos =
                potential_password_theft_info_pb.reused_password_infos();
        const RepeatedPtrField<
            ExtensionTelemetryReportRequest_SignalInfo_PotentialPasswordTheftInfo_RemoteHostData>&
            remote_hosts_data =
                potential_password_theft_info_pb.remote_hosts_data();
        if (!reused_password_infos.empty() && !remote_hosts_data.empty()) {
          ss << "  Signal: PotentialPasswordTheft\n";
          for (const auto& remote_hosts_data_pb : remote_hosts_data) {
            ss << "    RemoteHostData:\n"
               << "      URL: " << remote_hosts_data_pb.remote_host_url()
               << "\n"
               << "      count: " << remote_hosts_data_pb.count() << "\n";
          }
          for (const auto& reused_password_infos_pb : reused_password_infos) {
            ss << "    PasswordReuseInfo:\n";
            ss << "      DomainsMatchingPassword:\n";
            for (const std::string& matching_domain :
                 reused_password_infos_pb.domains_matching_password()) {
              ss << "        " << matching_domain << "\n";
            }
            ss << "      IsChromeSigninPassword: "
               << (reused_password_infos_pb.is_chrome_signin_password() ? "Y"
                                                                        : "N")
               << "\n";
            ss << "      ReusedPasswordAccountType:\n";
            ss << "        IsAccountSyncing: "
               << (reused_password_infos_pb.reused_password_account_type()
                           .is_account_syncing()
                       ? "Y"
                       : "N")
               << "\n";
            ss << "        AccountType: "
               << reused_password_infos_pb.reused_password_account_type()
                      .account_type()
               << "\n";
            ss << "      ReuseCount: " << reused_password_infos_pb.count()
               << "\n";
          }
        }
      }

      // Declarative Net Request Api
      if (signal_pb.has_declarative_net_request_info()) {
        const auto& declarative_net_request_info_pb =
            signal_pb.declarative_net_request_info();
        const RepeatedPtrField<std::string>& rules =
            declarative_net_request_info_pb.rules();
        if (!rules.empty()) {
          ss << "  Signal: DeclarativeNetRequest\n";
          for (const auto& rule : rules) {
            ss << "    Rule:" << rule << "\n";
          }
          ss << "    MaxExceededRulesCount:"
             << declarative_net_request_info_pb.max_exceeded_rules_count()
             << "\n";
        }
        continue;
      }

      // Declarative Net Request Action
      if (signal_pb.has_declarative_net_request_action_info()) {
        const auto& dnr_action_info_pb =
            signal_pb.declarative_net_request_action_info();
        const RepeatedPtrField<
            ExtensionTelemetryReportRequest_SignalInfo_DeclarativeNetRequestActionInfo_ActionDetails>&
            action_details = dnr_action_info_pb.action_details();
        if (!action_details.empty()) {
          ss << "  Signal: DeclarativeNetRequestAction\n";
          for (const auto& entry : action_details) {
            ss << "    Action Details:\n"
               << "      action type: "
               << base::NumberToString(static_cast<int>(entry.type())) << "\n"
               << "      request URL: " << entry.request_url() << "\n"
               << "      redirect URL: " << entry.redirect_url() << "\n"
               << "      count: " << entry.count() << "\n";
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

std::optional<ExtensionTelemetryService::OffstoreExtensionFileData>
ExtensionTelemetryService::RetrieveOffstoreFileDataForReport(
    const extensions::ExtensionId& extension_id) {
  const auto& pref_dict = GetExtensionTelemetryFileData(*pref_service_);
  const base::Value::Dict* extension_dict = pref_dict.FindDict(extension_id);
  if (!extension_dict) {
    return std::nullopt;
  }

  const base::Value::Dict* file_data_dict =
      extension_dict->FindDict(kFileDataDictPref);
  if (!file_data_dict || file_data_dict->empty()) {
    return std::nullopt;
  }

  OffstoreExtensionFileData offstore_extension_file_data;
  base::Value::Dict dict = file_data_dict->Clone();
  std::optional<base::Value> manifest_value = dict.Extract(kManifestFile);
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
  return std::make_optional(offstore_extension_file_data);
}

std::unique_ptr<ExtensionInfo>
ExtensionTelemetryService::GetExtensionInfoForReport(
    const extensions::Extension& extension) {
  ExtensionManagement* extension_management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(profile_);
  auto extension_info = std::make_unique<ExtensionInfo>();
  extension_info->set_id(extension.id());
  extension_info->set_name(extension.name());
  extension_info->set_version(extension.version().GetString());
  if (extension.location() == ManifestLocation::kCommandLine) {
    // Set the install timestamp to 0 explicitly for a command-line extension
    // to indicate that it is not actually installed. This is required
    // because the extension may still have associated extension prefs
    // from the last time it was installed (eg. when ESB was disabled).
    extension_info->set_install_timestamp_msec(0);
  } else {
    extension_info->set_install_timestamp_msec(
        extension_prefs_->GetLastUpdateTime(extension.id())
            .InMillisecondsSinceUnixEpoch());
  }
  extension_info->set_is_default_installed(
      extension.was_installed_by_default());
  extension_info->set_is_oem_installed(extension.was_installed_by_oem());
  extension_info->set_is_from_store(extension.from_webstore());
  extension_info->set_updates_from_store(
      extension_management->UpdatesFromWebstore(extension));
  extension_info->set_is_converted_from_user_script(
      extension.converted_from_user_script());
  extension_info->set_type(GetType(extension.GetType()));
  extension_info->set_install_location(
      GetInstallLocation(extension.location()));
  extension_info->set_blocklist_state(
      GetBlocklistState(extension.id(), extension_prefs_));
  extension_info->set_telemetry_blocklist_state(
      GetExtensionTelemetryServiceBlocklistState(extension.id(),
                                                 extension_prefs_));
  extension_info->set_disable_reasons(
      extension_prefs_->GetDisableReasons(extension.id()));
  if (base::FeatureList::IsEnabled(kExtensionTelemetryIncludePolicyData)) {
    bool installation_managed =
        extension_management->IsInstallationExplicitlyAllowed(extension.id()) ||
        extension_management->IsInstallationExplicitlyBlocked(extension.id());
    ExtensionInfo::InstallationPolicy installation_policy =
        installation_managed
            ? ExtensionManagementInstallationModeToExtensionInfoInstallationPolicy(
                  extension_management->GetInstallationMode(&extension))
            : ExtensionInfo::NO_POLICY;
    extension_info->set_installation_policy(installation_policy);
  }

  std::optional<OffstoreExtensionFileData> offstore_file_data =
      RetrieveOffstoreFileDataForReport(extension.id());

  if (offstore_file_data.has_value()) {
    extension_info->set_manifest_json(
        std::move(offstore_file_data.value().manifest));
    for (auto& file_info : offstore_file_data.value().file_infos) {
      extension_info->mutable_file_infos()->Add(std::move(file_info));
    }
  }

  return extension_info;
}

void ExtensionTelemetryService::SetUpSignalProcessorsAndSubscribersForESB() {
  // Create signal processors.
  // Map the processors to the signals they output reports for.
  signal_processors_.emplace(ExtensionSignalType::kCookiesGet,
                             std::make_unique<CookiesGetSignalProcessor>());
  signal_processors_.emplace(ExtensionSignalType::kCookiesGetAll,
                             std::make_unique<CookiesGetAllSignalProcessor>());
  signal_processors_.emplace(
      ExtensionSignalType::kDeclarativeNetRequestAction,
      std::make_unique<DeclarativeNetRequestActionSignalProcessor>());
  signal_processors_.emplace(
      ExtensionSignalType::kDeclarativeNetRequest,
      std::make_unique<DeclarativeNetRequestSignalProcessor>());
  signal_processors_.emplace(
      ExtensionSignalType::kPotentialPasswordTheft,
      std::make_unique<PotentialPasswordTheftSignalProcessor>());
  signal_processors_.emplace(
      ExtensionSignalType::kRemoteHostContacted,
      std::make_unique<RemoteHostContactedSignalProcessor>());
  signal_processors_.emplace(ExtensionSignalType::kTabsApi,
                             std::make_unique<TabsApiSignalProcessor>());
  signal_processors_.emplace(
      ExtensionSignalType::kTabsExecuteScript,
      std::make_unique<TabsExecuteScriptSignalProcessor>());

  // Create subscriber lists for each telemetry signal type.
  // Map the signal processors to the signals that they consume.
  std::vector<raw_ptr<ExtensionSignalProcessor, VectorExperimental>>
      subscribers_for_cookies_get = {
          signal_processors_[ExtensionSignalType::kCookiesGet].get()};
  std::vector<raw_ptr<ExtensionSignalProcessor, VectorExperimental>>
      subscribers_for_cookies_get_all = {
          signal_processors_[ExtensionSignalType::kCookiesGetAll].get()};
  std::vector<raw_ptr<ExtensionSignalProcessor, VectorExperimental>>
      subscribers_for_declarative_net_request_action = {
          signal_processors_[ExtensionSignalType::kDeclarativeNetRequestAction]
              .get()};
  std::vector<raw_ptr<ExtensionSignalProcessor, VectorExperimental>>
      subscribers_for_declarative_net_request = {
          signal_processors_[ExtensionSignalType::kDeclarativeNetRequest]
              .get()};
  std::vector<raw_ptr<ExtensionSignalProcessor, VectorExperimental>>
      subscribers_for_password_reuse = {
          signal_processors_[ExtensionSignalType::kPotentialPasswordTheft]
              .get()};
  std::vector<raw_ptr<ExtensionSignalProcessor, VectorExperimental>>
      subscribers_for_remote_host_contacted = {
          signal_processors_[ExtensionSignalType::kRemoteHostContacted].get(),
          signal_processors_[ExtensionSignalType::kPotentialPasswordTheft]
              .get()};

  std::vector<raw_ptr<ExtensionSignalProcessor, VectorExperimental>>
      subscribers_for_tabs_api = {
          signal_processors_[ExtensionSignalType::kTabsApi].get()};
  std::vector<raw_ptr<ExtensionSignalProcessor, VectorExperimental>>
      subscribers_for_tabs_execute_script = {
          signal_processors_[ExtensionSignalType::kTabsExecuteScript].get()};

  signal_subscribers_.emplace(ExtensionSignalType::kCookiesGet,
                              std::move(subscribers_for_cookies_get));
  signal_subscribers_.emplace(ExtensionSignalType::kCookiesGetAll,
                              std::move(subscribers_for_cookies_get_all));
  signal_subscribers_.emplace(
      ExtensionSignalType::kDeclarativeNetRequestAction,
      std::move(subscribers_for_declarative_net_request_action));
  signal_subscribers_.emplace(
      ExtensionSignalType::kDeclarativeNetRequest,
      std::move(subscribers_for_declarative_net_request));
  signal_subscribers_.emplace(ExtensionSignalType::kPasswordReuse,
                              std::move(subscribers_for_password_reuse));
  signal_subscribers_.emplace(ExtensionSignalType::kRemoteHostContacted,
                              std::move(subscribers_for_remote_host_contacted));
  signal_subscribers_.emplace(ExtensionSignalType::kTabsApi,
                              std::move(subscribers_for_tabs_api));
  signal_subscribers_.emplace(ExtensionSignalType::kTabsExecuteScript,
                              std::move(subscribers_for_tabs_execute_script));
}

void ExtensionTelemetryService::
    SetUpSignalProcessorsAndSubscribersForEnterprise() {
  // Create signal processors.
  // Map the processors to the signals they output reports for.
  enterprise_signal_processors_.emplace(
      ExtensionSignalType::kCookiesGet,
      std::make_unique<CookiesGetSignalProcessor>());
  enterprise_signal_processors_.emplace(
      ExtensionSignalType::kCookiesGetAll,
      std::make_unique<CookiesGetAllSignalProcessor>());
  enterprise_signal_processors_.emplace(
      ExtensionSignalType::kRemoteHostContacted,
      std::make_unique<RemoteHostContactedSignalProcessor>());
  enterprise_signal_processors_.emplace(
      ExtensionSignalType::kTabsApi,
      std::make_unique<TabsApiSignalProcessor>());

  // Create subscriber lists for each telemetry signal type.
  // Map the signal processors to the signals that they consume.
  std::vector<raw_ptr<ExtensionSignalProcessor, VectorExperimental>>
      enterprise_subscribers_for_cookies_get = {
          enterprise_signal_processors_[ExtensionSignalType::kCookiesGet]
              .get()};
  std::vector<raw_ptr<ExtensionSignalProcessor, VectorExperimental>>
      enterprise_subscribers_for_cookies_get_all = {
          enterprise_signal_processors_[ExtensionSignalType::kCookiesGetAll]
              .get()};
  std::vector<raw_ptr<ExtensionSignalProcessor, VectorExperimental>>
      enterprise_subscribers_for_remote_host_contacted = {
          enterprise_signal_processors_
              [ExtensionSignalType::kRemoteHostContacted]
                  .get()};
  std::vector<raw_ptr<ExtensionSignalProcessor, VectorExperimental>>
      enterprise_subscribers_for_tabs_api = {
          enterprise_signal_processors_[ExtensionSignalType::kTabsApi].get()};

  enterprise_signal_subscribers_.emplace(
      ExtensionSignalType::kCookiesGet,
      std::move(enterprise_subscribers_for_cookies_get));
  enterprise_signal_subscribers_.emplace(
      ExtensionSignalType::kCookiesGetAll,
      std::move(enterprise_subscribers_for_cookies_get_all));
  enterprise_signal_subscribers_.emplace(
      ExtensionSignalType::kRemoteHostContacted,
      std::move(enterprise_subscribers_for_remote_host_contacted));
  enterprise_signal_subscribers_.emplace(
      ExtensionSignalType::kTabsApi,
      std::move(enterprise_subscribers_for_tabs_api));
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

void ExtensionTelemetryService::SetUpOffstoreFileDataCollection() {
  // If both is enabled, set up has already been done.
  if (esb_enabled_ && enterprise_enabled_) {
    return;
  }

  // File data collection.
  file_processor_ = base::SequenceBound<ExtensionTelemetryFileProcessor>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));
  offstore_file_data_collection_timer_.Start(
      FROM_HERE, kOffstoreFileDataCollectionStartupDelaySeconds, this,
      &ExtensionTelemetryService::StartOffstoreFileDataCollection);
}

void ExtensionTelemetryService::StartOffstoreFileDataCollection() {
  if (!enabled()) {
    return;
  }

  offstore_file_data_collection_start_time_ = base::TimeTicks::Now();
  offstore_extension_dirs_.clear();
  offstore_extension_file_data_contexts_.clear();
  GetOffstoreExtensionDirs();
  RemoveStaleExtensionsFileDataFromPref();

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

    std::optional<base::Time> timestamp = base::ValueToTime(timestamp_value);
    if (!timestamp.has_value()) {
      offstore_extension_file_data_contexts_.emplace(extension_id, root_dir);
    } else if (base::Time::Now() - timestamp.value() > base::Days(1)) {
      offstore_extension_file_data_contexts_.emplace(extension_id, root_dir,
                                                     timestamp.value());
    }
  }

  CollectOffstoreFileData();
}

void ExtensionTelemetryService::OnCommandLineExtensionsInfoCollected(
    extensions::ExtensionSet commandline_extensions) {
  if (esb_enabled_) {
    // Only store this information if the telemetry service is enabled.
    commandline_extensions_ = std::move(commandline_extensions);
  }
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

  // Also add any extensions that were part of the --load-extension commandline
  // switch if applicable. The information about these extensions is collected
  // (only one time) off-thread using `CollectCommandLineExtensionsInfo` when
  // the extension telemetry service is enabled.
  for (const auto& extension : commandline_extensions_) {
    offstore_extension_dirs_[extension->id()] = extension->path();
  }

  RecordNumOffstoreExtensions(offstore_extension_dirs_.size());
}

void ExtensionTelemetryService::RemoveStaleExtensionsFileDataFromPref() {
  ScopedDictPrefUpdate pref_update(pref_service_,
                                   prefs::kExtensionTelemetryFileData);
  base::Value::Dict& pref_dict = pref_update.Get();

  std::vector<extensions::ExtensionId> stale_extensions;
  for (auto&& offstore : pref_dict) {
    if (offstore_extension_dirs_.find(offstore.first) ==
        offstore_extension_dirs_.end()) {
      stale_extensions.emplace_back(offstore.first);
    }
  }

  for (const auto& extension : stale_extensions) {
    pref_dict.Remove(extension);
  }
}

void ExtensionTelemetryService::CollectOffstoreFileData() {
  if (!enabled()) {
    return;
  }

  // If data for all offstore extensions has been collected or
  // |offstore_file_data_collection_duration_limit_| has been
  // exceeded, start the timer again to schedule the next pass of data
  // collection.
  if (offstore_extension_file_data_contexts_.empty() ||
      (base::TimeTicks::Now() - offstore_file_data_collection_start_time_) >=
          offstore_file_data_collection_duration_limit_) {
    offstore_file_data_collection_timer_.Start(
        FROM_HERE, kOffstoreFileDataCollectionIntervalSeconds, this,
        &ExtensionTelemetryService::StartOffstoreFileDataCollection);

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
  if (file_processor_.is_null() || timer_.IsRunning() ||
      enterprise_timer_.IsRunning()) {
    return;
  }

  offstore_file_data_collection_timer_.Stop();
  offstore_extension_dirs_.clear();
  offstore_extension_file_data_contexts_.clear();
  commandline_extensions_.Clear();
  collected_commandline_extension_info_ = false;
}

void ExtensionTelemetryService::ProcessOffstoreExtensionVerdicts(
    const ExtensionTelemetryReportResponse& response) {
  if (response.offstore_extension_verdicts_size() == 0) {
    return;
  }

  std::map<std::string, extensions::BlocklistState> blocklist_states;
  for (const auto& verdict : response.offstore_extension_verdicts()) {
    const auto& extension_id = verdict.extension_id();
    const extensions::Extension* extension =
        extension_registry_->GetInstalledExtension(extension_id);

    // Ignore the verdict if the extension is not an off-store extension. This
    // should not happen and is just a sanity check.
    if (!extension || extension->from_webstore() ||
        extensions::Manifest::IsComponentLocation(extension->location())) {
      continue;
    }

    blocklist_states[extension_id] =
        ConvertTelemetryResponseVerdictToBlocklistState(verdict.verdict_type());
  }

  auto* extension_system = extensions::ExtensionSystem::Get(profile_);
  if (!extension_system) {
    return;
  }
  extensions::ExtensionService* extension_service =
      extension_system->extension_service();
  if (!extension_service) {
    return;
  }
  extension_service->PerformActionBasedOnExtensionTelemetryServiceVerdicts(
      blocklist_states);
}

base::TimeDelta
ExtensionTelemetryService::GetOffstoreFileDataCollectionStartupDelaySeconds() {
  return kOffstoreFileDataCollectionStartupDelaySeconds;
}

base::TimeDelta
ExtensionTelemetryService::GetOffstoreFileDataCollectionIntervalSeconds() {
  return kOffstoreFileDataCollectionIntervalSeconds;
}

}  // namespace safe_browsing
