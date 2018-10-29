// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_snippets/download_suggestions_provider.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/guid.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/pref_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

using download::DownloadItem;
using content::DownloadManager;
using ntp_snippets::Category;
using ntp_snippets::CategoryInfo;
using ntp_snippets::CategoryStatus;
using ntp_snippets::ContentSuggestion;
using ntp_snippets::prefs::kDismissedAssetDownloadSuggestions;
using ntp_snippets::prefs::kDismissedOfflinePageDownloadSuggestions;
using offline_pages::OfflinePageItem;

namespace {

const int kDefaultMaxSuggestionsCount = 5;
const int kDefaultMaxDownloadAgeHours = 24;
const char kAssetDownloadsPrefix = 'D';
const char kOfflinePageDownloadsPrefix = 'O';

// NOTE: You must set variation param values for both features (one of them may
// be disabled in future).
const char kMaxSuggestionsCountParamName[] = "downloads_max_count";
const char kMaxDownloadAgeHoursParamName[] = "downloads_max_age_hours";

const base::Feature& GetEnabledDownloadsFeature() {
  bool assets_enabled =
      base::FeatureList::IsEnabled(features::kAssetDownloadSuggestionsFeature);
  DCHECK(assets_enabled ||
         base::FeatureList::IsEnabled(
             features::kOfflinePageDownloadSuggestionsFeature));
  return assets_enabled ? features::kAssetDownloadSuggestionsFeature
                        : features::kOfflinePageDownloadSuggestionsFeature;
}

int GetMaxSuggestionsCount() {
  // One cannot get a variation param from a disabled feature, so the enabled
  // one is taken.
  return variations::GetVariationParamByFeatureAsInt(
      GetEnabledDownloadsFeature(), kMaxSuggestionsCountParamName,
      kDefaultMaxSuggestionsCount);
}

int GetMaxDownloadAgeHours() {
  // One cannot get a variation param from a disabled feature, so the enabled
  // one is taken.
  return variations::GetVariationParamByFeatureAsInt(
      GetEnabledDownloadsFeature(), kMaxDownloadAgeHoursParamName,
      kDefaultMaxDownloadAgeHours);
}

base::Time GetOfflinePagePublishedTime(const OfflinePageItem& item) {
  return item.creation_time;
}

bool CompareOfflinePagesMostRecentlyPublishedFirst(
    const OfflinePageItem& left,
    const OfflinePageItem& right) {
  return GetOfflinePagePublishedTime(left) > GetOfflinePagePublishedTime(right);
}

std::string GetOfflinePagePerCategoryID(int64_t raw_offline_page_id) {
  // Raw ID is prefixed in order to avoid conflicts with asset downloads.
  return std::string(1, kOfflinePageDownloadsPrefix) +
         base::IntToString(raw_offline_page_id);
}

std::string GetAssetDownloadPerCategoryID(uint32_t raw_download_id) {
  // Raw ID is prefixed in order to avoid conflicts with offline page downloads.
  return std::string(1, kAssetDownloadsPrefix) +
         base::UintToString(raw_download_id);
}

// Determines whether |suggestion_id| corresponds to offline page suggestion or
// asset download based on |id_within_category| prefix.
bool CorrespondsToOfflinePage(const ContentSuggestion::ID& suggestion_id) {
  const std::string& id_within_category = suggestion_id.id_within_category();
  if (!id_within_category.empty()) {
    if (id_within_category[0] == kOfflinePageDownloadsPrefix) {
      return true;
    }
    if (id_within_category[0] == kAssetDownloadsPrefix) {
      return false;
    }
  }
  NOTREACHED() << "Unknown id_within_category " << id_within_category;
  return false;
}

bool IsAssetDownloadCompleted(const DownloadItem& item) {
  // Transient downloads are cleaned up after completion, therefore, they should
  // be ignored.
  return !item.IsTransient() &&
         item.GetState() == DownloadItem::DownloadState::COMPLETE &&
         !item.GetFileExternallyRemoved();
}

base::Time GetAssetDownloadPublishedTime(const DownloadItem& item) {
  return item.GetStartTime();
}

bool CompareDownloadsMostRecentlyPublishedFirst(const DownloadItem* left,
                                                const DownloadItem* right) {
  return GetAssetDownloadPublishedTime(*left) >
         GetAssetDownloadPublishedTime(*right);
}

bool IsClientIdForOfflinePageDownload(
    offline_pages::ClientPolicyController* policy_controller,
    const offline_pages::ClientId& client_id) {
  return policy_controller->IsSupportedByDownload(client_id.name_space) &&
         !policy_controller->IsSuggested(client_id.name_space);
}

}  // namespace

DownloadSuggestionsProvider::DownloadSuggestionsProvider(
    ContentSuggestionsProvider::Observer* observer,
    offline_pages::OfflinePageModel* offline_page_model,
    content::DownloadManager* download_manager,
    DownloadHistory* download_history,
    PrefService* pref_service,
    base::Clock* clock)
    : ContentSuggestionsProvider(observer),
      category_status_(CategoryStatus::AVAILABLE_LOADING),
      provided_category_(Category::FromKnownCategory(
          ntp_snippets::KnownCategories::DOWNLOADS)),
      offline_page_model_(offline_page_model),
      download_manager_(download_manager),
      download_history_(download_history),
      pref_service_(pref_service),
      clock_(clock),
      is_asset_downloads_initialization_complete_(false),
      weak_ptr_factory_(this) {
  observer->OnCategoryStatusChanged(this, provided_category_, category_status_);

  DCHECK(offline_page_model_ || download_manager_);
  if (offline_page_model_) {
    offline_page_model_->AddObserver(this);
  }

  if (download_manager_) {
    // We will start listening to download manager once it is loaded.
    // May be nullptr in tests.
    if (download_history_) {
      download_history_->AddObserver(this);
    }
  } else {
    download_history_ = nullptr;
  }

  if (!download_manager_) {
    // Usually, all downloads are fetched when the download manager is loaded,
    // but now it is disabled, so offline pages are fetched here instead.
    AsynchronouslyFetchOfflinePagesDownloads(/*notify=*/true);
  }
}

DownloadSuggestionsProvider::~DownloadSuggestionsProvider() {
  if (download_history_) {
    download_history_->RemoveObserver(this);
  }

  if (download_manager_) {
    download_manager_->RemoveObserver(this);
    UnregisterDownloadItemObservers();
  }

  if (offline_page_model_) {
    offline_page_model_->RemoveObserver(this);
  }
}

CategoryStatus DownloadSuggestionsProvider::GetCategoryStatus(
    Category category) {
  DCHECK_EQ(provided_category_, category);
  return category_status_;
}

CategoryInfo DownloadSuggestionsProvider::GetCategoryInfo(Category category) {
  DCHECK_EQ(provided_category_, category);
  return CategoryInfo(
      l10n_util::GetStringUTF16(IDS_NTP_DOWNLOAD_SUGGESTIONS_SECTION_HEADER),
      ntp_snippets::ContentSuggestionsCardLayout::FULL_CARD,
      ntp_snippets::ContentSuggestionsAdditionalAction::VIEW_ALL,
      /*show_if_empty=*/false,
      l10n_util::GetStringUTF16(IDS_NTP_DOWNLOADS_SUGGESTIONS_SECTION_EMPTY));
}

void DownloadSuggestionsProvider::DismissSuggestion(
    const ContentSuggestion::ID& suggestion_id) {
  DCHECK_EQ(provided_category_, suggestion_id.category());
  std::set<std::string> dismissed_ids =
      ReadDismissedIDsFromPrefs(CorrespondsToOfflinePage(suggestion_id));
  dismissed_ids.insert(suggestion_id.id_within_category());
  StoreDismissedIDsToPrefs(CorrespondsToOfflinePage(suggestion_id),
                           dismissed_ids);

  RemoveSuggestionFromCacheAndRetrieveMoreIfNeeded(suggestion_id);
}

void DownloadSuggestionsProvider::FetchSuggestionImage(
    const ContentSuggestion::ID& suggestion_id,
    ntp_snippets::ImageFetchedCallback callback) {
  // TODO(vitaliii): Fetch proper thumbnail from OfflinePageModel once it is
  // available there.
  // TODO(vitaliii): Provide site's favicon for assets downloads or file type.
  // See crbug.com/631447.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), gfx::Image()));
}

void DownloadSuggestionsProvider::FetchSuggestionImageData(
    const ContentSuggestion::ID& suggestion_id,
    ntp_snippets::ImageDataFetchedCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::string()));
}

void DownloadSuggestionsProvider::Fetch(
    const ntp_snippets::Category& category,
    const std::set<std::string>& known_suggestion_ids,
    ntp_snippets::FetchDoneCallback callback) {
  LOG(DFATAL) << "DownloadSuggestionsProvider has no |Fetch| functionality!";
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback),
          ntp_snippets::Status(
              ntp_snippets::StatusCode::PERMANENT_ERROR,
              "DownloadSuggestionsProvider has no |Fetch| functionality!"),
          std::vector<ContentSuggestion>()));
}

void DownloadSuggestionsProvider::ClearHistory(
    base::Time begin,
    base::Time end,
    const base::Callback<bool(const GURL& url)>& filter) {
  cached_offline_page_downloads_.clear();
  cached_asset_downloads_.clear();
  // This will trigger an asynchronous re-fetch.
  ClearDismissedSuggestionsForDebugging(provided_category_);
}

void DownloadSuggestionsProvider::ClearCachedSuggestions() {
  // Ignored. The internal caches are not stored on disk and they are just
  // partial copies of the data stored at OfflinePage model and DownloadManager.
  // If it is cleared there, it will be cleared in these caches as well.
}

void DownloadSuggestionsProvider::GetDismissedSuggestionsForDebugging(
    Category category,
    ntp_snippets::DismissedSuggestionsCallback callback) {
  DCHECK_EQ(provided_category_, category);

  if (offline_page_model_) {
    // Offline pages which are not related to downloads are also queried here,
    // so that they can be returned if they happen to be dismissed (e.g. due to
    // a bug).
    offline_page_model_->GetAllPages(base::Bind(
        &DownloadSuggestionsProvider::
            GetPagesMatchingQueryCallbackForGetDismissedSuggestions,
        weak_ptr_factory_.GetWeakPtr(), base::Passed(std::move(callback))));
  } else {
    GetPagesMatchingQueryCallbackForGetDismissedSuggestions(
        std::move(callback), std::vector<OfflinePageItem>());
  }
}

void DownloadSuggestionsProvider::ClearDismissedSuggestionsForDebugging(
    Category category) {
  DCHECK_EQ(provided_category_, category);
  StoreAssetDismissedIDsToPrefs(std::set<std::string>());
  StoreOfflinePageDismissedIDsToPrefs(std::set<std::string>());
  AsynchronouslyFetchAllDownloadsAndSubmitSuggestions();
}

// static
void DownloadSuggestionsProvider::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(kDismissedAssetDownloadSuggestions);
  registry->RegisterListPref(kDismissedOfflinePageDownloadSuggestions);
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

void DownloadSuggestionsProvider::
    GetPagesMatchingQueryCallbackForGetDismissedSuggestions(
        ntp_snippets::DismissedSuggestionsCallback callback,
        const std::vector<OfflinePageItem>& offline_pages) const {
  std::set<std::string> dismissed_ids = ReadOfflinePageDismissedIDsFromPrefs();
  std::vector<ContentSuggestion> suggestions;
  for (const OfflinePageItem& item : offline_pages) {
    if (dismissed_ids.count(GetOfflinePagePerCategoryID(item.offline_id))) {
      suggestions.push_back(ConvertOfflinePage(item));
    }
  }

  if (download_manager_) {
    std::vector<DownloadItem*> all_downloads;
    download_manager_->GetAllDownloads(&all_downloads);

    dismissed_ids = ReadAssetDismissedIDsFromPrefs();

    for (const DownloadItem* item : all_downloads) {
      if (dismissed_ids.count(GetAssetDownloadPerCategoryID(item->GetId()))) {
        suggestions.push_back(ConvertDownloadItem(*item));
      }
    }
  }

  std::move(callback).Run(std::move(suggestions));
}

void DownloadSuggestionsProvider::OfflinePageModelLoaded(
    offline_pages::OfflinePageModel* model) {
  DCHECK_EQ(offline_page_model_, model);
  // Ignored. We issue a fetch in the constructor (or when Downloads Manager is
  // loaded) and Offline Page model answers asynchronously once it has been
  // loaded.
}

void DownloadSuggestionsProvider::OfflinePageAdded(
    offline_pages::OfflinePageModel* model,
    const offline_pages::OfflinePageItem& added_page) {
  DCHECK_EQ(offline_page_model_, model);
  if (!IsClientIdForOfflinePageDownload(model->GetPolicyController(),
                                        added_page.client_id)) {
    return;
  }

  // This is all in one statement so that it is completely compiled out in
  // release builds.
  DCHECK_EQ(ReadOfflinePageDismissedIDsFromPrefs().count(
                GetOfflinePagePerCategoryID(added_page.offline_id)),
            0U);

  int max_suggestions_count = GetMaxSuggestionsCount();
  if (static_cast<int>(cached_offline_page_downloads_.size()) <
      max_suggestions_count) {
    cached_offline_page_downloads_.push_back(added_page);
  } else if (max_suggestions_count > 0) {
    auto oldest_page_iterator =
        std::max_element(cached_offline_page_downloads_.begin(),
                         cached_offline_page_downloads_.end(),
                         &CompareOfflinePagesMostRecentlyPublishedFirst);
    *oldest_page_iterator = added_page;
  }

  SubmitContentSuggestions();
}

void DownloadSuggestionsProvider::OfflinePageDeleted(
    const offline_pages::OfflinePageModel::DeletedPageInfo& page_info) {
  DCHECK(offline_page_model_);
  if (IsClientIdForOfflinePageDownload(
          offline_page_model_->GetPolicyController(), page_info.client_id)) {
    InvalidateSuggestion(GetOfflinePagePerCategoryID(page_info.offline_id));
  }
}

void DownloadSuggestionsProvider::OnDownloadCreated(DownloadManager* manager,
                                                    DownloadItem* item) {
  DCHECK(is_asset_downloads_initialization_complete_);
  DCHECK_EQ(download_manager_, manager);

  // This is called when new downloads are started. We listen to each item to
  // know when it is finished or destroyed.
  item->AddObserver(this);
  if (CacheAssetDownloadIfNeeded(item)) {
    SubmitContentSuggestions();
  }
}

void DownloadSuggestionsProvider::ManagerGoingDown(DownloadManager* manager) {
  DCHECK_EQ(download_manager_, manager);
  UnregisterDownloadItemObservers();
  download_manager_ = nullptr;
  if (download_history_) {
    download_history_->RemoveObserver(this);
    download_history_ = nullptr;
  }
}

void DownloadSuggestionsProvider::OnDownloadUpdated(DownloadItem* item) {
  DCHECK(is_asset_downloads_initialization_complete_);
  if (base::ContainsValue(cached_asset_downloads_, item)) {
    if (item->GetFileExternallyRemoved()) {
      InvalidateSuggestion(GetAssetDownloadPerCategoryID(item->GetId()));
    } else {
      // The download may have changed.
      SubmitContentSuggestions();
    }
  } else {
    // Unfinished downloads may become completed.
    if (CacheAssetDownloadIfNeeded(item)) {
      SubmitContentSuggestions();
    }
  }
}

void DownloadSuggestionsProvider::OnDownloadOpened(DownloadItem* item) {
  // Ignored.
}

void DownloadSuggestionsProvider::OnDownloadRemoved(DownloadItem* item) {
  // Ignored. We listen to |OnDownloadDestroyed| instead. The reason is that
  // we may need to retrieve all downloads, but |OnDownloadRemoved| is called
  // before the download is removed from the list.
}

void DownloadSuggestionsProvider::OnDownloadDestroyed(
    download::DownloadItem* item) {
  DCHECK(is_asset_downloads_initialization_complete_);

  item->RemoveObserver(this);

  if (!IsAssetDownloadCompleted(*item)) {
    return;
  }
  // TODO(vitaliii): Implement a better way to clean up dismissed IDs (in case
  // some calls are missed).
  InvalidateSuggestion(GetAssetDownloadPerCategoryID(item->GetId()));
}

void DownloadSuggestionsProvider::OnHistoryQueryComplete() {
  is_asset_downloads_initialization_complete_ = true;
  if (download_manager_) {
    download_manager_->AddObserver(this);
  }
  AsynchronouslyFetchAllDownloadsAndSubmitSuggestions();
}

void DownloadSuggestionsProvider::OnDownloadHistoryDestroyed() {
  DCHECK(download_history_);
  download_history_->RemoveObserver(this);
  download_history_ = nullptr;
}

void DownloadSuggestionsProvider::NotifyStatusChanged(
    CategoryStatus new_status) {
  DCHECK_NE(CategoryStatus::NOT_PROVIDED, category_status_);
  DCHECK_NE(CategoryStatus::NOT_PROVIDED, new_status);
  if (category_status_ == new_status) {
    return;
  }
  category_status_ = new_status;
  observer()->OnCategoryStatusChanged(this, provided_category_,
                                      category_status_);
}

void DownloadSuggestionsProvider::AsynchronouslyFetchOfflinePagesDownloads(
    bool notify) {
  if (!offline_page_model_) {
    // Offline pages are explicitly turned off, so we propagate "no pages"
    // further e.g. to clean its prefs.
    UpdateOfflinePagesCache(notify, std::vector<OfflinePageItem>());
    return;
  }

  // If Offline Page model is not loaded yet, it will process our query once it
  // has finished loading.
  offline_page_model_->GetPagesSupportedByDownloads(
      base::Bind(&DownloadSuggestionsProvider::UpdateOfflinePagesCache,
                 weak_ptr_factory_.GetWeakPtr(), notify));
}

void DownloadSuggestionsProvider::FetchAssetsDownloads() {
  if (!download_manager_) {
    // The manager has gone down or was explicitly turned off.
    return;
  }

  std::vector<DownloadItem*> all_downloads;
  download_manager_->GetAllDownloads(&all_downloads);
  std::set<std::string> old_dismissed_ids = ReadAssetDismissedIDsFromPrefs();
  std::set<std::string> retained_dismissed_ids;
  cached_asset_downloads_.clear();
  for (DownloadItem* item : all_downloads) {
    std::string within_category_id =
        GetAssetDownloadPerCategoryID(item->GetId());
    if (old_dismissed_ids.count(within_category_id)) {
      retained_dismissed_ids.insert(within_category_id);
    } else if (IsAssetDownloadCompleted(*item) &&
               !IsDownloadOutdated(GetAssetDownloadPublishedTime(*item),
                                   item->GetLastAccessTime())) {
      cached_asset_downloads_.push_back(item);
      // We may already observe this item and, therefore, we remove the
      // observer first.
      item->RemoveObserver(this);
      item->AddObserver(this);
    }
  }

  if (old_dismissed_ids.size() != retained_dismissed_ids.size()) {
    StoreAssetDismissedIDsToPrefs(retained_dismissed_ids);
  }

  const int max_suggestions_count = GetMaxSuggestionsCount();
  if (static_cast<int>(cached_asset_downloads_.size()) >
      max_suggestions_count) {
    // Partially sorts |downloads| such that:
    // 1) The element at the index |max_suggestions_count| is changed to the
    //    element which would occur on this position if |downloads| was sorted;
    // 2) All of the elements before index |max_suggestions_count| are less than
    //    or equal to the elements after it.
    std::nth_element(cached_asset_downloads_.begin(),
                     cached_asset_downloads_.begin() + max_suggestions_count,
                     cached_asset_downloads_.end(),
                     &CompareDownloadsMostRecentlyPublishedFirst);
    cached_asset_downloads_.resize(max_suggestions_count);
  }
}

void DownloadSuggestionsProvider::
    AsynchronouslyFetchAllDownloadsAndSubmitSuggestions() {
  FetchAssetsDownloads();
  AsynchronouslyFetchOfflinePagesDownloads(/*notify=*/true);
}

void DownloadSuggestionsProvider::SubmitContentSuggestions() {
  NotifyStatusChanged(CategoryStatus::AVAILABLE);

  std::vector<ContentSuggestion> suggestions;
  for (const OfflinePageItem& item : cached_offline_page_downloads_) {
    suggestions.push_back(ConvertOfflinePage(item));
  }

  for (const DownloadItem* item : cached_asset_downloads_) {
    suggestions.push_back(ConvertDownloadItem(*item));
  }

  std::sort(suggestions.begin(), suggestions.end(),
            [](const ContentSuggestion& left, const ContentSuggestion& right) {
              return left.publish_date() > right.publish_date();
            });

  const int max_suggestions_count = GetMaxSuggestionsCount();
  if (static_cast<int>(suggestions.size()) > max_suggestions_count) {
    suggestions.erase(suggestions.begin() + max_suggestions_count,
                      suggestions.end());
  }

  observer()->OnNewSuggestions(this, provided_category_,
                               std::move(suggestions));
}

ContentSuggestion DownloadSuggestionsProvider::ConvertOfflinePage(
    const OfflinePageItem& offline_page) const {
  ContentSuggestion suggestion(
      ContentSuggestion::ID(provided_category_, GetOfflinePagePerCategoryID(
                                                    offline_page.offline_id)),
      offline_page.url);

  if (offline_page.title.empty()) {
    // TODO(vitaliii): Remove this fallback once the OfflinePageModel provides
    // titles for all (relevant) OfflinePageItems.
    suggestion.set_title(base::UTF8ToUTF16(offline_page.url.spec()));
  } else {
    suggestion.set_title(offline_page.title);
  }
  suggestion.set_publish_date(GetOfflinePagePublishedTime(offline_page));
  suggestion.set_publisher_name(base::UTF8ToUTF16(offline_page.url.host()));
  auto extra = std::make_unique<ntp_snippets::DownloadSuggestionExtra>();
  extra->is_download_asset = false;
  extra->offline_page_id = offline_page.offline_id;
  suggestion.set_download_suggestion_extra(std::move(extra));
  return suggestion;
}

ContentSuggestion DownloadSuggestionsProvider::ConvertDownloadItem(
    const DownloadItem& download_item) const {
  ContentSuggestion suggestion(
      ContentSuggestion::ID(provided_category_, GetAssetDownloadPerCategoryID(
                                                    download_item.GetId())),
      download_item.GetOriginalUrl());
  suggestion.set_title(
      download_item.GetTargetFilePath().BaseName().LossyDisplayName());
  suggestion.set_publish_date(GetAssetDownloadPublishedTime(download_item));
  suggestion.set_publisher_name(
      base::UTF8ToUTF16(download_item.GetURL().host()));
  auto extra = std::make_unique<ntp_snippets::DownloadSuggestionExtra>();
  extra->download_guid = download_item.GetGuid();
  extra->target_file_path = download_item.GetTargetFilePath();
  extra->mime_type = download_item.GetMimeType();
  extra->is_download_asset = true;
  suggestion.set_download_suggestion_extra(std::move(extra));
  return suggestion;
}

bool DownloadSuggestionsProvider::IsDownloadOutdated(
    const base::Time& published_time,
    const base::Time& last_visited_time) {
  DCHECK(last_visited_time == base::Time() ||
         last_visited_time >= published_time);
  const base::Time& last_interaction_time =
      (last_visited_time == base::Time() ? published_time : last_visited_time);
  return last_interaction_time <
         clock_->Now() - base::TimeDelta::FromHours(GetMaxDownloadAgeHours());
}

bool DownloadSuggestionsProvider::CacheAssetDownloadIfNeeded(
    const download::DownloadItem* item) {
  if (!IsAssetDownloadCompleted(*item)) {
    return false;
  }

  if (base::ContainsValue(cached_asset_downloads_, item)) {
    return false;
  }

  std::set<std::string> dismissed_ids = ReadAssetDismissedIDsFromPrefs();
  if (dismissed_ids.count(GetAssetDownloadPerCategoryID(item->GetId()))) {
    return false;
  }

  DCHECK_LE(static_cast<int>(cached_asset_downloads_.size()),
            GetMaxSuggestionsCount());
  if (static_cast<int>(cached_asset_downloads_.size()) ==
      GetMaxSuggestionsCount()) {
    auto oldest = std::max_element(cached_asset_downloads_.begin(),
                                   cached_asset_downloads_.end(),
                                   &CompareDownloadsMostRecentlyPublishedFirst);
    if (GetAssetDownloadPublishedTime(*item) <=
        GetAssetDownloadPublishedTime(**oldest)) {
      return false;
    }

    *oldest = item;
  } else {
    cached_asset_downloads_.push_back(item);
  }

  return true;
}

bool DownloadSuggestionsProvider::RemoveSuggestionFromCacheIfPresent(
    const ContentSuggestion::ID& suggestion_id) {
  DCHECK_EQ(provided_category_, suggestion_id.category());
  if (CorrespondsToOfflinePage(suggestion_id)) {
    auto matching =
        std::find_if(cached_offline_page_downloads_.begin(),
                     cached_offline_page_downloads_.end(),
                     [&suggestion_id](const OfflinePageItem& item) {
                       return GetOfflinePagePerCategoryID(item.offline_id) ==
                              suggestion_id.id_within_category();
                     });
    if (matching != cached_offline_page_downloads_.end()) {
      cached_offline_page_downloads_.erase(matching);
      return true;
    }
    return false;
  }

  auto matching = std::find_if(
      cached_asset_downloads_.begin(), cached_asset_downloads_.end(),
      [&suggestion_id](const DownloadItem* item) {
        return GetAssetDownloadPerCategoryID(item->GetId()) ==
               suggestion_id.id_within_category();
      });
  if (matching != cached_asset_downloads_.end()) {
    cached_asset_downloads_.erase(matching);
    return true;
  }
  return false;
}

void DownloadSuggestionsProvider::
    RemoveSuggestionFromCacheAndRetrieveMoreIfNeeded(
        const ContentSuggestion::ID& suggestion_id) {
  DCHECK_EQ(provided_category_, suggestion_id.category());
  if (!RemoveSuggestionFromCacheIfPresent(suggestion_id)) {
    return;
  }

  if (CorrespondsToOfflinePage(suggestion_id)) {
    if (static_cast<int>(cached_offline_page_downloads_.size()) ==
        GetMaxSuggestionsCount() - 1) {
      // Previously there were |GetMaxSuggestionsCount()| cached suggestion,
      // therefore, overall there may be more than |GetMaxSuggestionsCount()|
      // suggestions in the model and now one of them may be cached instead of
      // the removed one. Even though, the suggestions are not immediately
      // used the cache has to be kept up to date, because it may be used when
      // other data source is updated.
      AsynchronouslyFetchOfflinePagesDownloads(/*notify=*/false);
    }
  } else {
    if (static_cast<int>(cached_asset_downloads_.size()) ==
        GetMaxSuggestionsCount() - 1) {
      // The same as the case above.
      FetchAssetsDownloads();
    }
  }
}

void DownloadSuggestionsProvider::UpdateOfflinePagesCache(
    bool notify,
    const std::vector<offline_pages::OfflinePageItem>&
        all_download_offline_pages) {
  std::set<std::string> old_dismissed_ids =
      ReadOfflinePageDismissedIDsFromPrefs();
  std::set<std::string> retained_dismissed_ids;
  std::vector<const OfflinePageItem*> items;
  // Filtering out dismissed items and pruning dismissed IDs.
  for (const OfflinePageItem& item : all_download_offline_pages) {
    std::string id_within_category =
        GetOfflinePagePerCategoryID(item.offline_id);
    if (old_dismissed_ids.count(id_within_category)) {
      retained_dismissed_ids.insert(id_within_category);
    } else {
      if (!IsDownloadOutdated(GetOfflinePagePublishedTime(item),
                              item.last_access_time) &&
          IsClientIdForOfflinePageDownload(
              offline_page_model_->GetPolicyController(), item.client_id)) {
        items.push_back(&item);
      }
    }
  }

  const int max_suggestions_count = GetMaxSuggestionsCount();
  if (static_cast<int>(items.size()) > max_suggestions_count) {
    // Partially sorts |items| such that:
    // 1) The element at the index |max_suggestions_count| is changed to the
    //    element which would occur on this position if |items| was sorted;
    // 2) All of the elements before index |max_suggestions_count| are less than
    //    or equal to the elements after it.
    std::nth_element(
        items.begin(), items.begin() + max_suggestions_count, items.end(),
        [](const OfflinePageItem* left, const OfflinePageItem* right) {
          return CompareOfflinePagesMostRecentlyPublishedFirst(*left, *right);
        });
    items.resize(max_suggestions_count);
  }

  cached_offline_page_downloads_.clear();
  for (const OfflinePageItem* item : items) {
    cached_offline_page_downloads_.push_back(*item);
  }

  if (old_dismissed_ids.size() != retained_dismissed_ids.size()) {
    StoreOfflinePageDismissedIDsToPrefs(retained_dismissed_ids);
  }

  if (notify) {
    SubmitContentSuggestions();
  }
}

void DownloadSuggestionsProvider::InvalidateSuggestion(
    const std::string& id_within_category) {
  ContentSuggestion::ID suggestion_id(provided_category_, id_within_category);
  observer()->OnSuggestionInvalidated(this, suggestion_id);

  std::set<std::string> dismissed_ids =
      ReadDismissedIDsFromPrefs(CorrespondsToOfflinePage(suggestion_id));
  auto it = dismissed_ids.find(id_within_category);
  if (it != dismissed_ids.end()) {
    dismissed_ids.erase(it);
    StoreDismissedIDsToPrefs(CorrespondsToOfflinePage(suggestion_id),
                             dismissed_ids);
  }

  RemoveSuggestionFromCacheAndRetrieveMoreIfNeeded(suggestion_id);
}

std::set<std::string>
DownloadSuggestionsProvider::ReadAssetDismissedIDsFromPrefs() const {
  return ntp_snippets::prefs::ReadDismissedIDsFromPrefs(
      *pref_service_, kDismissedAssetDownloadSuggestions);
}

void DownloadSuggestionsProvider::StoreAssetDismissedIDsToPrefs(
    const std::set<std::string>& dismissed_ids) {
  DCHECK(std::all_of(
      dismissed_ids.begin(), dismissed_ids.end(),
      [](const std::string& id) { return id[0] == kAssetDownloadsPrefix; }));
  ntp_snippets::prefs::StoreDismissedIDsToPrefs(
      pref_service_, kDismissedAssetDownloadSuggestions, dismissed_ids);
}

std::set<std::string>
DownloadSuggestionsProvider::ReadOfflinePageDismissedIDsFromPrefs() const {
  return ntp_snippets::prefs::ReadDismissedIDsFromPrefs(
      *pref_service_, kDismissedOfflinePageDownloadSuggestions);
}

void DownloadSuggestionsProvider::StoreOfflinePageDismissedIDsToPrefs(
    const std::set<std::string>& dismissed_ids) {
  DCHECK(std::all_of(dismissed_ids.begin(), dismissed_ids.end(),
                     [](const std::string& id) {
                       return id[0] == kOfflinePageDownloadsPrefix;
                     }));
  ntp_snippets::prefs::StoreDismissedIDsToPrefs(
      pref_service_, kDismissedOfflinePageDownloadSuggestions, dismissed_ids);
}

std::set<std::string> DownloadSuggestionsProvider::ReadDismissedIDsFromPrefs(
    bool for_offline_page_downloads) const {
  if (for_offline_page_downloads) {
    return ReadOfflinePageDismissedIDsFromPrefs();
  }
  return ReadAssetDismissedIDsFromPrefs();
}

void DownloadSuggestionsProvider::StoreDismissedIDsToPrefs(
    bool for_offline_page_downloads,
    const std::set<std::string>& dismissed_ids) {
  if (for_offline_page_downloads) {
    StoreOfflinePageDismissedIDsToPrefs(dismissed_ids);
  } else {
    StoreAssetDismissedIDsToPrefs(dismissed_ids);
  }
}

void DownloadSuggestionsProvider::UnregisterDownloadItemObservers() {
  DCHECK_NE(download_manager_, nullptr);

  std::vector<DownloadItem*> all_downloads;
  download_manager_->GetAllDownloads(&all_downloads);

  for (DownloadItem* item : all_downloads) {
    item->RemoveObserver(this);
  }
}
