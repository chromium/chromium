// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_INCIDENT_REPORTING_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_INCIDENT_REPORTING_SERVICE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/incident_reporting/delayed_analysis_callback.h"
#include "chrome/browser/safe_browsing/incident_reporting/delayed_callback_runner.h"
#include "chrome/browser/safe_browsing/incident_reporting/download_metadata_manager.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_report_uploader.h"
#include "chrome/browser/safe_browsing/incident_reporting/last_download_finder.h"

class Profile;

namespace base {
class TaskRunner;
}

namespace content {
class DownloadManager;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace prefs {
namespace mojom {
class TrackedPreferenceValidationDelegate;
}
}

namespace safe_browsing {

BASE_DECLARE_FEATURE(kIncidentReportingEnableUpload);

class ClientDownloadRequest;
class ClientIncidentReport;
class ClientIncidentReport_DownloadDetails;
class ClientIncidentReport_EnvironmentData;
class ClientIncidentReport_ExtensionData;
class Incident;
class IncidentReceiver;
class SafeBrowsingServiceImpl;

// A class that manages the collection of incidents and submission of incident
// reports to the safe browsing client-side detection service. The service
// begins operation when an incident is reported via the AddIncident method.
// Incidents reported from a profile that is loading are held until the profile
// is fully created. Incidents originating from profiles that do not participate
// in safe browsing extended reporting are dropped. Process-wide incidents are
// affiliated with a profile that participates in safe browsing extended
// reporting when one becomes available.
// Following the addition of an incident that is not dropped, the service
// collects environmental data, finds the most recent binary download, and waits
// a bit. Additional incidents that arrive during this time are collated with
// the initial incident. Finally, already-reported incidents are pruned and any
// remaining are uploaded in an incident report.
// Lives on the UI thread.
class IncidentReportingService : public ProfileManagerObserver,
                                 public ProfileObserver {
 public:
  explicit IncidentReportingService(
      SafeBrowsingServiceImpl* safe_browsing_service);

  IncidentReportingService(const IncidentReportingService&) = delete;
  IncidentReportingService& operator=(const IncidentReportingService&) = delete;

  // All incident collection, data collection, and uploads in progress are
  // dropped at destruction.
  ~IncidentReportingService() override;

  // Returns true if incident reporting is enabled for the given profile.
  static bool IsEnabledForProfile(Profile* profile);

  // Returns an object by which external components can add an incident to the
  // service. The object may outlive the service, but will no longer have any
  // effect after the service is deleted.
  std::unique_ptr<IncidentReceiver> GetIncidentReceiver();

  // Returns a preference validation delegate that adds incidents to the service
  // for validation failures in |profile|. The delegate may outlive the service,
  // but incidents reported by it will no longer have any effect after the
  // service is deleted.
  std::unique_ptr<prefs::mojom::TrackedPreferenceValidationDelegate>
  CreatePreferenceValidationDelegate(Profile* profile);

  // Registers |callback| to be run after some delay following process launch.
  void RegisterDelayedAnalysisCallback(DelayedAnalysisCallback callback);

  // Adds |download_manager| to the set monitored for client download request
  // storage.
  void AddDownloadManager(content::DownloadManager* download_manager);

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

 protected:
  // A pointer to a function that populates a protobuf with environment data.
  typedef void (*CollectEnvironmentDataFn)(
      ClientIncidentReport_EnvironmentData*);

  // For testing so that the TaskRunner used for delayed analysis callbacks can
  // be specified.
  IncidentReportingService(
      SafeBrowsingServiceImpl* safe_browsing_service,
      base::TimeDelta delayed_task_interval,
      const scoped_refptr<base::TaskRunner>& delayed_task_runner);

  // Sets the function called by the service to collect environment data and the
  // task runner on which it is called. Used by unit tests to provide a fake
  // environment data collector.
  void SetCollectEnvironmentHook(
      CollectEnvironmentDataFn collect_environment_data_hook,
      const scoped_refptr<base::TaskRunner>& task_runner);

  // Initiates extension collection. Overriden by unit tests to provide fake
  // extension data.
  virtual void DoExtensionCollection(
      ClientIncidentReport_ExtensionData* extension_data);

  // Initiates a search for the most recent binary download. Overriden by unit
  // tests to provide a fake finder.
  virtual std::unique_ptr<LastDownloadFinder> CreateDownloadFinder(
      LastDownloadFinder::LastDownloadCallback callback);

  // Initiates an upload. Overridden by unit tests to provide a fake uploader.
  virtual std::unique_ptr<IncidentReportUploader> StartReportUpload(
      IncidentReportUploader::OnResultCallback callback,
      const ClientIncidentReport& report);

  // Returns true if a report is currently being processed.
  bool IsProcessingReport() const;

 private:
  struct ProfileContext;
  class UploadContext;
  class Receiver;

  // Returns the context for |profile|, creating it if it does not exist.
  ProfileContext* GetOrCreateProfileContext(Profile* profile);

  // Returns the context for |profile|, or NULL if it is unknown.
  ProfileContext* GetProfileContext(Profile* profile);

  // Returns an initialized profile for which incident reporting is enabled.
  Profile* FindEligibleProfile() const;

  // Adds |incident_data| relating to the optional |profile| to the service.
  void AddIncident(Profile* profile, std::unique_ptr<Incident> incident);

  // Clears all data associated with the |incident| relating to the optional
  // |profile|.
  void ClearIncident(Profile* profile, std::unique_ptr<Incident> incident);

  // Returns true if there are incidents waiting to be sent.
  bool HasIncidentsToUpload() const;

  // Begins processing a report. If processing is already underway, ensures that
  // collection tasks have completed or are running.
  void BeginReportProcessing();

  // Begins the process of collating incidents by waiting for incidents to
  // arrive. This function is idempotent.
  void BeginIncidentCollation();

  // Returns true if the service is waiting for additional incidents before
  // uploading a report.
  bool WaitingToCollateIncidents();

  // Cancels the collection timeout.
  void CancelIncidentCollection();

  // A callback invoked on the UI thread after which incident collation has
  // completed. Incident report processing continues, either by waiting for
  // environment data or the most recent download to arrive or by sending an
  // incident report.
  void OnCollationTimeout();

  // Starts a task to collect environment data in the blocking pool.
  void BeginEnvironmentCollection();

  // Returns true if the environment collection task is outstanding.
  bool WaitingForEnvironmentCollection();

  // Cancels any pending environment collection task and drops any data that has
  // already been collected.
  void CancelEnvironmentCollection();

  // A callback invoked on the UI thread when environment data collection is
  // complete. Incident report processing continues, either by waiting for the
  // collection timeout or by sending an incident report.
  void OnEnvironmentDataCollected(
      std::unique_ptr<ClientIncidentReport_EnvironmentData> environment_data);

  // Starts the asynchronous process of finding the most recent executable
  // download if one is not currently being search for and/or has not already
  // been found.
  void BeginDownloadCollection();

  // True if the service is waiting to discover the most recent download either
  // because a task to do so is outstanding, or because one or more profiles
  // have yet to be added to the ProfileManager.
  bool WaitingForMostRecentDownload();

  // Cancels the search for the most recent executable download.
  void CancelDownloadCollection();

  // A callback invoked on the UI thread by the last download finder when the
  // search for the most recent binary download and most recent non-binary
  // download is complete.
  void OnLastDownloadFound(
      std::unique_ptr<ClientIncidentReport_DownloadDetails>
          last_binary_download,
      std::unique_ptr<ClientIncidentReport_NonBinaryDownloadDetails>
          last_non_binary_download);

  // Processes all received incidents once all data collection is
  // complete. Incidents originating from profiles that do not participate in
  // safe browsing extended reporting are dropped, incidents that have already
  // been reported are pruned, and prune state is cleared for incidents that are
  // now clear. Report upload is started if any incidents remain.
  void ProcessIncidentsIfCollectionComplete();

  // Cancels all uploads, discarding all reports and responses in progress.
  void CancelAllReportUploads();

  // Continues an upload if uploading is enabled.
  void UploadReportIfUploadingEnabled(UploadContext* context);

  // Performs processing for a report after succesfully receiving a response.
  void HandleResponse(const UploadContext& context);

  // IncidentReportUploader::OnResultCallback implementation.
  void OnReportUploadResult(UploadContext* context,
                            IncidentReportUploader::Result result,
                            std::unique_ptr<ClientIncidentResponse> response);

  // DownloadProtectionService::ClientDownloadRequestCallback implementation.
  void OnClientDownloadRequest(download::DownloadItem* download,
                               const ClientDownloadRequest* request);

  // Accessor for an URLLoaderFactory with which reports will be sent.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // A pointer to a function that collects environment data. The function will
  // be run by |environment_collection_task_runner_|. This is ordinarily
  // CollectEnvironmentData, but may be overridden by tests; see
  // SetCollectEnvironmentHook.
  CollectEnvironmentDataFn collect_environment_data_fn_;

  // The task runner on which environment collection takes place. This is
  // ordinarily a runner in the browser's blocking pool that will skip the
  // collection task at shutdown if it has not yet started.
  scoped_refptr<base::TaskRunner> environment_collection_task_runner_;

  // A subscription for ClientDownloadRequests, used to persist them for later
  // use.
  base::CallbackListSubscription client_download_request_subscription_;

  // True when the asynchronous environment collection task has been fired off
  // but has not yet completed.
  bool environment_collection_pending_ = false;

  // True when an incident has been received and the service is waiting for the
  // collation_timer_ to fire.
  bool collation_timeout_pending_ = false;

  // A timer upon the firing of which the service will report received
  // incidents.
  base::DelayTimer collation_timer_;

  // The report currently being assembled. This becomes non-NULL when an initial
  // incident is reported, and returns to NULL when the report is sent for
  // upload.
  std::unique_ptr<ClientIncidentReport> report_;

  // The time at which the initial incident is reported.
  base::Time first_incident_time_;

  // Context data for all on-the-record profiles plus the process-wide (NULL)
  // context. A mapping of profiles to contexts holding state about received
  // incidents.
  std::map<Profile*, std::unique_ptr<ProfileContext>> profiles_;

  // Callbacks registered for performing delayed analysis.
  DelayedCallbackRunner delayed_analysis_callbacks_;

  DownloadMetadataManager download_metadata_manager_;

  // The collection of uploads in progress.
  std::vector<std::unique_ptr<UploadContext>> uploads_;

  // An object that asynchronously searches for the most recent binary download.
  // Non-NULL while such a search is outstanding.
  std::unique_ptr<LastDownloadFinder> last_download_finder_;

  // A factory for handing out weak pointers for IncidentReceiver objects.
  base::WeakPtrFactory<IncidentReportingService> receiver_weak_ptr_factory_{
      this};

  // A factory for handing out weak pointers for internal asynchronous tasks
  // that are posted during normal processing (e.g., environment collection,
  // and report uploads).
  base::WeakPtrFactory<IncidentReportingService> weak_ptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_INCIDENT_REPORTING_SERVICE_H_
