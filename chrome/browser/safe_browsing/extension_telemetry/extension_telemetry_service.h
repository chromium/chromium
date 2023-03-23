// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_SERVICE_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/common/extension_id.h"

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
class ExtensionTelemetryUploader;
class SafeBrowsingTokenFetcher;

// This class process extension signals and reports telemetry for a given
// profile (regular profile only). It is used exclusively on the UI thread.
// Lifetime:
// The service is instantiated when the associated profile is instantiated. It
// is destructed when the corresponding profile is destructed.
// Enable/Disable state:
// The service is enabled/disabled based on kEnhancedSafeBrowsing. The service
// subscribes to the SB preference change notification to update its state.
// When enabled, the service receives and stores signal information. It also
// periodically creates telemetry reports and uploads them to the SB servers.
// When disabled, any previously stored signal information is cleared, incoming
// signals are ignored and no reports are sent to the SB servers.
class ExtensionTelemetryService : public KeyedService {
 public:
  ExtensionTelemetryService(
      Profile* profile,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      extensions::ExtensionRegistry* extension_registry,
      extensions::ExtensionPrefs* extension_prefs);

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

  // Enables/disables the service.
  void SetEnabled(bool enable);
  bool enabled() const { return enabled_; }

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

 private:
  // Called when prefs that affect extension telemetry service are changed.
  void OnPrefChanged();

  // Creates and uploads telemetry reports.
  void CreateAndUploadReport();

  void OnUploadComplete(bool success);

  // Returns a bool that represents if there is any signal processor
  // information to report.
  bool SignalDataPresent() const;

  // Creates telemetry report protobuf for all extension store extensions
  // and currently installed extensions along with signal data retrieved from
  // signal processors.
  std::unique_ptr<ExtensionTelemetryReportRequest> CreateReport();

  // Dumps a telemetry report in logs for testing.
  void DumpReportForTest(const ExtensionTelemetryReportRequest& report);

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

  // Remove any data in the PrefService that from uninstalled extensions.
  void RemoveUninstalledExtensionsFileDataFromPref();

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
  absl::optional<OffstoreExtensionFileData> RetrieveOffstoreFileDataForReport(
      const extensions::ExtensionId& extension_id);

  // The persister object is bound to the threadpool. This prevents the
  // the read/write operations the `persister_` runs from blocking
  // the UI thread. It also allows the `persister_` object to be
  // destroyed cleanly while running tasks during Chrome shutdown.
  base::SequenceBound<ExtensionTelemetryPersister> persister_;

  // The |file_processor_| object reads and hashes offstore extension files.
  // Since these are blocking operations, it is bound to a different sequence
  // task runner.
  base::SequenceBound<ExtensionTelemetryFileProcessor> file_processor_;

  // The `config_manager_` manages all configurable variables of the
  // Extension Telemetry Service. Variables are stored in Chrome Prefs
  // between sessions.
  std::unique_ptr<safe_browsing::ExtensionTelemetryConfigManager>
      config_manager_;

  // The profile with which this instance of the service is associated.
  const raw_ptr<Profile> profile_;

  // The URLLoaderFactory used to issue network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Unowned object used for getting preference settings.
  raw_ptr<PrefService> pref_service_;

  // Observes changes to kSafeBrowsingEnhanced.
  PrefChangeRegistrar pref_change_registrar_;

  // Unowned objects used for getting extension information.
  const raw_ptr<extensions::ExtensionRegistry> extension_registry_;
  const raw_ptr<extensions::ExtensionPrefs> extension_prefs_;

  // Keeps track of the state of the service.
  bool enabled_ = false;

  // Used for periodic collection of telemetry reports.
  base::RepeatingTimer timer_;
  base::TimeDelta current_reporting_interval_;

  // The current report being uploaded.
  std::unique_ptr<ExtensionTelemetryReportRequest> active_report_;
  // The current uploader instance uploading the active report.
  std::unique_ptr<ExtensionTelemetryUploader> active_uploader_;

  // Maps extension id to extension data.
  using ExtensionStore = base::flat_map<
      extensions::ExtensionId,
      std::unique_ptr<ExtensionTelemetryReportRequest_ExtensionInfo>>;
  ExtensionStore extension_store_;

  // Maps offstore extension id to extension root path
  using OffstoreExtensionDirs =
      base::flat_map<extensions::ExtensionId, base::FilePath>;
  OffstoreExtensionDirs offstore_extension_dirs_;
  // Set of offstore extensions to process in order. Sorted by oldest last
  // processing time.
  base::flat_set<OffstoreExtensionFileDataContext>
      offstore_extension_file_data_contexts_;
  // Used to start the initial offstore extension file data collection based on
  // |kExtensionTelemetryFileDataStartupDelaySeconds| - default: 5 mins.
  // Then repeat the collection based on
  // |kExtensionTelemetryFileDataProcessIntervalSeconds| - default: 2 hours.
  base::OneShotTimer offstore_file_data_collection_timer_;
  base::TimeTicks offstore_file_data_collection_start_time_;

  using SignalProcessors =
      base::flat_map<ExtensionSignalType,
                     std::unique_ptr<ExtensionSignalProcessor>>;
  SignalProcessors signal_processors_;

  using SignalSubscribers =
      base::flat_map<ExtensionSignalType,
                     std::vector<ExtensionSignalProcessor*>>;
  SignalSubscribers signal_subscribers_;

  friend class ExtensionTelemetryServiceTest;
  friend class ExtensionTelemetryServiceBrowserTest;

  base::WeakPtrFactory<ExtensionTelemetryService> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_SERVICE_H_
