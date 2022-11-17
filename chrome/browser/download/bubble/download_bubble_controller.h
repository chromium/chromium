// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_CONTROLLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/download/offline_item_model.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "content/public/browser/download_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

using OfflineItemState = ::offline_items_collection::OfflineItemState;
using ContentId = ::offline_items_collection::ContentId;
using OfflineContentProvider =
    ::offline_items_collection::OfflineContentProvider;
using OfflineContentAggregator =
    ::offline_items_collection::OfflineContentAggregator;
using OfflineItem = ::offline_items_collection::OfflineItem;
using UpdateDelta = ::offline_items_collection::UpdateDelta;
using DownloadUIModelPtr = ::DownloadUIModel::DownloadUIModelPtr;
using OfflineItemList =
    ::offline_items_collection::OfflineContentAggregator::OfflineItemList;

class DownloadBubbleUIController
    : public OfflineContentProvider::Observer,
      public download::AllDownloadItemNotifier::Observer {
 public:
  explicit DownloadBubbleUIController(Browser* browser);
  DownloadBubbleUIController(const DownloadBubbleUIController&) = delete;
  DownloadBubbleUIController& operator=(const DownloadBubbleUIController&) =
      delete;
  ~DownloadBubbleUIController() override;

  // Get the entries for the main view of the Download Bubble. The main view
  // contains all the recent downloads (finished within the last 24 hours).
  std::vector<DownloadUIModelPtr> GetMainView();

  // Get the entries for the partial view of the Download Bubble. The partial
  // view contains in-progress and uninteracted downloads, meant to capture the
  // user's recent tasks. This can only be opened by the browser in the event of
  // new downloads, and user action only creates a main view.
  std::vector<DownloadUIModelPtr> GetPartialView();

  // Get all entries that should be displayed in the UI, including downloads and
  // offline items.
  std::vector<DownloadUIModelPtr> GetAllItemsToDisplay();

  // The list is needed to populate GetAllItemsToDisplay.
  virtual const OfflineItemList& GetOfflineItems();

  // The list is needed to populate GetAllItemsToDisplay.
  virtual const std::vector<download::DownloadItem*> GetDownloadItems();

  // This function makes sure that the offline items field is
  // populated, and then calls the given callback. After this, GetOfflineItems
  // will return a populated list.
  virtual void InitOfflineItems(DownloadDisplayController* display_controller,
                                base::OnceCallback<void()> callback);

  // Process button press on the bubble.
  void ProcessDownloadButtonPress(DownloadUIModel* model,
                                  DownloadCommands::Command command,
                                  bool is_main_view);

  // Notify when a new download is ready to be shown on UI, and if the window
  // this controller belongs to should show the partial view.
  void OnNewItem(download::DownloadItem* item, bool show_details);

  // Notify when a download toolbar button (in any window) is pressed.
  void HandleButtonPressed();

  // Returns whether the incognito icon should be shown for the download.
  bool ShouldShowIncognitoIcon(const DownloadUIModel* model) const;

  // Schedules the ephemeral warning download to be canceled. It will only be
  // canceled if it continues to be an ephemeral warning that hasn't been acted
  // on when the scheduled time arrives.
  void ScheduleCancelForEphemeralWarning(const std::string& guid);

  // Force the controller to hide the download UI entirely, including the bubble
  // and the toolbar icon. This function should only be called if the event is
  // triggered outside of normal download events that are not listened by
  // observers.
  void HideDownloadUi();

  // Returns the DownloadDisplayController. Should always return a valid
  // controller.
  DownloadDisplayController* GetDownloadDisplayController() {
    return display_controller_;
  }

  download::AllDownloadItemNotifier& get_download_notifier_for_testing() {
    return download_notifier_;
  }

  download::AllDownloadItemNotifier* get_original_notifier_for_testing() {
    return original_notifier_.get();
  }

  void set_manager_for_testing(content::DownloadManager* manager) {
    download_manager_ = manager;
  }

 private:
  friend class DownloadBubbleUIControllerTest;
  friend class DownloadBubbleUIControllerIncognitoTest;
  // AllDownloadItemNotifier::Observer
  void OnDownloadUpdated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnDownloadRemoved(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnManagerGoingDown(content::DownloadManager* manager) override;

  // OfflineContentProvider::Observer
  void OnItemsAdded(
      const OfflineContentProvider::OfflineItemList& items) override;
  void OnItemRemoved(const ContentId& id) override;
  void OnItemUpdated(const OfflineItem& item,
                     const absl::optional<UpdateDelta>& update_delta) override;
  void OnContentProviderGoingDown() override;

  // Try to add the items to the set/list(s) and calling callback on completion.
  void MaybeAddOfflineItems(base::OnceCallback<void()> callback,
                            bool is_new,
                            const OfflineItemList& offline_items);

  // Try to add the new item to the list, returning success status.
  bool MaybeAddOfflineItem(const OfflineItem& item, bool is_new);

  // Prune OfflineItems to recent items to in-progress offline items, or
  // downloads started in the last day.
  void PruneOfflineItems();

  // Common method for getting main and partial views.
  std::vector<DownloadUIModelPtr> GetDownloadUIModels(bool is_main_view);

  // Kick off retrying an eligible interrupted download.
  void RetryDownload(DownloadUIModel* model, DownloadCommands::Command command);

  raw_ptr<Browser, DanglingUntriaged> browser_;
  raw_ptr<Profile, DanglingUntriaged> profile_;
  raw_ptr<content::DownloadManager, DanglingUntriaged> download_manager_;
  download::AllDownloadItemNotifier download_notifier_;
  // Null if the profile is not off the record.
  std::unique_ptr<download::AllDownloadItemNotifier> original_notifier_;
  raw_ptr<OfflineContentAggregator, DanglingUntriaged> aggregator_;
  raw_ptr<OfflineItemModelManager, DanglingUntriaged> offline_manager_;
  base::ScopedObservation<OfflineContentProvider,
                          OfflineContentProvider::Observer>
      observation_{this};
  // DownloadDisplayController and DownloadBubbleUIController have the same
  // lifetime. Both are owned, constructed together, and destructed together by
  // DownloadToolbarButtonView. If one is valid, so is the other.
  raw_ptr<DownloadDisplayController, DanglingUntriaged> display_controller_;

  // Pruned list of offline items.
  OfflineItemList offline_items_;

  absl::optional<base::Time> last_partial_view_shown_time_ = absl::nullopt;

  base::WeakPtrFactory<DownloadBubbleUIController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_CONTROLLER_H_
