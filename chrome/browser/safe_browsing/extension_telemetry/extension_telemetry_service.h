// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_SERVICE_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"

class Profile;
class PrefService;

namespace extensions {
class Extension;
class ExtensionPrefs;
class ExtensionRegistry;
}  // namespace extensions

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {

enum class ExtensionSignalType;
class ExtensionSignal;
class ExtensionSignalProcessor;
class ExtensionTelemetryConfigManager;
class ExtensionTelemetryFileProcessor;
class ExtensionTelemetryPersister;
class ExtensionTelemetryReportRequest;
class ExtensionTelemetryReportRequest_ExtensionInfo;
class ExtensionTelemetryReportRequest_ExtensionInfo_FileInfo;
class ExtensionTelemetryReportResponse;
class ExtensionTelemetryUploader;
class SafeBrowsingTokenFetcher;

// This class processes extension signals and reports telemetry for a given
// profile (regular profile only). It is used exclusively on the UI thread.
// Lifetime:
// The service is instantiated when the associated profile is instantiated. It
// is destructed when the corresponding profile is destructed.
// Enable/Disable states:
// The service is enabled if:
//  - the user is opted into Enhanced Safe Browsing (ESB) OR
//  - enterprise telemetry is enabled by a policy
//
// |esb_enabled_| - Generates telemetry reports for Enhanced Safe Browsing
// (ESB) users. When enabled, the service receives/stores signal
// information, and collects file data for off-store extensions.
// Periodically, the telemetry reports are uploaded to the SB servers. In
// the upload response, the CRX telemetry server includes unsafe off-store
// extension verdicts that the service can take action on.
// |enterprise_enabled_| - Generates enterprise telemetry reports for
// managed profiles. When enabled, the service also collects signal
// information and file data for off-store extensions. Periodically, the
// telemetry reports are sent to the Chrome Enterprise Reporting servers
// instead. Unlike the ESB flow, there is no response received when an
// enterprise report is sent.
//
// For both ESB and enterprise: when disabled, any previously stored signal
// information is cleared, incoming signals are ignored and no reports are
// sent.
class ExtensionTelemetryService : public KeyedService {
 public:
  // Convenience method to get the service for a profile.
  static ExtensionTelemetryService* Get(Profile* profile);

  ExtensionTelemetryService(ExtensionTelemetryService&&) = delete;
  ExtensionTelemetryService& operator=(ExtensionTelemetryService&&) = delete;
  ExtensionTelemetryService(
      Profile* profile,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ExtensionTelemetryService(const ExtensionTelemetryService&) = delete;
  ExtensionTelemetryService& operator=(const ExtensionTelemetryService&) =
      delete;

  ~ExtensionTelemetryService() override;

  // Records the signal type when a signal is:
  // - created externally and passed to extension service using AddSignal OR
  // - created internally by a signal processor from other signals received.
  static void RecordSignalType(ExtensionSignalType signal_type);

  // Recorded when a signal is discarded because it contains invalid data (e.g.,
  // invalid extension id).
  static void RecordSignalDiscarded(ExtensionSignalType signal_type);

  // Enables/disables the service for ESB reports.
  void SetEnabledForESB(bool enable);
  // Enables/disables the service for Enterprise reports.
  void SetEnabledForEnterprise(bool enable);
  // Returns true if the telemetry service is enabled for either ESB,
  // enterprise, or both.
  bool enabled() const;

  // Accepts extension telemetry signals for processing.
  void AddSignal(std::unique_ptr<ExtensionSignal> signal);

  // Checks the `extension_id` and `signal_type` against the
  // configuration and reports true if the signal should be created.
  bool IsSignalEnabled(const extensions::ExtensionId& extension_id,
                       ExtensionSignalType signal_type) const;

  base::TimeDelta current_reporting_interval() {
    return current_reporting_interval_;
  }

  // KeyedService:
  void Shutdown() override;

  base::TimeDelta GetOffstoreFileDataCollectionStartupDelaySeconds();
  base::TimeDelta GetOffstoreFileDataCollectionIntervalSeconds();

 private:
  using SignalProcessors =
      base::flat_map<ExtensionSignalType,
                     std::unique_ptr<ExtensionSignalProcessor>>;
  using SignalSubscribers = base::flat_map<
      ExtensionSignalType,
      std::vector<raw_ptr<ExtensionSignalProcessor, VectorExperimental>>>;
  // Maps extension id to extension data.
  using ExtensionStore = base::flat_map<
      extensions::ExtensionId,
      std::unique_ptr<ExtensionTelemetryReportRequest_ExtensionInfo>>;

  // Called when the pref that affects ESB telemetry reporting is changed.
  void OnESBPrefChanged();

  // Called when the policy that affects enterprise telemetry reporting is
  // changed.
  void OnEnterprisePolicyChanged();

  // Helper method to add and process an extension signal. Shared by both ESB
  // and enterprise reporting.
  void AddSignalHelper(const ExtensionSignal& signal,
                       ExtensionStore& store,
                       SignalSubscribers& subscribers);

  // Creates a telemetry report with common fields shared by both ESB and
  // enterprise reporting.
  std::unique_ptr<ExtensionTelemetryReportRequest>
  CreateReportWithCommonFieldsPopulated();

  // Creates and uploads telemetry reports.
  void CreateAndUploadReport();

  // Creates and sends telemetry report to enterprise.
  void CreateAndSendEnterpriseReport();

  void OnUploadComplete(bool success, const std::string& response_data);

  // Returns a bool that represents if there is any signal processor
  // information to report.
  bool SignalDataPresent() const;

  // Creates telemetry report protobuf for all extension store extensions
  // and currently installed extensions along with signal data retrieved from
  // signal processors.
  std::unique_ptr<ExtensionTelemetryReportRequest> CreateReport();
  // For enterprise, a report is only created for extensions with signals data.
  std::unique_ptr<ExtensionTelemetryReportRequest> CreateReportForEnterprise();

  // Dumps a telemetry report in logs for testing.
  void DumpReportForTesting(const ExtensionTelemetryReportRequest& report);

  // Collects extension information for reporting.
  std::unique_ptr<ExtensionTelemetryReportRequest_ExtensionInfo>
  GetExtensionInfoForReport(const extensions::Extension& extension);

  void UploadPersistedFile(std::string report);

  // Creates access token fetcher based on profile log-in status.
  // Returns nullptr when the user is not signed in.
  std::unique_ptr<SafeBrowsingTokenFetcher> GetTokenFetcher();

  // Called periodically based on the Telemetry Service timer. If time
  // elapsed since last upload is less than the reporting interval, persists
  // a new report, else uploads current report and any persisted reports.
  void PersistOrUploadData();

  // Uploads an extension telemetry report.
  void UploadReport(std::unique_ptr<std::string> report);

  // Checks the time of the last upload and if enough time has passed,
  // uploads telemetry data. Runs on a delayed post task on startup.
  void StartUploadCheck();

  // Callback used to receive information about any extensions present in
  // the Chrome command line switch, --load-extension. The information is
  // collected off the UI thread since it involves reading the manifest file of
  // the extension. The callback stores the received information, a set of
  // extension objects, in `commandline_extensions_`.
  // NOTE: The extension objects are created without actually installing the
  // extensions.
  void OnCommandLineExtensionsInfoCollected(
      extensions::ExtensionSet commandline_extensions);

  // Sets up signal processors and subscribers for ESB telemetry.
  void SetUpSignalProcessorsAndSubscribersForESB();

  // Sets up signal processors and subscribers for enterprise telemetry.
  void SetUpSignalProcessorsAndSubscribersForEnterprise();

  // Sets up the off-store file data collection: file processor, timer, and
  // command line extensions. If it is already set up, this is a no-op.
  void SetUpOffstoreFileDataCollection();

  // Searches for offstore extensions, collects file data such as
  // hashes/manifest content, and saves the data to PrefService. Repeats every 2
  // hours. This method is repeated periodically (default 2 hours) to check if
  // the file data needs to be updated. File data is updated once every 24 hours
  // by default.
  void StartOffstoreFileDataCollection();

  // Searches through the extension registry for offstore extensions and
  // populates |offstore_extension_dirs_| with extension id and root directory.
  // An off-store extension is defined as not from the webstore and not
  // installed from components.
  void GetOffstoreExtensionDirs();

  // Remove any stale off-store file data stored in prefs. The data is
  // considered stale if the associated off-store extension is no longer
  // installed or no longer part of the --load-extension command line switch.
  void RemoveStaleExtensionsFileDataFromPref();

  // Collect file data from an offstore extension by making a call to the
  // FileProcessor.
  void CollectOffstoreFileData();

  // Stores information to identify file data collected for each offstore
  // extension.
  struct OffstoreExtensionFileDataContext {
    OffstoreExtensionFileDataContext(
        const extensions::ExtensionId& extension_id,
        const base::FilePath& root_dir);
    OffstoreExtensionFileDataContext(
        const extensions::ExtensionId& extension_id,
        const base::FilePath& root_dir,
        const base::Time& last_processed_time);

    extensions::ExtensionId extension_id;
    base::FilePath root_dir;
    base::Time last_processed_time;

    bool operator<(const OffstoreExtensionFileDataContext& other) const;
  };

  // Callback invoked when file data for an extension is completed. The data is
  // saved to Prefs and the next extension file data collection is initiated.
  void OnOffstoreFileDataCollected(
      base::flat_set<OffstoreExtensionFileDataContext>::iterator context,
      base::Value::Dict file_data);

  // Stops and clears any offstore file data collection objects/contexts.
  void StopOffstoreFileDataCollection();

  // Stores offstore extension file data retrieved from PrefService.
  struct OffstoreExtensionFileData {
    OffstoreExtensionFileData();
    ~OffstoreExtensionFileData();
    OffstoreExtensionFileData(const OffstoreExtensionFileData&);

    std::string manifest;
    std::vector<ExtensionTelemetryReportRequest_ExtensionInfo_FileInfo>
        file_infos;
  };

  // Given an |extension_id|, retrieves the collected file data from PrefService
  // if available.
  std::optional<OffstoreExtensionFileData> RetrieveOffstoreFileDataForReport(
      const extensions::ExtensionId& extension_id);

  // Validates offending off-store extension verdicts received in a telemetry
  // report response, and converts them into a blocklist state map for the
  // ExtensionService to act on.
  void ProcessOffstoreExtensionVerdicts(
      const ExtensionTelemetryReportResponse& response);

  // Common variables shared between ESB and enterprise reporting:
  // The profile with which this instance of the service is associated.
  const raw_ptr<Profile> profile_;

  // Unowned objects used for getting extension information.
  const raw_ptr<extensions::ExtensionRegistry> extension_registry_;
  const raw_ptr<extensions::ExtensionPrefs> extension_prefs_;

  // The |file_processor_| object reads and hashes offstore extension files.
  // Since these are blocking operations, it is bound to a different sequence
  // task runner.
  base::SequenceBound<ExtensionTelemetryFileProcessor> file_processor_;

  // Stores extension objects for extensions that are included in the
  // --load-extension command line switch.
  extensions::ExtensionSet commandline_extensions_;
  // Used to ensure that the information about command line extensions is only
  // collected once.
  bool collected_commandline_extension_info_ = false;

  // Maps offstore extension id to extension root path
  using OffstoreExtensionDirs =
      base::flat_map<extensions::ExtensionId, base::FilePath>;
  OffstoreExtensionDirs offstore_extension_dirs_;
  // Set of offstore extensions to process in order. Sorted by oldest last
  // processing time.
  base::flat_set<OffstoreExtensionFileDataContext>
      offstore_extension_file_data_contexts_;
  // Used to start the initial offstore extension file data collection based on
  // |kOffstoreFileDataCollectionStartupDelaySeconds| - default: 5 mins.
  // Then repeat the collection based on
  // |kOffstoreFileDataCollectionIntervalSeconds| - default: 2 hours.
  base::OneShotTimer offstore_file_data_collection_timer_;
  base::TimeTicks offstore_file_data_collection_start_time_;
  base::TimeDelta offstore_file_data_collection_duration_limit_;

  // ESB-specific reporting variables:
  // Keeps track of the state of the service for ESB telemetry reporting.
  bool esb_enabled_ = false;

  SignalProcessors signal_processors_;
  SignalSubscribers signal_subscribers_;

  // Stores data on extensions with generated signals for ESB reporting.
  ExtensionStore extension_store_;

  // Used for periodic collection of ESB telemetry reports.
  base::RepeatingTimer timer_;
  base::TimeDelta current_reporting_interval_;

  // The persister object is bound to the threadpool. This prevents the
  // the read/write operations the `persister_` runs from blocking
  // the UI thread. It also allows the `persister_` object to be
  // destroyed cleanly while running tasks during Chrome shutdown.
  base::SequenceBound<ExtensionTelemetryPersister> persister_;

  // The `config_manager_` manages all configurable variables of the
  // Extension Telemetry Service. Variables are stored in Chrome Prefs
  // between sessions.
  std::unique_ptr<safe_browsing::ExtensionTelemetryConfigManager>
      config_manager_;

  // The URLLoaderFactory used to issue network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Unowned object used for getting preference settings.
  raw_ptr<PrefService> pref_service_;

  // Observes changes to kSafeBrowsingEnhanced.
  PrefChangeRegistrar pref_change_registrar_;

  // Specifies the number of times(N) the telemetry service checks if a
  // telemetry upload is required within an upload interval(I). The telemetry
  // service checks if an upload is necessary at I/N intervals. At each check
  // interval, the in-memory telemetry data is saved to disk - till the time an
  // upload interval has elapsed. For example, a value of 2 means that the
  // telemetry service checks for uploads at I/2 and I. At the first check
  // interval, the in-memory report is written to disk. At the second check
  // interval, the in-memory report and the previously saved report in disk are
  // both uploaded to the telemetry server.
  int num_checks_per_upload_interval_;

  // The current report being uploaded.
  std::unique_ptr<ExtensionTelemetryReportRequest> active_report_;
  // The current uploader instance uploading the active report.
  std::unique_ptr<ExtensionTelemetryUploader> active_uploader_;

  // Enterprise-specific reporting variables:
  // Keeps track of the state of the service for enterprise telemetry reporting.
  bool enterprise_enabled_ = false;

  SignalProcessors enterprise_signal_processors_;
  SignalSubscribers enterprise_signal_subscribers_;

  // Stores data on extensions with generated signals for enterprise reporting.
  ExtensionStore enterprise_extension_store_;

  // Used for periodic collection of enterprise telemetry reports.
  base::RepeatingTimer enterprise_timer_;

  // Indicates whether |Shutdown| has been called.
  bool is_shutdown_ = false;

  friend class ExtensionTelemetryServiceTest;
  friend class ExtensionTelemetryServiceBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(ExtensionTelemetryServiceTest,
                           PersistsReportsOnInterval);
  FRIEND_TEST_ALL_PREFIXES(ExtensionTelemetryServiceTest,
                           MalformedPersistedFile);
  FRIEND_TEST_ALL_PREFIXES(ExtensionTelemetryServiceTest,
                           FileData_EnforcesCollectionDurationLimit);

  base::WeakPtrFactory<ExtensionTelemetryService> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_SERVICE_H_
