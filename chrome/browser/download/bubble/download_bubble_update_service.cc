// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_update_service.h"

#include <iterator>
#include <optional>
#include <tuple>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/content_index/content_index_provider_impl.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/bubble/download_bubble_update_service_factory.h"
#include "chrome/browser/download/bubble/download_bubble_utils.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_item_web_app_data.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/download/offline_item_model_manager.h"
#include "chrome/browser/download/offline_item_model_manager_factory.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "components/download/public/common/download_item.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "content/public/browser/download_manager.h"

namespace {

using ::offline_items_collection::ContentId;
using ::offline_items_collection::OfflineContentProvider;
using ::offline_items_collection::OfflineItem;
using ::offline_items_collection::OfflineItemState;
using DownloadState = download::DownloadItem::DownloadState;
using DownloadUIModelPtr = DownloadUIModel::DownloadUIModelPtr;
using ItemSortKey = DownloadBubbleUpdateService::ItemSortKey;
template <typename Id, typename Item>
using IterMap = DownloadBubbleUpdateService::IterMap<Id, Item>;
using ProgressInfo = DownloadDisplay::ProgressInfo;
template <typename Item>
using SortedItems = DownloadBubbleUpdateService::SortedItems<Item>;

// Show up to 30 items in total by default.
constexpr size_t kDefaultMaxNumItemsToShow = 30u;
// Cache a few more items of each type than we will return from
// GetAllModelsToDisplay. This gives us some wiggle room and makes it more
// likely that we'll return enough items before backfilling.
constexpr size_t kDefaultExtraItemsToCache = 30u;
// Amount of time to show an item in the bubble. Items older than this duration
// ago will be pruned.
constexpr base::TimeDelta kShowItemInBubbleDuration = base::Hours(24);
// Don't send the "download started" notification for an extension or theme
// (crx) download until 2 seconds after it has begun. If it is a small download
// that finishes in under 2 seconds, the download UI does not show at all. If it
// is a large download that takes longer than 2 seconds, show the UI so that the
// user knows Chrome is working on it.
constexpr base::TimeDelta kCrxShowNewItemDelay = base::Seconds(2);
// Limit the size of the |delayed_crx_guids_| set so it doesn't grow
// unboundedly. It is unlikely that the user would have 20 active crx downloads
// simultaneously.
constexpr int kMaxDelayedCrxGuids = 20;

template <typename Item>
ItemSortKey::State GetState(const Item& item) {
  if (IsItemInProgress(item)) {
    return IsItemPaused(item) ? ItemSortKey::kInProgressPaused
                              : ItemSortKey::kInProgressActive;
  }
  return ItemSortKey::kNotInProgress;
}

template <typename Item>
ItemSortKey GetSortKey(const Item& item) {
  return ItemSortKey{GetState(item), GetItemStartTime(item)};
}

// Helper to get an iterator to the last element in the cache. The cache
// must not be empty.
template <typename Item>
SortedItems<Item>::const_iterator GetLastIter(const SortedItems<Item>& cache) {
  CHECK(!cache.empty());
  auto it = cache.end();
  return std::prev(it);
}

// Returns the earliest creation time for which we will show items in the
// bubble.
base::Time GetCutoffTime() {
  return base::Time::Now() - kShowItemInBubbleDuration;
}

// Updates the count of received vs total bytes. Returns whether progress is
// certain.
bool AddItemProgress(int64_t item_received_bytes,
                     int64_t item_total_bytes,
                     int& in_progress_items,
                     int64_t& received_bytes,
                     int64_t& total_bytes) {
  ++in_progress_items;
  if (item_total_bytes <= 0) {
    // Progress is uncertain: there may or may not be more data coming down this
    // pipe.
    return false;
  }
  received_bytes += item_received_bytes;
  total_bytes += item_total_bytes;
  return true;
}

bool ShouldIncludeModel(const DownloadUIModel* model, base::Time cutoff_time) {
  return DownloadUIModelIsRecent(model, cutoff_time) &&
         model->ShouldShowInBubble();
}

// Returns whether |model| was eligible to be added to |models|, regardless of
// whether it was actually added.
bool MaybeAddModel(DownloadUIModelPtr model,
                   base::Time cutoff_time,
                   size_t max_num_models,
                   std::vector<DownloadUIModelPtr>& models) {
  if (!ShouldIncludeModel(model.get(), cutoff_time)) {
    model->SetActionedOn(true);
    return false;
  }
  if (models.size() < max_num_models) {
    models.push_back(std::move(model));
  }
  return true;
}

// For GetAllModelsToDisplay()'s iteration over the merged caches, don't stop
// until all models have been processed.
bool NeverStop() {
  return false;
}

// `update_is_for_model` is whether the current call to this function was
// triggered on behalf of `model`.
void UpdateInfoForModel(const DownloadUIModel& model,
                        bool update_is_for_model,
                        base::Time cutoff_time,
                        DownloadBubbleDisplayInfo& info) {
  if (!ShouldIncludeModel(&model, cutoff_time)) {
    return;
  }
  ++info.all_models_size;
  if (model.GetDangerType() == download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING &&
      model.GetState() != download::DownloadItem::CANCELLED) {
    info.has_deep_scanning = true;
  }
  if (!model.WasActionedOn()) {
    info.has_unactioned = true;
  }
  if (IsModelInProgress(&model)) {
    ++info.in_progress_count;
    if (model.IsPaused()) {
      ++info.paused_count;
    }
  } else {
    base::Time cur_completed_time = model.GetEndTime();
    if (cur_completed_time.is_null() && update_is_for_model &&
        model.GetState() != download::DownloadItem::CANCELLED) {
      // Given that we consider dangerous/insecure downloads to be complete, the
      // completion time should reflect the time they were marked as
      // dangerous/insecure. Since download is still technically IN_PROGRESS in
      // this scenario and thus has a null end time, we just use the current
      // time based on the assumption that a download in a dangerous/insecure
      // state does not receive further updates besides cancellation.
      cur_completed_time = base::Time::Now();
    }
    info.last_completed_time =
        std::max(info.last_completed_time, cur_completed_time);
  }
}

bool BrowserMatchesWebAppData(const Browser* browser,
                              const DownloadItemWebAppData* data) {
  return data != nullptr
             ? web_app::AppBrowserController::IsForWebApp(browser, data->id())
             : !web_app::AppBrowserController::IsWebApp(browser);
}

}  // namespace

bool DownloadBubbleUpdateService::ItemSortKey::operator<(
    const DownloadBubbleUpdateService::ItemSortKey& other) const {
  if (state < other.state) {
    return true;
  } else if (state > other.state) {
    return false;
  }
  return start_time > other.start_time;
}

bool DownloadBubbleUpdateService::ItemSortKey::operator==(
    const DownloadBubbleUpdateService::ItemSortKey& other) const {
  return std::tie(state, start_time) == std::tie(other.state, other.start_time);
}

bool DownloadBubbleUpdateService::ItemSortKey::operator!=(
    const DownloadBubbleUpdateService::ItemSortKey& other) const {
  return !(*this == other);
}

bool DownloadBubbleUpdateService::ItemSortKey::operator>(
    const DownloadBubbleUpdateService::ItemSortKey& other) const {
  return !(*this == other || *this < other);
}

// static
DownloadBubbleUpdateService::ItemSortKey
DownloadBubbleUpdateService::ItemSortKey::Min() {
  return ItemSortKey{kInProgressActive, base::Time::Max()};
}

DownloadBubbleUpdateService::CacheManager::CacheManager(
    DownloadBubbleUpdateService* update_service)
    : update_service_(update_service) {
  CHECK(update_service);
}

DownloadBubbleUpdateService::CacheManager::~CacheManager() = default;

DownloadBubbleUpdateService::DownloadBubbleUpdateService(Profile* profile)
    : profile_(profile),
      original_profile_(IsProfileOtr() ? profile_->GetOriginalProfile()
                                       : nullptr),
      main_cache_(this) {
  offline_content_provider_observation_.Observe(
      OfflineContentAggregatorFactory::GetForKey(profile_->GetProfileKey()));
}

DownloadBubbleUpdateService::~DownloadBubbleUpdateService() = default;

void DownloadBubbleUpdateService::Shutdown() {
  offline_content_provider_observation_.Reset();
  is_shut_down_ = true;
}

bool DownloadBubbleUpdateService::IsShutDown() const {
  return is_shut_down_;
}

bool DownloadBubbleUpdateService::IsProfileOtr() const {
  return profile_->IsOffTheRecord();
}

size_t DownloadBubbleUpdateService::GetMaxNumItemsToShow() const {
  size_t max =
      max_num_items_to_show_for_testing_.value_or(kDefaultMaxNumItemsToShow);
  CHECK_GE(max, 2u);
  return max;
}

size_t DownloadBubbleUpdateService::GetNumItemsToCache() const {
  return GetMaxNumItemsToShow() +
         extra_items_to_cache_for_testing_.value_or(kDefaultExtraItemsToCache);
}

size_t DownloadBubbleUpdateService::CacheManager::GetMaxNumItemsToShow() const {
  return update_service_->GetMaxNumItemsToShow();
}

size_t DownloadBubbleUpdateService::CacheManager::GetNumItemsToCache() const {
  return update_service_->GetNumItemsToCache();
}

bool DownloadBubbleUpdateService::CacheManager::IsDownloadItemCacheAtMax()
    const {
  CHECK(download_items_.size() <= GetNumItemsToCache());
  return download_items_.size() == GetNumItemsToCache();
}

bool DownloadBubbleUpdateService::CacheManager::IsOfflineItemCacheAtMax()
    const {
  CHECK(offline_items_.size() <= GetNumItemsToCache());
  return offline_items_.size() == GetNumItemsToCache();
}

DownloadBubbleUpdateService::CacheManager&
DownloadBubbleUpdateService::GetCacheForWebApp(const webapps::AppId& app_id) {
  auto it = web_app_caches_.find(app_id);
  if (it == web_app_caches_.end()) {
    // Create a new CacheManager for this |app_id|.
    it = web_app_caches_.emplace(app_id, this).first;
  }
  return it->second;
}

const DownloadBubbleUpdateService::CacheManager*
DownloadBubbleUpdateService::GetExistingCacheForWebApp(
    const webapps::AppId& app_id) const {
  if (auto it = web_app_caches_.find(app_id); it != web_app_caches_.end()) {
    return &it->second;
  }
  return nullptr;
}

DownloadBubbleUpdateService::CacheManager*
DownloadBubbleUpdateService::GetExistingCacheForWebApp(
    const webapps::AppId& app_id) {
  return const_cast<CacheManager*>(
      std::as_const(*this).GetExistingCacheForWebApp(app_id));
}

DownloadBubbleUpdateService::CacheManager&
DownloadBubbleUpdateService::GetCacheForItem(download::DownloadItem* item) {
  auto* web_app_data = DownloadItemWebAppData::Get(item);
  if (web_app_data == nullptr) {
    return main_cache_;
  }
  return GetCacheForWebApp(web_app_data->id());
}

std::vector<DownloadBubbleUpdateService::CacheManager*>
DownloadBubbleUpdateService::GetAllCacheManagers() {
  std::vector<CacheManager*> cache_managers;
  cache_managers.push_back(&main_cache_);
  for (auto& [web_app_id, cache_manager] : web_app_caches_) {
    cache_managers.push_back(&cache_manager);
  }
  return cache_managers;
}

void DownloadBubbleUpdateService::ObserveDownloadHistory() {
  // If OTR, this is the original profile. Otherwise, this is just the profile
  // itself.
  Profile* profile = profile_->GetOriginalProfile();
  if (DownloadCoreService* dcs =
          DownloadCoreServiceFactory::GetForBrowserContext(profile);
      dcs && dcs->GetDownloadHistory()) {
    download_history_observation_.Observe(dcs->GetDownloadHistory());
  }
}

void DownloadBubbleUpdateService::Initialize(
    content::DownloadManager* manager) {
  CHECK(manager);
  CHECK(!download_item_notifier_);

  // This is safe to do here because we know the DownloadManager has been
  // created by now. If we did this earlier, then it might trigger early
  // initialization of the DownloadManager and ChromeDownloadManagerDelegate.
  ObserveDownloadHistory();

  // Assume we have an original profile and it has an OTR profile.
  // If the original profile's DownloadBubbleUpdateService is Initialize()'d
  // already when this function is invoked on the OTR profile's
  // DownloadBubbleUpdateService, we set the OTR profile's
  // DownloadBubbleUpdateService's |original_download_item_notifier_| in the
  // 'if' block below. If the original profile's DownloadBubbleUpdateService is
  // not yet initialized when this function is invoked on the OTR profile's
  // DownloadBubbleUpdateService, we will set the OTR profile's
  // DownloadBubbleUpdateService's |original_download_item_notifier_| when the
  // original profile's DownloadBubbleUpdateService does become Initialize()'d,
  // in the 'else' block below (which will trigger re-intialization of the
  // download item cache).
  if (profile_->IsOffTheRecord()) {
    DownloadBubbleUpdateService* original_update_service =
        DownloadBubbleUpdateServiceFactory::GetForProfile(original_profile_);
    content::DownloadManager* original_download_manager =
        original_update_service ? original_update_service->GetDownloadManager()
                                : nullptr;
    if (original_download_manager) {
      InitializeOriginalNotifier(original_download_manager);
    }
  } else {
    for (Profile* otr_profile : profile_->GetAllOffTheRecordProfiles()) {
      DownloadBubbleUpdateServiceFactory::GetForProfile(otr_profile)
          ->InitializeOriginalNotifier(manager);
    }
  }
  download_item_notifier_ =
      std::make_unique<download::AllDownloadItemNotifier>(manager, this);
  // As long as we have a notifier for this profile, we can initialize the cache
  // with the current profile's downloads. If we get an original manager in the
  // future, we will initialize from scratch at that time.
  InitializeDownloadItemsCache();
  StartInitializeOfflineItemsCache();
}

void DownloadBubbleUpdateService::InitializeOriginalNotifier(
    content::DownloadManager* manager) {
  CHECK(profile_->IsOffTheRecord());
  CHECK(manager);
  if (original_download_item_notifier_) {
    return;
  }
  original_download_item_notifier_ =
      std::make_unique<download::AllDownloadItemNotifier>(manager, this);
  // Reset the download items cache, now that we have an original
  // DownloadManager to pull from.
  if (download_item_notifier_) {
    InitializeDownloadItemsCache();
  }
}

content::DownloadManager* DownloadBubbleUpdateService::GetDownloadManager() {
  return download_item_notifier_ ? download_item_notifier_->GetManager()
                                 : nullptr;
}

bool DownloadBubbleUpdateService::IsInitialized() const {
  return download_item_notifier_ && offline_items_initialized_;
}

bool DownloadBubbleUpdateService::CacheManager::GetAllModelsToDisplay(
    std::vector<DownloadUIModelPtr>& models,
    bool force_backfill_download_items) {
#if DCHECK_IS_ON()
  ConsistencyCheckCaches();
#endif  // DCHECK_IS_ON()

  base::Time cutoff_time = GetCutoffTime();
  models.clear();
  // If the caches are at max capacity, and we prune some items that are too
  // old, we may need to backfill items.
  bool download_items_cache_was_at_max = IsDownloadItemCacheAtMax();
  bool offline_items_cache_was_at_max = IsOfflineItemCacheAtMax();
  bool download_item_pruned = false;
  bool offline_item_pruned = false;

  // Merge the two sorted lists, while pruning the expired items. Since the
  // criteria for pruning requires the model, to avoid unnecessary creation and
  // destruction of models, we collect the models to return and prune items in
  // the same loop iteration.
  IterateOverMergedCaches(
      base::BindRepeating(&DownloadBubbleUpdateService::CacheManager::
                              GetDownloadItemModelToDisplayOrPrune,
                          base::Unretained(this), cutoff_time, std::ref(models),
                          std::ref(download_item_pruned)),
      base::BindRepeating(&DownloadBubbleUpdateService::CacheManager::
                              GetOfflineItemModelToDisplayOrPrune,
                          base::Unretained(this), cutoff_time, std::ref(models),
                          std::ref(offline_item_pruned)),
      base::BindRepeating(&NeverStop));

  CHECK_LE(models.size(), GetMaxNumItemsToShow());

  bool download_items_need_backfill =
      download_item_pruned && download_items_cache_was_at_max;
  bool offline_items_need_backfill =
      offline_item_pruned && offline_items_cache_was_at_max;

  if (download_items_need_backfill) {
    // A key that will sort before any other key.
    ItemSortKey last_download_item_key = ItemSortKey::Min();
    if (!download_items_.empty()) {
      last_download_item_key = GetLastIter(download_items_)->first;
    }
    if (force_backfill_download_items) {
      // Get more items synchronously.
      update_service_->BackfillDownloadItems(last_download_item_key);
      AppendBackfilledDownloadItems(last_download_item_key, cutoff_time,
                                    models);
      download_items_need_backfill = false;
    } else {
      update_service_->StartBackfillDownloadItems(last_download_item_key);
    }
  }

  if (offline_items_need_backfill) {
    // A key that will sort before any other key.
    ItemSortKey last_offline_item_key = ItemSortKey::Min();
    if (!offline_items_.empty()) {
      last_offline_item_key = GetLastIter(offline_items_)->first;
    }
    update_service_->StartBackfillOfflineItems(last_offline_item_key);
  }

#if DCHECK_IS_ON()
  ConsistencyCheckCaches();
#endif  // DCHECK_IS_ON()
  return models.size() == GetMaxNumItemsToShow() ||
         !(download_items_need_backfill || offline_items_need_backfill);
}

void DownloadBubbleUpdateService::CacheManager::
    GetDownloadItemModelToDisplayOrPrune(
        base::Time cutoff_time,
        std::vector<DownloadUIModel::DownloadUIModelPtr>& models,
        bool& download_item_pruned,
        SortedDownloadItems::iterator& download_item_it) {
  if (!MaybeAddDownloadItemModel(download_item_it->second, cutoff_time,
                                 models)) {
    download_item_it = RemoveItemFromCacheByIter(
        download_item_it, download_items_, download_items_iter_map_);
    download_item_pruned = true;
  } else {
    ++download_item_it;
  }
}

void DownloadBubbleUpdateService::CacheManager::
    GetOfflineItemModelToDisplayOrPrune(
        base::Time cutoff_time,
        std::vector<DownloadUIModel::DownloadUIModelPtr>& models,
        bool& offline_item_pruned,
        SortedOfflineItems::iterator& offline_item_it) {
  if (!MaybeAddOfflineItemModel(offline_item_it->second, cutoff_time, models)) {
    offline_item_it = RemoveItemFromCacheByIter(offline_item_it, offline_items_,
                                                offline_items_iter_map_);
    offline_item_pruned = true;
  } else {
    ++offline_item_it;
  }
}

bool DownloadBubbleUpdateService::GetAllModelsToDisplay(
    std::vector<DownloadUIModelPtr>& models,
    const webapps::AppId* web_app_id,
    bool force_backfill_download_items) {
  if (web_app_id == nullptr) {
    return main_cache_.GetAllModelsToDisplay(models,
                                             force_backfill_download_items);
  }
  return GetCacheForWebApp(*web_app_id)
      .GetAllModelsToDisplay(models, force_backfill_download_items);
}

const DownloadBubbleDisplayInfo&
DownloadBubbleUpdateService::CacheManager::GetDisplayInfo() const {
  return display_info_;
}

const DownloadBubbleDisplayInfo& DownloadBubbleUpdateService::GetDisplayInfo(
    const webapps::AppId* web_app_id) {
  if (web_app_id == nullptr) {
    return main_cache_.GetDisplayInfo();
  }
  if (const CacheManager* cache = GetExistingCacheForWebApp(*web_app_id);
      cache != nullptr) {
    return cache->GetDisplayInfo();
  }
  return DownloadBubbleDisplayInfo::EmptyInfo();
}

void DownloadBubbleUpdateService::CacheManager::UpdateDisplayInfo(
    const std::string& updating_for_item) {
#if DCHECK_IS_ON()
  ConsistencyCheckCaches();
#endif  // DCHECK_IS_ON()

  // A new info is constructed from scratch based on the current cache contents.
  DownloadBubbleDisplayInfo info;
  base::Time cutoff_time = GetCutoffTime();

  // Iterate over the two sorted caches (download items and offline items) in
  // combined/merged sorted order. This is done in the same way as in
  // GetAllItemsToDisplay() to ensure that the info most accurately represents
  // the list of items that would be returned from that method.
  IterateOverMergedCaches(
      base::BindRepeating(
          &DownloadBubbleUpdateService::CacheManager::
              UpdateDisplayInfoForDownloadItem,
          base::Unretained(this),
          base::optional_ref<const std::string>(updating_for_item), cutoff_time,
          std::ref(info)),
      base::BindRepeating(&DownloadBubbleUpdateService::CacheManager::
                              UpdateDisplayInfoForOfflineItem,
                          base::Unretained(this), std::nullopt, cutoff_time,
                          std::ref(info)),
      base::BindRepeating(&DownloadBubbleUpdateService::CacheManager::
                              ShouldStopUpdatingDisplayInfo,
                          base::Unretained(this), std::ref(info)));

  display_info_ = info;
}

void DownloadBubbleUpdateService::CacheManager::UpdateDisplayInfo(
    const ContentId& updating_for_item) {
#if DCHECK_IS_ON()
  ConsistencyCheckCaches();
#endif  // DCHECK_IS_ON()

  // A new info is constructed from scratch based on the current cache contents.
  DownloadBubbleDisplayInfo info;
  base::Time cutoff_time = GetCutoffTime();

  // Iterate over the two sorted caches (download items and offline items) in
  // combined/merged sorted order. This is done in the same way as in
  // GetAllItemsToDisplay() to ensure that the info most accurately represents
  // the list of items that would be returned from that method.
  IterateOverMergedCaches(
      base::BindRepeating(&DownloadBubbleUpdateService::CacheManager::
                              UpdateDisplayInfoForDownloadItem,
                          base::Unretained(this), std::nullopt, cutoff_time,
                          std::ref(info)),
      base::BindRepeating(
          &DownloadBubbleUpdateService::CacheManager::
              UpdateDisplayInfoForOfflineItem,
          base::Unretained(this),
          base::optional_ref<const ContentId>(updating_for_item), cutoff_time,
          std::ref(info)),
      base::BindRepeating(&DownloadBubbleUpdateService::CacheManager::
                              ShouldStopUpdatingDisplayInfo,
                          base::Unretained(this), std::ref(info)));

  display_info_ = info;
}

void DownloadBubbleUpdateService::CacheManager::
    UpdateDisplayInfoForDownloadItem(
        base::optional_ref<const std::string> updating_for_item,
        base::Time cutoff_time,
        DownloadBubbleDisplayInfo& info,
        SortedDownloadItems::iterator& download_item_it) {
  DownloadItemModel model(
      download_item_it->second,
      std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>());
  bool update_is_for_model =
      updating_for_item.has_value() &&
      *updating_for_item == GetItemId(download_item_it->second);
  UpdateInfoForModel(model, update_is_for_model, cutoff_time, info);
  ++download_item_it;
}

void DownloadBubbleUpdateService::CacheManager::UpdateDisplayInfoForOfflineItem(
    base::optional_ref<const ContentId> updating_for_item,
    base::Time cutoff_time,
    DownloadBubbleDisplayInfo& info,
    SortedOfflineItems::iterator& offline_item_it) {
  OfflineItemModel model(
      update_service_->GetOfflineManager(), offline_item_it->second,
      std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>());
  bool update_is_for_model =
      updating_for_item.has_value() &&
      *updating_for_item == GetItemId(offline_item_it->second);
  UpdateInfoForModel(model, update_is_for_model, cutoff_time, info);
  ++offline_item_it;
}

bool DownloadBubbleUpdateService::CacheManager::ShouldStopUpdatingDisplayInfo(
    const DownloadBubbleDisplayInfo& info) {
  return info.all_models_size >= GetMaxNumItemsToShow();
}

void DownloadBubbleUpdateService::CacheManager::IterateOverMergedCaches(
    base::RepeatingCallback<void(SortedDownloadItems::iterator&)>
        download_item_action,
    base::RepeatingCallback<void(SortedOfflineItems::iterator&)>
        offline_item_action,
    base::RepeatingCallback<bool()> should_stop) {
  auto download_item_it = download_items_.begin();
  auto offline_item_it = offline_items_.begin();
  while (download_item_it != download_items_.end() ||
         offline_item_it != offline_items_.end()) {
    // If the current download item sorts before the current offline item (or we
    // are out of offline items), take the download item.
    if (download_item_it != download_items_.end() &&
        (offline_item_it == offline_items_.end() ||
         download_item_it->first < offline_item_it->first)) {
      download_item_action.Run(download_item_it);
    } else {
      // Else, the current offline item sorts before the current download item
      // (or we are out of download items), so take the offline item.
      offline_item_action.Run(offline_item_it);
    }
    if (should_stop.Run()) {
      break;
    }
  }
}

ProgressInfo DownloadBubbleUpdateService::CacheManager::GetProgressInfo()
    const {
#if DCHECK_IS_ON()
  ConsistencyCheckCaches();
#endif  // DCHECK_IS_ON()

  ProgressInfo info;
  int in_progress_items = 0;
  int64_t received_bytes = 0;
  int64_t total_bytes = 0;

  base::Time cutoff_time = GetCutoffTime();

  for (const auto& [key, item] : download_items_) {
    if (key.state == ItemSortKey::kNotInProgress) {
      break;
    }
    if (GetItemStartTime(item) < cutoff_time) {
      continue;
    }
    // Note that operator&= does not short-circuit.
    info.progress_certain &=
        AddItemProgress(item->GetReceivedBytes(), item->GetTotalBytes(),
                        in_progress_items, received_bytes, total_bytes);
  }

  for (const auto& [key, item] : offline_items_) {
    if (key.state == ItemSortKey::kNotInProgress) {
      break;
    }
    if (GetItemStartTime(item) < cutoff_time) {
      continue;
    }
    // Note that operator&= does not short-circuit.
    info.progress_certain &=
        AddItemProgress(item.received_bytes, item.total_size_bytes,
                        in_progress_items, received_bytes, total_bytes);
  }

  info.download_count = in_progress_items;
  if (total_bytes > 0) {
    info.progress_percentage =
        base::ClampFloor(received_bytes * 100.0 / total_bytes);
  }
  return info;
}

ProgressInfo DownloadBubbleUpdateService::GetProgressInfo(
    const webapps::AppId* web_app_id) const {
  if (web_app_id == nullptr) {
    return main_cache_.GetProgressInfo();
  }
  if (const CacheManager* cache = GetExistingCacheForWebApp(*web_app_id);
      cache != nullptr) {
    return cache->GetProgressInfo();
  }
  return ProgressInfo{};
}

std::vector<std::u16string> DownloadBubbleUpdateService::CacheManager::
    TakeAccessibleAlertsForAnnouncement() {
  std::vector<std::u16string> to_announce =
      accessible_alerts_.TakeAlertsForAnnouncement();
  accessible_alerts_.GarbageCollect();
  return to_announce;
}

std::vector<std::u16string>
DownloadBubbleUpdateService::TakeAccessibleAlertsForAnnouncement(
    const webapps::AppId* web_app_id) {
  if (web_app_id == nullptr) {
    return main_cache_.TakeAccessibleAlertsForAnnouncement();
  }
  if (CacheManager* cache = GetExistingCacheForWebApp(*web_app_id);
      cache != nullptr) {
    return cache->TakeAccessibleAlertsForAnnouncement();
  }
  return std::vector<std::u16string>();
}

void DownloadBubbleUpdateService::OnDownloadCreated(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  if (IsShutDown()) {
    return;
  }
  CHECK(download_item_notifier_ || original_download_item_notifier_);
  if (!download_item_notifier_) {
    return;
  }
  if (download_crx_util::IsExtensionDownload(*item) &&
      delayed_crx_guids_.size() < kMaxDelayedCrxGuids) {
    const std::string& guid = item->GetGuid();
    CHECK(!delayed_crx_guids_.contains(guid));
    delayed_crx_guids_.insert(guid);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &DownloadBubbleUpdateService::OnDelayedCrxDownloadCreated,
            weak_factory_.GetWeakPtr(), guid),
        kCrxShowNewItemDelay);
    return;
  }
  GetCacheForItem(item).MaybeAddDownloadItemToCache(
      item, /*is_new=*/true,
      /*maybe_add_alert=*/download_history_loaded_);
  // NotifyWindowsOfDownloadItemAdded() is called from
  // DownloadBubbleUIControllerDelegate for new non-crx downloads.
}

void DownloadBubbleUpdateService::OnDelayedCrxDownloadCreated(
    const std::string& guid) {
  if (IsShutDown()) {
    return;
  }
  CHECK(download_item_notifier_ || original_download_item_notifier_);
  if (!download_item_notifier_) {
    return;
  }
  // This assumes that for extension/theme downloads, the DownloadItem is
  // removed from the DownloadManager upon completion.
  download::DownloadItem* item =
      download_item_notifier_->GetManager()->GetDownloadByGuid(guid);
  if (item && !item->IsDone()) {
    GetCacheForItem(item).MaybeAddDownloadItemToCache(
        item, /*is_new=*/true,
        /*maybe_add_alert=*/download_history_loaded_);
    NotifyWindowsOfDownloadItemAdded(item);
  }
  size_t erased = delayed_crx_guids_.erase(guid);
  CHECK_EQ(erased, 1u);
}

void DownloadBubbleUpdateService::NotifyWindowsOfDownloadItemAdded(
    download::DownloadItem* item) {
  Browser* browser_to_show_animation =
      FindBrowserToShowAnimation(item, profile_);
  auto* web_app_data = DownloadItemWebAppData::Get(item);
  for (Browser* browser : chrome::FindAllBrowsersWithProfile(profile_)) {
    if (browser->window() &&
        browser->window()->GetDownloadBubbleUIController() &&
        BrowserMatchesWebAppData(browser, web_app_data)) {
      browser->window()->GetDownloadBubbleUIController()->OnDownloadItemAdded(
          item, /*may_show_animation=*/browser == browser_to_show_animation);
    }
  }
}

void DownloadBubbleUpdateService::OnDownloadUpdated(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  if (IsShutDown()) {
    return;
  }
  CHECK(download_item_notifier_ || original_download_item_notifier_);
  if (!download_item_notifier_) {
    return;
  }
  // If the item is an extension or theme download waiting out its 2-second
  // delay, don't show a UI update for it.
  if (delayed_crx_guids_.contains(item->GetGuid())) {
    return;
  }
  CacheManager& cache = GetCacheForItem(item);
  // When persisted web app download items are restored from the history
  // database, we first get a OnDownloadCreated() notification about the item
  // without its DownloadItemWebAppData, causing the item to go into the main
  // cache, followed by an OnDownloadUpdated() notification after the
  // DownloadItemWebAppData is added. In order to keep the item in the
  // appropriate cache for the web app, and NOT in the main cache, we must
  // remove it from the main cache explicitly here. Note this assumes that an
  // item's associated web app id never changes once it is tagged.
  if (!IsMainCache(cache)) {
    main_cache_.OnDownloadItemRemoved(item);
  }
  cache.OnDownloadItemUpdated(item);

  auto* web_app_data = DownloadItemWebAppData::Get(item);
  for (Browser* browser : chrome::FindAllBrowsersWithProfile(profile_)) {
    if (browser->window() &&
        browser->window()->GetDownloadBubbleUIController() &&
        BrowserMatchesWebAppData(browser, web_app_data)) {
      browser->window()->GetDownloadBubbleUIController()->OnDownloadItemUpdated(
          item);
    }
  }
}

void DownloadBubbleUpdateService::CacheManager::OnDownloadItemUpdated(
    download::DownloadItem* item) {
  bool cache_was_at_max = IsDownloadItemCacheAtMax();
  bool removed_item = RemoveDownloadItemFromCache(item);
  bool added_back_at_end = MaybeAddDownloadItemToCache(
      item, /*is_new=*/false,
      /*maybe_add_alert=*/update_service_->download_history_loaded());
  if (cache_was_at_max && removed_item && added_back_at_end) {
    CHECK_EQ(download_items_.size(), GetNumItemsToCache());
    const ItemSortKey& last_key =
        std::prev(GetLastIter(download_items_))->first;
    update_service_->StartBackfillDownloadItems(last_key);
  }
}

void DownloadBubbleUpdateService::OnDownloadRemoved(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  if (IsShutDown()) {
    return;
  }
  CHECK(download_item_notifier_ || original_download_item_notifier_);
  if (!download_item_notifier_) {
    return;
  }
  GetCacheForItem(item).OnDownloadItemRemoved(item);

  auto* web_app_data = DownloadItemWebAppData::Get(item);
  for (Browser* browser : chrome::FindAllBrowsersWithProfile(profile_)) {
    if (browser->window() &&
        browser->window()->GetDownloadBubbleUIController() &&
        BrowserMatchesWebAppData(browser, web_app_data)) {
      browser->window()->GetDownloadBubbleUIController()->OnDownloadItemRemoved(
          item);
    }
  }
}

void DownloadBubbleUpdateService::CacheManager::OnDownloadItemRemoved(
    download::DownloadItem* item) {
  bool cache_was_at_max = IsDownloadItemCacheAtMax();
  if (RemoveDownloadItemFromCache(item) && cache_was_at_max) {
    CHECK_EQ(download_items_.size(), GetNumItemsToCache() - 1);
    const ItemSortKey& last_key = GetLastIter(download_items_)->first;
    update_service_->StartBackfillDownloadItems(last_key);
  }
}

void DownloadBubbleUpdateService::OnManagerGoingDown(
    content::DownloadManager* manager) {
  CHECK(download_item_notifier_ || original_download_item_notifier_);
  // Assume that the original manager (if this is an OTR profile) may or may not
  // have shut down, but we still want to cease all operations when this
  // profile's manager shuts down.
  if (download_item_notifier_ &&
      (manager == download_item_notifier_->GetManager())) {
    is_shut_down_ = true;
    download_item_notifier_.reset();
    for (CacheManager* cache : GetAllCacheManagers()) {
      cache->DropAllDownloadItems();
    }
  }
}

void DownloadBubbleUpdateService::OnItemsAdded(
    const OfflineContentProvider::OfflineItemList& items) {
  if (IsShutDown()) {
    return;
  }
  if (!offline_items_initialized_) {
    offline_item_callbacks_.push_back(
        base::BindOnce(&DownloadBubbleUpdateService::OnItemsAdded,
                       weak_factory_.GetWeakPtr(), items));
    return;
  }
  for (const OfflineItem& item : items) {
    main_cache_.MaybeAddOfflineItemToCache(item, /*is_new=*/true,
                                           /*maybe_add_alert=*/true);
  }

  for (Browser* browser : chrome::FindAllBrowsersWithProfile(profile_)) {
    if (browser->window() &&
        browser->window()->GetDownloadBubbleUIController() &&
        !web_app::AppBrowserController::IsWebApp(browser)) {
      browser->window()->GetDownloadBubbleUIController()->OnOfflineItemsAdded(
          items);
    }
  }
}

void DownloadBubbleUpdateService::OnItemRemoved(const ContentId& id) {
  if (IsShutDown()) {
    return;
  }
  if (!offline_items_initialized_) {
    offline_item_callbacks_.push_back(
        base::BindOnce(&DownloadBubbleUpdateService::OnItemRemoved,
                       weak_factory_.GetWeakPtr(), id));
    return;
  }
  main_cache_.OnOfflineItemRemoved(id);

  for (Browser* browser : chrome::FindAllBrowsersWithProfile(profile_)) {
    if (browser->window() &&
        browser->window()->GetDownloadBubbleUIController() &&
        !web_app::AppBrowserController::IsWebApp(browser)) {
      browser->window()->GetDownloadBubbleUIController()->OnOfflineItemRemoved(
          id);
    }
  }
}

void DownloadBubbleUpdateService::CacheManager::OnOfflineItemRemoved(
    const ContentId& id) {
  bool cache_was_at_max = IsOfflineItemCacheAtMax();
  if (RemoveOfflineItemFromCache(id) && cache_was_at_max) {
    CHECK_EQ(offline_items_.size(), GetNumItemsToCache() - 1);
    const ItemSortKey& last_key = GetLastIter(offline_items_)->first;
    update_service_->StartBackfillOfflineItems(last_key);
  }
}

void DownloadBubbleUpdateService::OnItemUpdated(
    const OfflineItem& item,
    const std::optional<offline_items_collection::UpdateDelta>& update_delta) {
  if (IsShutDown()) {
    return;
  }
  if (!offline_items_initialized_) {
    offline_item_callbacks_.push_back(
        base::BindOnce(&DownloadBubbleUpdateService::OnItemUpdated,
                       weak_factory_.GetWeakPtr(), item, update_delta));
    return;
  }
  main_cache_.OnOfflineItemUpdated(item);

  for (Browser* browser : chrome::FindAllBrowsersWithProfile(profile_)) {
    if (browser->window() &&
        browser->window()->GetDownloadBubbleUIController() &&
        !web_app::AppBrowserController::IsWebApp(browser)) {
      browser->window()->GetDownloadBubbleUIController()->OnOfflineItemUpdated(
          item);
    }
  }
}

void DownloadBubbleUpdateService::CacheManager::OnOfflineItemUpdated(
    const OfflineItem& item) {
  bool cache_was_at_max = IsOfflineItemCacheAtMax();
  bool removed_item = RemoveOfflineItemFromCache(GetItemId(item));
  bool added_back_to_end = MaybeAddOfflineItemToCache(item, /*is_new=*/false,
                                                      /*maybe_add_alert=*/true);
  if (cache_was_at_max && removed_item && added_back_to_end) {
    CHECK_EQ(offline_items_.size(), GetNumItemsToCache());
    const ItemSortKey& last_key = std::prev(GetLastIter(offline_items_))->first;
    update_service_->StartBackfillOfflineItems(last_key);
  }
}

void DownloadBubbleUpdateService::OnContentProviderGoingDown() {
  is_shut_down_ = true;
  main_cache_.DropAllOfflineItems();
}

void DownloadBubbleUpdateService::OnHistoryQueryComplete() {
  download_history_loaded_ = true;
}

void DownloadBubbleUpdateService::OnDownloadHistoryDestroyed() {
  download_history_observation_.Reset();
}

bool DownloadBubbleUpdateService::CacheManager::MaybeAddDownloadItemToCache(
    download::DownloadItem* item,
    bool is_new,
    bool maybe_add_alert) {
  DownloadItemModel model(
      item, std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>());
  if (!ShouldIncludeModel(&model, GetCutoffTime())) {
    return false;
  }
  if (is_new && model.ShouldNotifyUI()) {
    model.SetActionedOn(false);
  }
  if (maybe_add_alert) {
    // Garbage collect accessible alerts before we add another item because this
    // can be called after a long time since the last alert activity.
    accessible_alerts_.GarbageCollect();
    accessible_alerts_.MaybeAddAccessibleAlert(
        model.GetContentId(), GetAccessibleAlertForModel(model));
  }
  return AddItemToCacheImpl(item, download_items_, download_items_iter_map_);
}

bool DownloadBubbleUpdateService::CacheManager::MaybeAddOfflineItemToCache(
    const OfflineItem& item,
    bool is_new,
    bool maybe_add_alert) {
  CHECK(update_service_->IsMainCache(*this));
  if (update_service_->IsProfileOtr() != item.is_off_the_record) {
    return false;
  }
  if (OfflineItemUtils::IsDownload(item.id)) {
    return false;
  }
  if (item.state == offline_items_collection::OfflineItemState::CANCELLED) {
    return false;
  }
  if (item.id.name_space == ContentIndexProviderImpl::kProviderNamespace) {
    return false;
  }

  OfflineItemModel model(
      update_service_->GetOfflineManager(), item,
      std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>());
  if (!ShouldIncludeModel(&model, GetCutoffTime())) {
    return false;
  }
  if (is_new && model.ShouldNotifyUI()) {
    model.SetActionedOn(false);
  }
  if (maybe_add_alert) {
    // Garbage collect accessible alerts before we add another item because this
    // can be called after a long time since the last alert activity.
    accessible_alerts_.GarbageCollect();
    accessible_alerts_.MaybeAddAccessibleAlert(
        model.GetContentId(), GetAccessibleAlertForModel(model));
  }

  return AddItemToCacheImpl(item, offline_items_, offline_items_iter_map_);
}

template <typename Id, typename Item>
bool DownloadBubbleUpdateService::CacheManager::AddItemToCacheImpl(
    Item item,
    SortedItems<Item>& cache,
    IterMap<Id, Item>& iter_map) {
  // This check duplicates part of the ShouldIncludeModel() check, but is still
  // needed because we don't always call that before this function.
  if (GetItemStartTime(item) < GetCutoffTime()) {
    return false;
  }
  Id id = GetItemId(item);
  if (iter_map.contains(id)) {
    return false;
  }
  ItemSortKey key = GetSortKey(item);

  if (cache.size() >= GetNumItemsToCache()) {
    CHECK_EQ(cache.size(), GetNumItemsToCache());
    if (key > GetLastIter(cache)->first) {
      return false;
    }
  }

  auto it = cache.insert(std::make_pair(std::move(key), item));
  iter_map.insert(std::make_pair(id, it));

  while (cache.size() > GetNumItemsToCache()) {
    CHECK(!cache.empty());
    CHECK_EQ(cache.size(), 1 + GetNumItemsToCache());
    auto to_remove = GetLastIter(cache);
    const Id& id_to_remove = GetItemId(to_remove->second);
    iter_map.erase(id_to_remove);
    cache.erase(to_remove);
  }

  UpdateDisplayInfo(id);

  CHECK(!cache.empty());
  auto last_it = GetLastIter(cache);
  return GetItemId(last_it->second) == id;
}

bool DownloadBubbleUpdateService::CacheManager::RemoveDownloadItemFromCache(
    download::DownloadItem* item) {
  return RemoveItemFromCacheImpl(GetItemId(item), download_items_,
                                 download_items_iter_map_);
}

bool DownloadBubbleUpdateService::CacheManager::RemoveOfflineItemFromCache(
    const ContentId& id) {
  CHECK(update_service_->IsMainCache(*this));
  return RemoveItemFromCacheImpl(id, offline_items_, offline_items_iter_map_);
}

template <typename Id, typename Item>
bool DownloadBubbleUpdateService::CacheManager::RemoveItemFromCacheImpl(
    const Id& id,
    SortedItems<Item>& cache,
    IterMap<Id, Item>& iter_map) {
  auto iter_map_it = iter_map.find(id);
  if (iter_map_it == iter_map.end()) {
    return false;
  }

  cache.erase(iter_map_it->second);
  iter_map.erase(iter_map_it);

  UpdateDisplayInfo(id);

  CHECK(cache.size() < GetNumItemsToCache());
  return true;
}

template <typename Id, typename Item>
SortedItems<Item>::iterator
DownloadBubbleUpdateService::CacheManager::RemoveItemFromCacheByIter(
    SortedItems<Item>::iterator iter,
    SortedItems<Item>& cache,
    IterMap<Id, Item>& iter_map) {
  CHECK(iter != cache.end());
  auto next_iter = std::next(iter);
  Id id = GetItemId(iter->second);
  iter_map.erase(id);
  cache.erase(iter);

  UpdateDisplayInfo(id);

  return next_iter;
}

void DownloadBubbleUpdateService::StartBackfillDownloadItems(
    const ItemSortKey& last_key) {
  if (IsShutDown()) {
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&DownloadBubbleUpdateService::BackfillDownloadItems,
                     weak_factory_.GetWeakPtr(), last_key));
}

void DownloadBubbleUpdateService::BackfillDownloadItems(
    const ItemSortKey& last_key) {
  if (IsShutDown()) {
    return;
  }
  for (download::DownloadItem* item : GetAllDownloadItems()) {
    if (GetSortKey(item) < last_key) {
      continue;
    }
    GetCacheForItem(item).MaybeAddDownloadItemToCache(
        item, /*is_new=*/false, /*maybe_add_alert=*/false);
  }
}

void DownloadBubbleUpdateService::StartBackfillOfflineItems(
    const ItemSortKey& last_key) {
  if (IsShutDown()) {
    return;
  }
  offline_items_collection::OfflineContentProvider* provider =
      OfflineContentAggregatorFactory::GetForKey(profile_->GetProfileKey());
  provider->GetAllItems(
      base::BindOnce(&DownloadBubbleUpdateService::BackfillOfflineItems,
                     weak_factory_.GetWeakPtr(), last_key));
}

void DownloadBubbleUpdateService::BackfillOfflineItems(
    const ItemSortKey& last_key,
    const std::vector<OfflineItem>& all_items) {
  if (IsShutDown()) {
    return;
  }
  for (const OfflineItem& item : all_items) {
    if (GetSortKey(item) < last_key) {
      continue;
    }
    main_cache_.MaybeAddOfflineItemToCache(item, /*is_new=*/false,
                                           /*maybe_add_alert=*/false);
  }
}

void DownloadBubbleUpdateService::CacheManager::DropAllDownloadItems() {
  download_items_.clear();
  download_items_iter_map_.clear();
}

void DownloadBubbleUpdateService::InitializeDownloadItemsCache() {
  CHECK(download_item_notifier_);
  for (CacheManager* cache : GetAllCacheManagers()) {
    cache->DropAllDownloadItems();
  }
  for (download::DownloadItem* item : GetAllDownloadItems()) {
    GetCacheForItem(item).MaybeAddDownloadItemToCache(
        item, /*is_new=*/false, /*maybe_add_alert=*/false);
  }
}

void DownloadBubbleUpdateService::CacheManager::DropAllOfflineItems() {
  CHECK(update_service_->IsMainCache(*this));
  offline_items_.clear();
  offline_items_iter_map_.clear();
}

void DownloadBubbleUpdateService::StartInitializeOfflineItemsCache() {
  if (IsShutDown()) {
    return;
  }
  if (offline_items_initialized_) {
    return;
  }
  offline_items_collection::OfflineContentProvider* provider =
      OfflineContentAggregatorFactory::GetForKey(profile_->GetProfileKey());
  provider->GetAllItems(
      base::BindOnce(&DownloadBubbleUpdateService::InitializeOfflineItemsCache,
                     weak_factory_.GetWeakPtr()));
}

void DownloadBubbleUpdateService::InitializeOfflineItemsCache(
    const std::vector<OfflineItem>& all_items) {
  main_cache_.DropAllOfflineItems();
  for (const OfflineItem& item : all_items) {
    main_cache_.MaybeAddOfflineItemToCache(item, /*is_new=*/false,
                                           /*maybe_add_alert=*/false);
  }
  offline_items_initialized_ = true;
  for (auto& callback : offline_item_callbacks_) {
    std::move(callback).Run();
  }
  offline_item_callbacks_.clear();
}

std::vector<raw_ptr<download::DownloadItem, VectorExperimental>>
DownloadBubbleUpdateService::GetAllDownloadItems() {
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> all_items;
  if (download_item_notifier_) {
    download_item_notifier_->GetManager()->GetAllDownloads(&all_items);
  }
  if (original_download_item_notifier_) {
    original_download_item_notifier_->GetManager()->GetAllDownloads(&all_items);
  }
  return all_items;
}

OfflineItemModelManager* DownloadBubbleUpdateService::GetOfflineManager()
    const {
  return OfflineItemModelManagerFactory::GetForBrowserContext(profile_);
}

bool DownloadBubbleUpdateService::CacheManager::MaybeAddDownloadItemModel(
    download::DownloadItem* item,
    base::Time cutoff_time,
    std::vector<DownloadUIModelPtr>& models) {
  DownloadUIModelPtr model = DownloadItemModel::Wrap(
      item, std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>());
  return MaybeAddModel(std::move(model), cutoff_time, GetMaxNumItemsToShow(),
                       models);
}

bool DownloadBubbleUpdateService::CacheManager::MaybeAddOfflineItemModel(
    const offline_items_collection::OfflineItem& item,
    base::Time cutoff_time,
    std::vector<DownloadUIModelPtr>& models) {
  DownloadUIModelPtr model = OfflineItemModel::Wrap(
      update_service_->GetOfflineManager(), item,
      std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>());
  return MaybeAddModel(std::move(model), cutoff_time, GetMaxNumItemsToShow(),
                       models);
}

void DownloadBubbleUpdateService::CacheManager::AppendBackfilledDownloadItems(
    const ItemSortKey& last_key,
    base::Time cutoff_time,
    std::vector<DownloadUIModelPtr>& models) {
  // This is not quite right because there might be a newly backfilled item
  // whose key is equal to |last_key| that this would then skip
  // over (and we would not be able to detect/fix the omission, unless the item
  // received an update later), but this should happen rarely enough (requires
  // two download items with the exact same creation time) that we will not
  // handle this case.
  auto it = download_items_.upper_bound(last_key);

  while (it != download_items_.end()) {
    if (!MaybeAddDownloadItemModel(it->second, cutoff_time, models)) {
      it = RemoveItemFromCacheByIter(it, download_items_,
                                     download_items_iter_map_);
    } else {
      ++it;
    }
  }
}

bool DownloadBubbleUpdateService::IsMainCache(
    const DownloadBubbleUpdateService::CacheManager& cache) const {
  return &cache == &main_cache_;
}

void DownloadBubbleUpdateService::OnEphemeralWarningExpired(
    const std::string& guid) {
  if (IsShutDown()) {
    return;
  }
  CHECK(download_item_notifier_ || original_download_item_notifier_);
  if (!download_item_notifier_) {
    return;
  }
  content::DownloadManager* download_manager = GetDownloadManager();
  if (!download_manager) {
    return;
  }

  download::DownloadItem* item = download_manager->GetDownloadByGuid(guid);
  // The item might be from the original profile.
  if (!item && original_download_item_notifier_ &&
      original_download_item_notifier_->GetManager()) {
    item =
        original_download_item_notifier_->GetManager()->GetDownloadByGuid(guid);
  }
  if (!item) {
    return;
  }

  GetCacheForItem(item).UpdateDisplayInfo(guid);

  auto* web_app_data = DownloadItemWebAppData::Get(item);
  for (Browser* browser : chrome::FindAllBrowsersWithProfile(profile_)) {
    if (browser->window() &&
        browser->window()->GetDownloadBubbleUIController() &&
        BrowserMatchesWebAppData(browser, web_app_data)) {
      browser->window()->GetDownloadBubbleUIController()->OnDownloadItemRemoved(
          item);
    }
  }
}

#if DCHECK_IS_ON()
void DownloadBubbleUpdateService::CacheManager::ConsistencyCheckCaches() const {
  ConsistencyCheckImpl(download_items_, download_items_iter_map_);
  ConsistencyCheckImpl(offline_items_, offline_items_iter_map_);
}

template <typename Id, typename Item>
void DownloadBubbleUpdateService::CacheManager::ConsistencyCheckImpl(
    const SortedItems<Item>& cache,
    const IterMap<Id, Item>& iter_map) const {
  DCHECK_EQ(cache.size(), iter_map.size())
      << "Cache size " << cache.size() << " does not match index size "
      << iter_map.size() << ".";
  DCHECK_LE(cache.size(), GetNumItemsToCache())
      << "Cache size " << cache.size() << " exceeds max size "
      << GetNumItemsToCache() << ".";
  for (auto it = cache.begin(); it != cache.end(); ++it) {
    const ItemSortKey& key = it->first;
    const Item& item = it->second;
    // The state of the stored key and the current state of the item may not
    // always match, if we haven't received the update notification yet.
    DCHECK_EQ(key.start_time, GetSortKey(item).start_time)
        << "Start time in key does not match item.";
    const Id& id = GetItemId(item);
    auto iter_map_it = iter_map.find(id);
    DCHECK(iter_map_it != iter_map.end()) << "Item id not in index.";
    DCHECK(iter_map_it->second == it) << "Index inconsistent.";
  }
}
#endif  // DCHECK_IS_ON()
