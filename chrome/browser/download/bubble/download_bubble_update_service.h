// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_UPDATE_SERVICE_H_
#define CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_UPDATE_SERVICE_H_

#include <map>
#include <optional>
#include <vector>

#include "base/dcheck_is_on.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/download/bubble/download_bubble_accessible_alerts_map.h"
#include "chrome/browser/download/bubble/download_bubble_display_info.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/download/download_display.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace content {
class DownloadManager;
}  // namespace content

namespace download {
class DownloadItem;
}  // namespace download

// Caches download items and offline items in sorted order, so that UI updates
// can be processed more quickly without fetching and sorting all items every
// time. Passes notifications on to the window-level UI controllers.
class DownloadBubbleUpdateService
    : public KeyedService,
      public download::AllDownloadItemNotifier::Observer,
      public offline_items_collection::OfflineContentProvider::Observer,
      public DownloadHistory::Observer {
 public:
  // Defines sort priority for items.
  struct ItemSortKey {
    enum State {
      kInProgressActive = 0,
      kInProgressPaused = 1,
      kNotInProgress = 2,
    };

    bool operator<(const ItemSortKey& other) const;
    bool operator==(const ItemSortKey& other) const;
    bool operator!=(const ItemSortKey& other) const;
    bool operator>(const ItemSortKey& other) const;

    // Returns a key that sorts before any other.
    static ItemSortKey Min();

    // Active in-progress items come before paused items, which come before
    // not-in-progress items.
    State state;
    // Within each state, items are sorted in reverse chronological order by
    // start time (most recent first).
    base::Time start_time;
  };

  explicit DownloadBubbleUpdateService(Profile* profile);
  DownloadBubbleUpdateService(const DownloadBubbleUpdateService&) = delete;
  DownloadBubbleUpdateService& operator=(const DownloadBubbleUpdateService&) =
      delete;

  ~DownloadBubbleUpdateService() override;

  // Gets models for the top GetMaxNumItemsToShow() combined download items
  // and offline items, in sorted order. If |web_app_id| is non-null, the
  // results are limited to downloads initiated by the specified web app,
  // otherwise the results are limited to downloads initiated by normal Chrome
  // windows. May cause items to be pruned from the cache, if they have grown
  // too old to be included. May trigger backfilling the caches, but does not
  // wait for backfill results, unless |force_backfill_download_items| is true
  // (in which case download items will be backfilled synchronously if
  // necessary; offline items will not be backfilled synchronously). |models| is
  // cleared. Returns whether results are complete. Results may not be complete
  // if there might be more items to be returned after backfilling. Virtual for
  // testing.
  virtual bool GetAllModelsToDisplay(
      std::vector<DownloadUIModel::DownloadUIModelPtr>& models,
      const webapps::AppId* web_app_id,
      bool force_backfill_download_items = false);

  // Returns information relevant to the display state of the download button.
  // If |web_app_id| is non-null, the results are limited to downloads initiated
  // by the specified web app, otherwise the results are limited to downloads
  // initiated by normal Chrome windows. Does not prune the cache or backfill
  // missing items. May be slightly inaccurate in edge cases. Virtual for
  // testing.
  virtual const DownloadBubbleDisplayInfo& GetDisplayInfo(
      const webapps::AppId* web_app_id);

  // Computes progress info based on in-progress downloads. If |web_app_id| is
  // non-null, the results are limited to downloads initiated by the specified
  // web app, otherwise the results are limited to downloads initiated by normal
  // Chrome windows. Does not prune the cache or backfill missing items, so the
  // returned progress info may be slightly inaccurate in edge cases. This is
  // ok, as it is only for the purpose of showing a progress ring around the
  // icon, which is not precise anyway. Virtual for testing.
  virtual DownloadDisplay::ProgressInfo GetProgressInfo(
      const webapps::AppId* web_app_id) const;

  // Gets a list of accessible alerts that have built up since the last time
  // they were taken. May return an empty vector. Removes those alerts from
  // `accessible_alerts_`. If |web_app_id| is non-null, the results are limited
  // to downloads initiated by the specified web app, otherwise the results are
  // limited to downloads initiated by normal Chrome windows. Virtual for
  // testing.
  virtual std::vector<std::u16string> TakeAccessibleAlertsForAnnouncement(
      const webapps::AppId* web_app_id);

  // Notifies the appropriate browser windows that a download item was added.
  void NotifyWindowsOfDownloadItemAdded(download::DownloadItem* item);

  // Sets up the download history observation.
  void ObserveDownloadHistory();

  // Initializes AllDownloadItemNotifier for the current profile, and
  // initializes caches. This is called when the manager is ready, to signal
  // that the DownloadBubbleUpdateService should begin tracking downloads. This
  // starts initialization of both the download items and the offline items.
  // Should only be called once.
  void Initialize(content::DownloadManager* manager);

  // Initializes the AllDownloadItemNotifier for the original profile, if
  // |profile_| is off the record. May trigger re-initialization of the download
  // items cache.
  void InitializeOriginalNotifier(content::DownloadManager* manager);

  // Get the DownloadManager that |download_item_notifier_| is listening to.
  content::DownloadManager* GetDownloadManager();

  // Virtual for testing.
  virtual bool IsInitialized() const;

  // KeyedService:
  void Shutdown() override;

  bool IsShutDown() const;

  // download::AllDownloadItemNotifier::Observer:
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnDownloadUpdated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnDownloadRemoved(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnManagerGoingDown(content::DownloadManager* manager) override;

  // offline_items_collection::OfflineContentProvider::Observer:
  void OnItemsAdded(
      const offline_items_collection::OfflineContentProvider::OfflineItemList&
          items) override;
  void OnItemRemoved(const offline_items_collection::ContentId& id) override;
  void OnItemUpdated(const offline_items_collection::OfflineItem& item,
                     const std::optional<offline_items_collection::UpdateDelta>&
                         update_delta) override;
  void OnContentProviderGoingDown() override;

  // DownloadHistory::Observer:
  void OnHistoryQueryComplete() override;
  void OnDownloadHistoryDestroyed() override;

  OfflineItemModelManager* GetOfflineManager() const;

  bool IsProfileOtr() const;

  // Returns the max number of combined download items and offline items that
  // will be returned from GetAllModelsToDisplay(). Applies to each
  // CacheManager.
  size_t GetMaxNumItemsToShow() const;

  // Returns the max number of items (of each type) to cache. This is slightly
  // more than the max number of items to show. Applies to each CacheManager.
  size_t GetNumItemsToCache() const;

  // Gets all items from the DownloadManager/ContentProvider, finds the top
  // items that sort at or after |last_key| and adds them to the cache such that
  // the total number of items does not exceed the max. The Start*() versions
  // just post a task to kick off backfilling while the other two perform the
  // backfilling synchronously. Note that it is ok if other additions/deletions
  // happen while the backfill task is queued. If an item is inserted before
  // last_key then it would have been there anyway. If an item is inserted
  // after last_key, it is the same as if it were added during backfilling. If
  // an item is removed before last_key, then there is just more space to
  // backfill.
  void StartBackfillDownloadItems(const ItemSortKey& last_key);
  void BackfillDownloadItems(const ItemSortKey& last_key);
  void StartBackfillOfflineItems(const ItemSortKey& last_key);
  void BackfillOfflineItems(
      const ItemSortKey& last_key,
      const std::vector<offline_items_collection::OfflineItem>& all_items);

  // Logic in CacheManager assumes the max is at least 2.
  void set_max_num_items_to_show_for_testing(size_t max) {
    max_num_items_to_show_for_testing_ = max;
  }

  void set_extra_items_to_cache_for_testing(size_t items) {
    extra_items_to_cache_for_testing_ = items;
  }

  download::AllDownloadItemNotifier& download_item_notifier_for_testing() {
    return *download_item_notifier_;
  }

  download::AllDownloadItemNotifier&
  original_download_item_notifier_for_testing() {
    return *original_download_item_notifier_;
  }

  bool download_history_loaded() const { return download_history_loaded_; }

  base::WeakPtr<DownloadBubbleUpdateService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  // Encapsulates the caching functionality of DownloadBubbleUpdateService.
  // Holds two caches, one for DownloadItems and one for OfflineItems, and their
  // associated indexes and aggregate info (DownloadBubbleDisplayInfo).
  // Represents one "namespace" of items for the download bubble, corresponding
  // to either a single web app or all the regular (non-app) Chrome browsers.
  class CacheManager {
   public:
    template <typename Item>
    using SortedItems = std::multimap<ItemSortKey, Item>;
    template <typename Id, typename Item>
    using IterMap = std::map<Id, typename SortedItems<Item>::iterator>;

    using SortedDownloadItems = SortedItems<download::DownloadItem*>;
    using SortedOfflineItems =
        SortedItems<offline_items_collection::OfflineItem>;
    using DownloadItemIterMap = IterMap<std::string, download::DownloadItem*>;
    using OfflineItemIterMap = IterMap<offline_items_collection::ContentId,
                                       offline_items_collection::OfflineItem>;

    explicit CacheManager(DownloadBubbleUpdateService* update_service);
    ~CacheManager();

    CacheManager(const CacheManager&) = delete;
    CacheManager& operator=(const CacheManager&) = delete;

    // Whether the cache is at its max allowed capacity.
    bool IsDownloadItemCacheAtMax() const;
    bool IsOfflineItemCacheAtMax() const;

    // See comments on the public DownloadBubbleUpdateService method of the
    // same name.
    bool GetAllModelsToDisplay(
        std::vector<DownloadUIModel::DownloadUIModelPtr>& models,
        bool force_backfill_download_items = false);
    // Implements the above.
    void GetDownloadItemModelToDisplayOrPrune(
        base::Time cutoff_time,
        std::vector<DownloadUIModel::DownloadUIModelPtr>& models,
        bool& download_item_pruned,
        SortedDownloadItems::iterator& download_item_it);
    void GetOfflineItemModelToDisplayOrPrune(
        base::Time cutoff_time,
        std::vector<DownloadUIModel::DownloadUIModelPtr>& models,
        bool& offline_item_pruned,
        SortedOfflineItems::iterator& offline_item_it);

    // See comments on the public DownloadBubbleUpdateService method of the
    // same name.
    const DownloadBubbleDisplayInfo& GetDisplayInfo() const;

    // See comments on the public DownloadBubbleUpdateService method of the
    // same name.
    DownloadDisplay::ProgressInfo GetProgressInfo() const;

    // See comments on the public DownloadBubbleUpdateService method of the
    // same name.
    std::vector<std::u16string> TakeAccessibleAlertsForAnnouncement();

    // Adds an item to the cache if it is recent enough and meets other criteria
    // for showing in the bubble. If adding an item makes the map size exceed
    // the maximum, this removes excess items from the end of the map. Returns
    // whether the item was stored as the last item in the map. If |item| was
    // already in the cache, this does nothing. |is_new| is whether the item is
    // a newly added item (as opposed to an updated one). May mark the item
    // model as not-actioned-on if the item is new. May add an accessible alert
    // if `maybe_add_alert` is true.
    bool MaybeAddDownloadItemToCache(download::DownloadItem* item,
                                     bool is_new,
                                     bool maybe_add_alert);
    bool MaybeAddOfflineItemToCache(
        const offline_items_collection::OfflineItem& item,
        bool is_new,
        bool maybe_add_alert);

    // Updates an item by removing it and reinserting it in the cache. May
    // kick off backfilling of the cache.
    void OnDownloadItemUpdated(download::DownloadItem* item);
    void OnOfflineItemUpdated(
        const offline_items_collection::OfflineItem& item);

    // Removes an item from the cache. May kick off backfilling of the cache.
    void OnDownloadItemRemoved(download::DownloadItem* item);
    void OnOfflineItemRemoved(const offline_items_collection::ContentId& id);

    // Removes an item from the maps. Note: If the cache size was already at the
    // limit, and removing an item brings it under that limit, we must then get
    // all items in order to backfill the newly created space. (See
    // Backfill*Items() below.) Returns whether item was removed. If |item| is
    // not already in the cache, this does nothing.
    bool RemoveDownloadItemFromCache(download::DownloadItem* item);
    bool RemoveOfflineItemFromCache(
        const offline_items_collection::ContentId& id);

    // Updates |display_info_| based on the current contents of the cache.
    // This is kept updated as items are added or removed from the cache.
    // `updating_for_item` is the id of the item (download or offline item)
    // whose update triggered calling this function.
    void UpdateDisplayInfo(const std::string& updating_for_item);
    void UpdateDisplayInfo(
        const offline_items_collection::ContentId& updating_for_item);
    // Implements the above.
    void UpdateDisplayInfoForDownloadItem(
        base::optional_ref<const std::string> updating_for_item,
        base::Time cutoff_time,
        DownloadBubbleDisplayInfo& info,
        SortedDownloadItems::iterator& download_item_it);
    void UpdateDisplayInfoForOfflineItem(
        base::optional_ref<const offline_items_collection::ContentId>
            updating_for_item,
        base::Time cutoff_time,
        DownloadBubbleDisplayInfo& info,
        SortedOfflineItems::iterator& offline_item_it);
    bool ShouldStopUpdatingDisplayInfo(const DownloadBubbleDisplayInfo& info);

    // Clears the cache.
    void DropAllDownloadItems();
    void DropAllOfflineItems();

   private:
    // Forwards to |update_service_|.
    size_t GetMaxNumItemsToShow() const;
    size_t GetNumItemsToCache() const;

    // Implements the loop that iterates over the download item and offline item
    // caches and merges them, running `download_item_action` if we take a
    // download item and running `offline_item_action` if we take an offline
    // action (these should also advance the corresponding iterator). Iterates
    // until `should_stop` returns true or all items have been processed.
    void IterateOverMergedCaches(
        base::RepeatingCallback<void(SortedDownloadItems::iterator&)>
            download_item_action,
        base::RepeatingCallback<void(SortedOfflineItems::iterator&)>
            offline_item_action,
        base::RepeatingCallback<bool()> should_stop);

    template <typename Id, typename Item>
    bool AddItemToCacheImpl(Item item,
                            SortedItems<Item>& cache,
                            IterMap<Id, Item>& iter_map);

    template <typename Id, typename Item>
    bool RemoveItemFromCacheImpl(const Id& id,
                                 SortedItems<Item>& cache,
                                 IterMap<Id, Item>& iter_map);

    // Removes item if we already have the iterator to it. Returns next
    // iterator.
    template <typename Id, typename Item>
    typename SortedItems<Item>::iterator RemoveItemFromCacheByIter(
        typename SortedItems<Item>::iterator iter,
        SortedItems<Item>& cache,
        IterMap<Id, Item>& iter_map);

    // Wraps an item into a DownloadUIModel and possibly adds it to |models|, if
    // it is new enough (newer than |cutoff_time|) and meets other criteria.
    // Returns whether model was eligible to be added, regardless of whether it
    // was added (it may not have been added if |models| was at the size limit).
    bool MaybeAddDownloadItemModel(
        download::DownloadItem* item,
        base::Time cutoff_time,
        std::vector<DownloadUIModel::DownloadUIModelPtr>& models);
    bool MaybeAddOfflineItemModel(
        const offline_items_collection::OfflineItem& item,
        base::Time cutoff_time,
        std::vector<DownloadUIModel::DownloadUIModelPtr>& models);

    // Append newly backfilled download items to |models|. |last_key| is the
    // last key that was processed before backfilling. May prune any expired
    // items (i.e. items older than |cutoff_time|).
    void AppendBackfilledDownloadItems(
        const ItemSortKey& last_key,
        base::Time cutoff_time,
        std::vector<DownloadUIModel::DownloadUIModelPtr>& models);

#if DCHECK_IS_ON()
    // Checks that the cache data structures are consistent.
    void ConsistencyCheckCaches() const;

    template <typename Id, typename Item>
    void ConsistencyCheckImpl(const SortedItems<Item>& cache,
                              const IterMap<Id, Item>& iter_map) const;
#endif  // DCHECK_IS_ON()

    // DownloadBubbleUpdateService that owns this. Never null.
    const raw_ptr<DownloadBubbleUpdateService> update_service_ = nullptr;

    // Caches the current most-relevant items in sorted order. Size of each map
    // will generally be limited to GetNumItemsToCache (except during addition
    // of an item). Note: These are multimaps because, in theory, multiple items
    // might have the same sort key. The cache manipulation logic in this class
    // accounts for this by assuming that, if there's not enough space for all
    // the items with the last key, then caching an arbitrary subset of them is
    // fine.
    SortedDownloadItems download_items_;
    SortedOfflineItems offline_items_;

    // Holds iterators pointing into the above two maps, allowing lookup of an
    // item by GUID or ContentId.
    DownloadItemIterMap download_items_iter_map_;
    OfflineItemIterMap offline_items_iter_map_;

    // Holds the latest info about all models, relevant to the display state of
    // the download toolbar icon.
    DownloadBubbleDisplayInfo display_info_;

    // Holds the latest batch of accessible alerts since the last update.
    DownloadBubbleAccessibleAlertsMap accessible_alerts_;
  };

 public:
  // Convenience typedefs for brevity in the implementation file.
  template <typename Id, typename Item>
  using IterMap = CacheManager::IterMap<Id, Item>;
  template <typename Item>
  using SortedItems = CacheManager::SortedItems<Item>;

  // Checks whether |cache| is the main cache, used for CHECKs to ensure that
  // offline items only go into the main cache.
  bool IsMainCache(const CacheManager& cache) const;

  // Called when a download with an ephemeral warning should disappear from the
  // download bubble. To enact the disappearance, the item is omitted when
  // calculating `all_models_info_` and GetAllModelsToDisplay(). The item
  // remains in the cache until pruned in GetAllModelsToDisplay(). This function
  // handles the other part of that, which is updating `all_models_info_` and
  // notifying the display controller (which then hides the toolbar button if no
  // other downloads are displayed).
  void OnEphemeralWarningExpired(const std::string& guid);

 private:
  // Finds the appropriate CacheManager for a web app, creating one if it
  // doesn't exist.
  CacheManager& GetCacheForWebApp(const webapps::AppId& app_id);
  // As above, but does not create one if it doesn't exist (in which case it
  // returns nullptr).
  const CacheManager* GetExistingCacheForWebApp(
      const webapps::AppId& app_id) const;
  CacheManager* GetExistingCacheForWebApp(const webapps::AppId& app_id);

  // Finds the appropriate CacheManager for a download item, creating one if it
  // doesn't exist.
  CacheManager& GetCacheForItem(download::DownloadItem* item);

  // Returns pointers to all CacheManagers this object owns, in no particular
  // order.
  std::vector<CacheManager*> GetAllCacheManagers();

  // Populate the cache from items fetched from the download manager or
  // offline content manager.
  void InitializeDownloadItemsCache();
  void StartInitializeOfflineItemsCache();
  void InitializeOfflineItemsCache(
      const std::vector<offline_items_collection::OfflineItem>& all_items);

  // Gets download items from profile and original profile.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>>
  GetAllDownloadItems();

  // Called when a crx download has waited out its 2 second delay. Adds the
  // item to the cache if it's not already done, and notifies window-level
  // controllers.
  void OnDelayedCrxDownloadCreated(const std::string& guid);

  // Profile corresponding to this object.
  const raw_ptr<Profile> profile_ = nullptr;
  // Null if the profile is not OTR.
  const raw_ptr<Profile> original_profile_ = nullptr;

  // Override for the number of combined items to return.
  std::optional<size_t> max_num_items_to_show_for_testing_;
  // Override for the number of extra items to cache.
  std::optional<size_t> extra_items_to_cache_for_testing_;

  // Notifier for the current profile's DownloadManager. Null until initialized
  // in Initialize().
  std::unique_ptr<download::AllDownloadItemNotifier> download_item_notifier_;
  // Null if the profile is not OTR. Null until the original profile initiates
  // a download. If the profile is OTR, this holds a notifier for the original
  // profile.
  std::unique_ptr<download::AllDownloadItemNotifier>
      original_download_item_notifier_;

  bool offline_items_initialized_ = false;
  // Holds functions queued up while offline items were being initialized.
  std::vector<base::OnceClosure> offline_item_callbacks_;

  // Whether Shutdown() has been called, or the download manager or offline
  // content provider have been shut down.
  bool is_shut_down_ = false;

  // Set of GUIDs for extension/theme (crx) downloads that are pending notifying
  // the UI. GUIDs are added here when the download begins, and are removed
  // when the 2 second delay is up.
  std::set<std::string> delayed_crx_guids_;

  // The cache for all regular Chrome windows. Note that offline items all go
  // here, whereas download items may end up in other CacheManagers depending
  // on whether they were downloaded by a web app.
  CacheManager main_cache_;

  // A separate cache for each web app.
  std::map<webapps::AppId, CacheManager> web_app_caches_;

  // Observes download history so we can keep track of when updates from history
  // occur, to ignore them for the purposes of adding accessible alerts.
  // Note: Incognito profiles do not have download histories. For incognito
  // profiles, this observation corresponds to the original profile, because
  // downloads for the original profile show up in the incognito window download
  // bubble. For normal profiles, it is for the profile itself.
  // Note: No such observer exists for OfflineItems.
  bool download_history_loaded_ = false;
  base::ScopedObservation<DownloadHistory, DownloadHistory::Observer>
      download_history_observation_{this};

  // Observes the offline content provider.
  base::ScopedObservation<
      offline_items_collection::OfflineContentProvider,
      offline_items_collection::OfflineContentProvider::Observer>
      offline_content_provider_observation_{this};

  base::WeakPtrFactory<DownloadBubbleUpdateService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_UPDATE_SERVICE_H_
