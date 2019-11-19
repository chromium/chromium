// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_IMPL_H_
#define CHROME_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "components/download/public/background_service/download_params.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_items_collection/core/update_delta.h"
#include "content/public/browser/background_fetch_delegate.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/gfx/image/image.h"
#include "url/origin.h"

class Profile;

namespace download {
class DownloadService;
}  // namespace download

namespace offline_items_collection {
class OfflineContentAggregator;
}  // namespace offline_items_collection

// Implementation of BackgroundFetchDelegate using the DownloadService. This
// also implements OfflineContentProvider which allows it to show notifications
// for its downloads.
class BackgroundFetchDelegateImpl
    : public content::BackgroundFetchDelegate,
      public offline_items_collection::OfflineContentProvider,
      public KeyedService {
 public:
  BackgroundFetchDelegateImpl(Profile* profile,
                              const std::string& provider_namespace);

  ~BackgroundFetchDelegateImpl() override;

  // Lazily initializes and returns the DownloadService.
  download::DownloadService* GetDownloadService();

  // KeyedService implementation:
  void Shutdown() override;

  // BackgroundFetchDelegate implementation:
  void GetIconDisplaySize(GetIconDisplaySizeCallback callback) override;
  void GetPermissionForOrigin(const url::Origin& origin,
                              const content::WebContents::Getter& wc_getter,
                              GetPermissionForOriginCallback callback) override;
  void CreateDownloadJob(base::WeakPtr<Client> client,
                         std::unique_ptr<content::BackgroundFetchDescription>
                             fetch_description) override;
  void DownloadUrl(const std::string& job_unique_id,
                   const std::string& guid,
                   const std::string& method,
                   const GURL& url,
                   const net::NetworkTrafficAnnotationTag& traffic_annotation,
                   const net::HttpRequestHeaders& headers,
                   bool has_request_body) override;
  void Abort(const std::string& job_unique_id) override;
  void MarkJobComplete(const std::string& job_unique_id) override;
  void UpdateUI(const std::string& job_unique_id,
                const base::Optional<std::string>& title,
                const base::Optional<SkBitmap>& icon) override;

  // Abort all ongoing downloads and fail the fetch. Currently only used when
  // the bytes downloaded exceed the total download size, if specified.
  void FailFetch(const std::string& job_unique_id);

  void OnDownloadStarted(
      const std::string& guid,
      std::unique_ptr<content::BackgroundFetchResponse> response);

  void OnDownloadUpdated(const std::string& guid,
                         uint64_t bytes_uploaded,
                         uint64_t bytes_downloaded);

  void OnDownloadFailed(const std::string& guid,
                        std::unique_ptr<content::BackgroundFetchResult> result);

  void OnDownloadSucceeded(
      const std::string& guid,
      std::unique_ptr<content::BackgroundFetchResult> result);

  // OfflineContentProvider implementation:
  void OpenItem(offline_items_collection::LaunchLocation location,
                const offline_items_collection::ContentId& id) override;
  void RemoveItem(const offline_items_collection::ContentId& id) override;
  void CancelDownload(const offline_items_collection::ContentId& id) override;
  void PauseDownload(const offline_items_collection::ContentId& id) override;
  void ResumeDownload(const offline_items_collection::ContentId& id,
                      bool has_user_gesture) override;
  void GetItemById(const offline_items_collection::ContentId& id,
                   SingleItemCallback callback) override;
  void GetAllItems(MultipleItemCallback callback) override;
  void GetVisualsForItem(const offline_items_collection::ContentId& id,
                         GetVisualsOptions options,
                         VisualsCallback callback) override;
  void GetShareInfoForItem(const offline_items_collection::ContentId& id,
                           ShareCallback callback) override;
  void RenameItem(const offline_items_collection::ContentId& id,
                  const std::string& name,
                  RenameCallback callback) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Whether the provided GUID is resuming from the perspective of Background
  // Fetch.
  bool IsGuidOutstanding(const std::string& guid) const;

  // Notifies the OfflineContentAggregator of an interrupted download that is
  // in a paused state.
  void RestartPausedDownload(const std::string& download_guid);

  // Returns the set of download GUIDs that have started but did not finish
  // according to Background Fetch. Clears out all references to outstanding
  // GUIDs.
  std::set<std::string> TakeOutstandingGuids();

  // Gets the upload data, if any, associated with the |download_guid|.
  void GetUploadData(const std::string& download_guid,
                     download::GetUploadDataCallback callback);

  void set_ukm_event_recorded_for_testing(base::OnceClosure closure) {
    ukm_event_recorded_for_testing_ = std::move(closure);
  }

  base::WeakPtr<BackgroundFetchDelegateImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(BackgroundFetchBrowserTest, ClickEventIsDispatched);
  FRIEND_TEST_ALL_PREFIXES(BackgroundFetchDelegateImplTest, RecordUkmEvent);
  FRIEND_TEST_ALL_PREFIXES(BackgroundFetchDelegateImplTest,
                           HistoryServiceIntegration);
  FRIEND_TEST_ALL_PREFIXES(BackgroundFetchDelegateImplTest,
                           HistoryServiceIntegrationUrlVsOrigin);

  struct JobDetails {
    // If a job is part of the |job_details_map_|, it will have one of these
    // states.
    enum class State {
      kPendingWillStartPaused,
      kPendingWillStartDownloading,
      kStartedButPaused,
      kStartedAndDownloading,
      // The job was aborted.
      kCancelled,
      // All requests were processed (either succeeded or failed).
      kDownloadsComplete,
      // The appropriate completion event (success, fail, abort) has been
      // dispatched.
      kJobComplete,
    };

    JobDetails(JobDetails&&);
    JobDetails(
        base::WeakPtr<Client> client,
        std::unique_ptr<content::BackgroundFetchDescription> fetch_description,
        const std::string& provider_namespace,
        bool is_off_the_record);
    ~JobDetails();

    void UpdateOfflineItem();
    void MarkJobAsStarted();
    void UpdateJobOnDownloadComplete(const std::string& download_guid);

    // Returns how many bytes have been processed by the Download Service so
    // far.
    uint64_t GetProcessedBytes() const;

    // Returns the number of downloaded bytes, including for the in progress
    // requests.
    uint64_t GetDownloadedBytes() const;

    void UpdateInProgressBytes(const std::string& download_guid,
                               uint64_t bytes_uploaded,
                               uint64_t bytes_downloaded);

    struct RequestData {
      enum class Status {
        kAbsent,
        kIncluded,
      };

      explicit RequestData(bool has_upload_data);
      ~RequestData();

      Status status = Status::kAbsent;

      uint64_t body_size_bytes = 0u;
      uint64_t in_progress_uploaded_bytes = 0u;
      uint64_t in_progress_downloaded_bytes = 0u;
    };

    // The client to report the Background Fetch updates to.
    base::WeakPtr<Client> client;

    // Set of DownloadService GUIDs that are currently processed. They are
    // added by DownloadUrl and are removed when the fetch completes, fails,
    // or is cancelled.
    std::map<std::string, RequestData> current_fetch_guids;

    offline_items_collection::OfflineItem offline_item;
    base::Optional<offline_items_collection::UpdateDelta> update_delta;
    State job_state;
    std::unique_ptr<content::BackgroundFetchDescription> fetch_description;
    bool cancelled_from_ui = false;

    base::OnceClosure on_resume;

   private:
    // Whether we should report progress of the job in terms of size of
    // downloads or in terms of the number of files being downloaded.
    bool ShouldReportProgressBySize();

    // Returns the number of bytes processed by in-progress requests.
    uint64_t GetInProgressBytes() const;

    DISALLOW_COPY_AND_ASSIGN(JobDetails);
  };

  // Starts a download according to |params| belonging to |job_unique_id|.
  void StartDownload(const std::string& job_unique_id,
                     const download::DownloadParams& params,
                     bool has_request_body);

  // Updates the OfflineItem that controls the contents of download
  // notifications and notifies any OfflineContentProvider::Observer that was
  // registered with this instance.
  void UpdateOfflineItemAndUpdateObservers(JobDetails* job_details);

  void OnDownloadReceived(const std::string& guid,
                          download::DownloadParams::StartResult result);

  // The callback passed to DownloadRequestLimiter::CanDownload().
  void DidGetPermissionFromDownloadRequestLimiter(
      GetPermissionForOriginCallback callback,
      bool has_permission);

  void DidGetUploadData(const std::string& unique_id,
                        const std::string& download_guid,
                        download::GetUploadDataCallback callback,
                        blink::mojom::SerializedBlobPtr blob);

  // Returns the client for a given |job_unique_id|.
  base::WeakPtr<Client> GetClient(const std::string& job_unique_id);

  // Helper methods for recording BackgroundFetchDeletingRegistration UKM event.
  // We check with UkmBackgroundRecorderService whether this event for |origin|
  // can be recorded.
  void RecordBackgroundFetchDeletingRegistrationUkmEvent(
      const url::Origin& origin,
      bool user_initiated_abort);
  void DidGetBackgroundSourceId(bool user_initiated_abort,
                                base::Optional<ukm::SourceId> source_id);

  // The profile this service is being created for.
  Profile* profile_;

  // The namespace provided to the |offline_content_aggregator_| and used when
  // creating Content IDs.
  std::string provider_namespace_;

  // The BackgroundFetchDelegateImplFactory depends on the
  // DownloadServiceFactory, so |download_service_| should outlive |this|.
  download::DownloadService* download_service_ = nullptr;

  // Map from individual download GUIDs to job unique ids.
  std::map<std::string, std::string> download_job_unique_id_map_;

  // Map from job unique ids to the details of the job.
  std::map<std::string, JobDetails> job_details_map_;

  offline_items_collection::OfflineContentAggregator*
      offline_content_aggregator_;

  // Set of Observers to be notified of any changes to the shown notifications.
  std::set<Observer*> observers_;

  // Testing-only closure to inform tests when a UKM event has been recorded.
  base::OnceClosure ukm_event_recorded_for_testing_;

  base::WeakPtrFactory<BackgroundFetchDelegateImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchDelegateImpl);
};

#endif  // CHROME_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_IMPL_H_
