// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_IMPL_H_
#define CHROME_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/background_fetch/background_fetch_delegate_base.h"
#include "components/download/public/background_service/download_params.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_items_collection/core/update_delta.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

class Profile;

namespace offline_items_collection {
class OfflineContentAggregator;
}  // namespace offline_items_collection

// Implementation of BackgroundFetchDelegateBase which makes use of offline
// items collection to show UI.
class BackgroundFetchDelegateImpl
    : public background_fetch::BackgroundFetchDelegateBase,
      public offline_items_collection::OfflineContentProvider,
      public KeyedService {
 public:
  explicit BackgroundFetchDelegateImpl(Profile* profile);
  BackgroundFetchDelegateImpl(const BackgroundFetchDelegateImpl&) = delete;
  BackgroundFetchDelegateImpl& operator=(const BackgroundFetchDelegateImpl&) =
      delete;
  ~BackgroundFetchDelegateImpl() override;

  // BackgroundFetchDelegate implementation:
  void MarkJobComplete(const std::string& job_id) override;
  void UpdateUI(const std::string& job_unique_id,
                const std::optional<std::string>& title,
                const std::optional<SkBitmap>& icon) override;

  // OfflineContentProvider implementation:
  void OpenItem(const offline_items_collection::OpenParams& open_params,
                const offline_items_collection::ContentId& id) override;
  void RemoveItem(const offline_items_collection::ContentId& id) override;
  void CancelDownload(const offline_items_collection::ContentId& id) override;
  void PauseDownload(const offline_items_collection::ContentId& id) override;
  void ResumeDownload(const offline_items_collection::ContentId& id) override;
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

 protected:
  // BackgroundFetchDelegateBase:
  download::BackgroundDownloadService* GetDownloadService() override;
  void OnJobDetailsCreated(const std::string& job_id) override;
  void DoShowUi(const std::string& job_id) override;
  void DoUpdateUi(const std::string& job_id) override;
  void DoCleanUpUi(const std::string& job_id) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BackgroundFetchBrowserTest, ClickEventIsDispatched);
  FRIEND_TEST_ALL_PREFIXES(BackgroundFetchDelegateImplTest, RecordUkmEvent);
  FRIEND_TEST_ALL_PREFIXES(BackgroundFetchDelegateImplTest,
                           HistoryServiceIntegration);
  FRIEND_TEST_ALL_PREFIXES(BackgroundFetchDelegateImplTest,
                           HistoryServiceIntegrationUrlVsOrigin);

  // Represents the data which is associated with a particular BGF job and is
  // relevant to the UI.
  struct UiState {
    UiState();
    ~UiState();

    offline_items_collection::OfflineItem offline_item;
    std::optional<offline_items_collection::UpdateDelta> update_delta;
  };

  // Updates the entry in |ui_state_map_| based on the corresponding JobDetails
  // object.
  void UpdateOfflineItem(const std::string& job_id);

  // Helper methods for recording BackgroundFetchDeletingRegistration UKM event.
  // We check with UkmBackgroundRecorderService whether this event for |origin|
  // can be recorded.
  void RecordBackgroundFetchDeletingRegistrationUkmEvent(
      const url::Origin& origin,
      bool user_initiated_abort);
  void DidGetBackgroundSourceId(bool user_initiated_abort,
                                std::optional<ukm::SourceId> source_id);

  // The profile this service is being created for.
  raw_ptr<Profile> profile_;

  // The namespace provided to the |offline_content_aggregator_| and used when
  // creating Content IDs.
  std::string provider_namespace_;

  // A map from job id to associated UI state.
  std::map<std::string, UiState> ui_state_map_;

  raw_ptr<offline_items_collection::OfflineContentAggregator>
      offline_content_aggregator_;

  base::WeakPtrFactory<BackgroundFetchDelegateImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_IMPL_H_
