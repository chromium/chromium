// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DownloadHistory manages persisting DownloadItems to the history service by
// observing a single DownloadManager and all its DownloadItems using an
// AllDownloadItemNotifier.
//
// DownloadHistory decides whether and when to add items to, remove items from,
// and update items in the database. DownloadHistory uses DownloadHistoryData to
// store per-DownloadItem data such as whether the item is persisted or being
// persisted, and the last history::DownloadRow that was passed to the database.
// When the DownloadManager and its delegate (ChromeDownloadManagerDelegate) are
// initialized, DownloadHistory is created and queries the HistoryService. When
// the HistoryService calls back from QueryDownloads() to QueryCallback(),
// DownloadHistory will then wait for DownloadManager to call
// LoadHistoryDownloads(), and uses DownloadManager::CreateDownloadItem() to
// inform DownloadManager of these persisted DownloadItems. CreateDownloadItem()
// internally calls OnDownloadCreated(), which normally adds items to the
// database, so LoadHistoryDownloads() uses |loading_id_| to disable adding
// these items to the database.  If a download is removed via
// OnDownloadRemoved() while the item is still being added to the database,
// DownloadHistory uses |removed_while_adding_| to remember to remove the item
// when its ItemAdded() callback is called.  All callbacks are bound with a weak
// pointer to DownloadHistory to prevent use-after-free bugs.
// ChromeDownloadManagerDelegate owns DownloadHistory, and deletes it in
// Shutdown(), which is called by DownloadManagerImpl::Shutdown() after all
// DownloadItems are destroyed.

#include "chrome/browser/download/download_history.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_path_reservation_tracker.h"
#include "components/download/public/common/download_utils.h"
#include "components/history/content/browser/download_conversions.h"
#include "components/history/core/browser/download_database.h"
#include "components/history/core/browser/download_row.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/storage_partition_config.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/origin.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/api/downloads/downloads_api.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/download/download_item_web_app_data.h"
#endif

using history::DownloadState;

namespace {



// Per-DownloadItem data. This information does not belong inside DownloadItem,
// and keeping maps in DownloadHistory from DownloadItem to this information is
// error-prone and complicated. Unfortunately, DownloadHistory::removing_*_ and
// removed_while_adding_ cannot be moved into this class partly because
// DownloadHistoryData is destroyed when DownloadItems are destroyed, and we
// have no control over when DownloadItems are destroyed.
class DownloadHistoryData : public base::SupportsUserData::Data {
 public:
  enum PersistenceState {
    NOT_PERSISTED,
    PERSISTING,
    PERSISTED,
  };

  static DownloadHistoryData* Get(download::DownloadItem* item) {
    base::SupportsUserData::Data* data = item->GetUserData(kKey);
    return static_cast<DownloadHistoryData*>(data);
  }

  static const DownloadHistoryData* Get(const download::DownloadItem* item) {
    const base::SupportsUserData::Data* data = item->GetUserData(kKey);
    return static_cast<const DownloadHistoryData*>(data);
  }

  explicit DownloadHistoryData(download::DownloadItem* item) {
    item->SetUserData(kKey, base::WrapUnique(this));
  }

  DownloadHistoryData(const DownloadHistoryData&) = delete;
  DownloadHistoryData& operator=(const DownloadHistoryData&) = delete;

  ~DownloadHistoryData() override = default;

  PersistenceState state() const { return state_; }
  void SetState(PersistenceState s) { state_ = s; }

  // This allows DownloadHistory::OnDownloadUpdated() to see what changed in a
  // DownloadItem if anything, in order to prevent writing to the database
  // unnecessarily.  It is nullified when the item is no longer in progress in
  // order to save memory.
  history::DownloadRow* info() { return info_.get(); }
  void set_info(const history::DownloadRow& i) {
    if (info_) {
      *info_ = i;
    } else {
      info_ = std::make_unique<history::DownloadRow>(i);
    }
  }
  void clear_info() {
    info_.reset();
  }

 private:
  static const char kKey[];

  PersistenceState state_ = NOT_PERSISTED;
  std::unique_ptr<history::DownloadRow> info_;
};

const char DownloadHistoryData::kKey[] =
  "DownloadItem DownloadHistoryData";

history::DownloadRow GetDownloadRow(download::DownloadItem* item) {
  std::string by_ext_id, by_ext_name;
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  extensions::DownloadedByExtension* by_ext =
      extensions::DownloadedByExtension::Get(item);
  if (by_ext) {
    by_ext_id = by_ext->id();
    by_ext_name = by_ext->name();
  }
#endif

  history::DownloadRow download;
  download.current_path = item->GetFullPath();
  download.target_path = item->GetTargetFilePath();
  download.url_chain = item->GetUrlChain();
  download.referrer_url = item->GetReferrerUrl();
  download.embedder_download_data = item->GetSerializedEmbedderDownloadData();
  download.tab_url = item->GetTabUrl();
  download.tab_referrer_url = item->GetTabReferrerUrl();
  download.http_method = std::string();  // HTTP method not available yet.
  download.mime_type = item->GetMimeType();
  download.original_mime_type = item->GetOriginalMimeType();
  download.start_time = item->GetStartTime();
  download.end_time = item->GetEndTime();
  download.etag = item->GetETag();
  download.last_modified = item->GetLastModifiedTime();
  download.received_bytes = item->GetReceivedBytes();
  download.total_bytes = item->GetTotalBytes();
  download.state = history::ToHistoryDownloadState(item->GetState());
  download.danger_type =
      history::ToHistoryDownloadDangerType(item->GetDangerType());
  download.interrupt_reason =
      history::ToHistoryDownloadInterruptReason(item->GetLastReason());
  download.hash = std::string();  // Hash value not available yet.
  download.id = history::ToHistoryDownloadId(item->GetId());
  download.guid = item->GetGuid();
  download.opened = item->GetOpened();
  download.last_access_time = item->GetLastAccessTime();
  download.transient = item->IsTransient();
  download.by_ext_id = by_ext_id;
  download.by_ext_name = by_ext_name;
#if !BUILDFLAG(IS_ANDROID)
  if (DownloadItemWebAppData* web_app_data = DownloadItemWebAppData::Get(item);
      web_app_data != nullptr) {
    download.by_web_app_id = web_app_data->id();
  }
#endif
  download.download_slice_info = history::GetHistoryDownloadSliceInfos(*item);
  download::TruncateDataUrlAtTheEndIfNeeded(&download.url_chain);
  return download;
}

enum class ShouldUpdateHistoryResult {
  NO_UPDATE,
  UPDATE,
  UPDATE_IMMEDIATELY,
};

ShouldUpdateHistoryResult ShouldUpdateHistory(
    const history::DownloadRow* previous,
    const history::DownloadRow& current) {
  // When download path is determined, Chrome should commit the history
  // immediately. Otherwise the file will be left permanently on the external
  // storage if Chrome crashes right away.
  // TODO(qinmin): this doesn't solve all the issues. When download starts,
  // Chrome will write the http response data to a temporary file, and later
  // rename it. If Chrome is killed before committing the history here,
  // that temporary file will still get permanently left.
  // See http://crbug.com/40493321.
  if (previous == nullptr || previous->current_path != current.current_path) {
    return ShouldUpdateHistoryResult::UPDATE_IMMEDIATELY;
  }

  // Ignore url_chain, referrer, site_url, http_method, mime_type,
  // original_mime_type, start_time, id, and guid. These fields don't change.
  if ((previous->target_path != current.target_path) ||
      (previous->end_time != current.end_time) ||
      (previous->received_bytes != current.received_bytes) ||
      (previous->total_bytes != current.total_bytes) ||
      (previous->etag != current.etag) ||
      (previous->last_modified != current.last_modified) ||
      (previous->state != current.state) ||
      (previous->danger_type != current.danger_type) ||
      (previous->interrupt_reason != current.interrupt_reason) ||
      (previous->hash != current.hash) ||
      (previous->opened != current.opened) ||
      (previous->last_access_time != current.last_access_time) ||
      (previous->transient != current.transient) ||
      (previous->by_ext_id != current.by_ext_id) ||
      (previous->by_ext_name != current.by_ext_name) ||
      (previous->by_web_app_id != current.by_web_app_id) ||
      (previous->download_slice_info != current.download_slice_info)) {
    return ShouldUpdateHistoryResult::UPDATE;
  }

  return ShouldUpdateHistoryResult::NO_UPDATE;
}

// Counts how many times a target file path exists in `rows` and stores
// the result into `file_path_count`.
void CountFilePathOccurences(
    const std::vector<history::DownloadRow>& rows,
    absl::flat_hash_map<base::FilePath::StringViewType, int>* file_path_count) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);

  if (!base::FeatureList::IsEnabled(
          download::features::kDeleteOverwrittenDownloads)) {
    return;
  }

  file_path_count->reserve(rows.size());
  for (const history::DownloadRow& row : rows) {
    if (row.state != DownloadState::COMPLETE || row.target_path.empty())
      continue;
    ++(*file_path_count)[row.target_path.value()];
  }
}

// Checks whether a particular download row should be skipped from loading given
// the number of times the same target file path appears in `file_path_count`.
bool ShouldSkipLoadingDownload(
    const history::DownloadRow& row,
    absl::flat_hash_map<base::FilePath::StringViewType, int>* file_path_count) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);

  if (!base::FeatureList::IsEnabled(
          download::features::kDeleteOverwrittenDownloads)) {
    return false;
  }

  if (row.state != DownloadState::COMPLETE || row.target_path.empty())
    return false;
  auto iter = file_path_count->find(row.target_path.value());
  CHECK(iter != file_path_count->end());
  --iter->second;
  if (iter->second < 1)
    return false;
  return base::Time::Now() - row.end_time >=
         download::GetOverwrittenDownloadDeleteTime();
}

// Minimum number of loaded downloads in a time window required to emit
// Download.History.HeavyProfile.* histograms for that window.
size_t g_heavy_profile_download_count_threshold = 1000;

// Returns the first valid non-opaque origin among `row.tab_url`,
// `row.referrer_url`, `row.site_url`, and `row.url_chain`.
std::optional<url::Origin> GetDownloadOrigin(const history::DownloadRow& row) {
  for (const GURL& url : {row.tab_url, row.referrer_url, row.site_url}) {
    url::Origin origin = url::Origin::Create(url);
    if (!origin.opaque()) {
      return origin;
    }
  }
  for (const GURL& url : row.url_chain) {
    url::Origin origin = url::Origin::Create(url);
    if (!origin.opaque()) {
      return origin;
    }
  }
  return std::nullopt;
}

// Records Download.History.HeavyProfile.*{window_suffix} histograms for `rows`.
void RecordHeavyProfileMetrics(
    base::span<const history::DownloadRow* const> rows,
    std::string_view window_suffix) {
  CHECK_GE(rows.size(), g_heavy_profile_download_count_threshold);

  size_t num_by_extension = 0;
  size_t num_by_web_app = 0;
  size_t num_blob_or_data_url = 0;
  size_t num_incomplete = 0;
  size_t num_opened = 0;
  size_t num_uniquified_duplicates = 0;

  absl::flat_hash_map<std::string, size_t> origin_counts;
  size_t max_origin_count = 0;

  absl::flat_hash_set<std::string> unique_stripped_paths;

  absl::flat_hash_map<int64_t, size_t> downloads_per_day;
  size_t max_downloads_in_a_day = 0;

  for (const history::DownloadRow* row : rows) {
    if (!row->by_ext_id.empty()) {
      ++num_by_extension;
    }
    if (!row->by_web_app_id.empty()) {
      ++num_by_web_app;
    }
    if (!row->url_chain.empty() &&
        (row->url_chain.back().SchemeIsBlob() ||
         row->url_chain.back().SchemeIs(url::kDataScheme))) {
      ++num_blob_or_data_url;
    }
    if (row->state != DownloadState::COMPLETE) {
      ++num_incomplete;
    }
    if (row->opened) {
      ++num_opened;
    }
    if (!row->target_path.empty() &&
        !unique_stripped_paths
             .insert(
                 DownloadHistory::GetPathWithoutUniquifier(row->target_path))
             .second) {
      ++num_uniquified_duplicates;
    }
    if (std::optional<url::Origin> origin = GetDownloadOrigin(*row)) {
      max_origin_count =
          std::max(max_origin_count, ++origin_counts[origin->Serialize()]);
    }
    const int64_t day_index =
        row->start_time.ToDeltaSinceWindowsEpoch().InDaysFloored();
    max_downloads_in_a_day =
        std::max(max_downloads_in_a_day, ++downloads_per_day[day_index]);
  }

  auto record_percentage = [&](std::string_view characteristic, size_t count) {
    base::UmaHistogramPercentage(
        base::StrCat({"Download.History.HeavyProfile.Percent", characteristic,
                      window_suffix}),
        base::checked_cast<int>(count * 100 / rows.size()));
  };

  record_percentage("BlobOrDataUrl", num_blob_or_data_url);
  record_percentage("ByExtension", num_by_extension);
  record_percentage("ByWebApp", num_by_web_app);
  record_percentage("FromTopOrigin", max_origin_count);
  record_percentage("Incomplete", num_incomplete);
  record_percentage("InPeakDay", max_downloads_in_a_day);
  record_percentage("Opened", num_opened);
  record_percentage("UniquifiedDuplicates", num_uniquified_duplicates);

  base::UmaHistogramCounts100000(
      base::StrCat(
          {"Download.History.HeavyProfile.MaxDownloadsPerDay", window_suffix}),
      base::saturated_cast<int>(max_downloads_in_a_day));
}

}  // anonymous namespace

DownloadHistory::HistoryAdapter::HistoryAdapter(
    history::HistoryService* history)
    : history_(history) {
}
DownloadHistory::HistoryAdapter::~HistoryAdapter() = default;

void DownloadHistory::HistoryAdapter::QueryDownloads(
    history::HistoryService::DownloadQueryCallback callback) {
  history_->QueryDownloads(std::move(callback));
}

void DownloadHistory::HistoryAdapter::CreateDownload(
    const history::DownloadRow& info,
    history::HistoryService::DownloadCreateCallback callback) {
  history_->CreateDownload(info, std::move(callback));
}

void DownloadHistory::HistoryAdapter::UpdateDownload(
    const history::DownloadRow& data, bool should_commit_immediately) {
  history_->UpdateDownload(data, should_commit_immediately);
}

void DownloadHistory::HistoryAdapter::RemoveDownloads(
    const std::set<uint32_t>& ids) {
  history_->RemoveDownloads(ids);
}

DownloadHistory::Observer::Observer() = default;
DownloadHistory::Observer::~Observer() = default;

// static
bool DownloadHistory::IsPersisted(const download::DownloadItem* item) {
  const DownloadHistoryData* data = DownloadHistoryData::Get(item);
  return data && (data->state() == DownloadHistoryData::PERSISTED);
}

// static
std::string DownloadHistory::GetPathWithoutUniquifier(
    const base::FilePath& path) {
  static_assert(download::DownloadPathReservationTracker::kMaxUniqueFiles ==
                100);
  static const base::NoDestructor<re2::RE2> kUniquifierRegex(
      R"((?: \((?:[1-9]\d?|100)\)| - \d{4}-\d{2}-\d{2}T\d{6}\.\d{3}))"
      R"((\.[^/\\]*)?$)");
  std::string result = path.AsUTF8Unsafe();
  re2::RE2::Replace(&result, *kUniquifierRegex, R"(\1)");
  return result;
}

// static
base::AutoReset<size_t> DownloadHistory::SetHeavyProfileThresholdForTesting(
    size_t threshold) {
  return base::AutoReset<size_t>(&g_heavy_profile_download_count_threshold,
                                 threshold);
}

DownloadHistory::DownloadHistory(content::DownloadManager* manager,
                                 std::unique_ptr<HistoryAdapter> history)
    : notifier_(manager, this),
      history_(std::move(history)),
      loading_id_(download::DownloadItem::kInvalidId),
      initial_history_query_complete_(false) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  download::SimpleDownloadManager::DownloadVector items;
  notifier_.GetManager()->GetAllDownloads(&items);
  for (download::DownloadItem* item : items) {
    OnDownloadCreated(notifier_.GetManager(), item);
  }
  history_->QueryDownloads(base::BindOnce(&DownloadHistory::QueryCallback,
                                          weak_ptr_factory_.GetWeakPtr()));
}

DownloadHistory::~DownloadHistory() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  for (Observer& observer : observers_)
    observer.OnDownloadHistoryDestroyed();
  observers_.Clear();
}

void DownloadHistory::AddObserver(DownloadHistory::Observer* observer) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  observers_.AddObserver(observer);

  if (initial_history_query_complete_)
    observer->OnHistoryQueryComplete();
}

void DownloadHistory::RemoveObserver(DownloadHistory::Observer* observer) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  observers_.RemoveObserver(observer);
}

void DownloadHistory::QueryCallback(std::vector<history::DownloadRow> rows) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  // ManagerGoingDown() may have happened before the history loaded.
  if (!notifier_.GetManager())
    return;

#if BUILDFLAG(IS_ANDROID)
  base::OnceClosure callback =
      base::FeatureList::IsEnabled(
          download::features::kRetrieveDisplayNamesForHistoryDownloads)
          ? base::BindOnce(
                &DownloadHistory::RetrieveDisplayNamesAndLoadHistoryDownloads,
                weak_ptr_factory_.GetWeakPtr(), std::move(rows))
          : base::BindOnce(&DownloadHistory::LoadHistoryDownloads,
                           weak_ptr_factory_.GetWeakPtr(), std::move(rows),
                           nullptr);
#else
  base::OnceClosure callback =
      base::BindOnce(&DownloadHistory::LoadHistoryDownloads,
                     weak_ptr_factory_.GetWeakPtr(), std::move(rows));
#endif
  notifier_.GetManager()->OnHistoryQueryComplete(std::move(callback));
}

#if BUILDFLAG(IS_ANDROID)
void DownloadHistory::RetrieveDisplayNamesAndLoadHistoryDownloads(
    std::vector<history::DownloadRow> rows) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  if (!notifier_.GetManager()) {
    return;
  }
  std::vector<base::FilePath> content_uris;
  for (const history::DownloadRow& row : rows) {
    if (row.target_path.IsContentUri()) {
      content_uris.push_back(row.target_path);
    }
  }
  download::GetDisplayNamesForDownloads(
      std::move(content_uris),
      base::BindOnce(&DownloadHistory::LoadHistoryDownloads,
                     weak_ptr_factory_.GetWeakPtr(), std::move(rows)));
}

void DownloadHistory::LoadHistoryDownloads(
    const std::vector<history::DownloadRow>& rows,
    download::DisplayNames display_names) {
#else
void DownloadHistory::LoadHistoryDownloads(
    const std::vector<history::DownloadRow>& rows) {
#endif
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  if (!notifier_.GetManager()) {
    return;
  }

  absl::flat_hash_map<base::FilePath::StringViewType, int> file_name_count;
  CountFilePathOccurences(rows, &file_name_count);

  // Number of downloads loaded, in total and started in the last 30/90 days.
  const base::Time now = base::Time::Now();
  int num_loaded = 0;
  int num_loaded_last_30_days = 0;
  int num_loaded_last_90_days = 0;
  const bool may_be_heavy_profile =
      rows.size() >= g_heavy_profile_download_count_threshold;
  std::vector<const history::DownloadRow*> loaded_rows;
  std::vector<const history::DownloadRow*> loaded_rows_last_30_days;
  std::vector<const history::DownloadRow*> loaded_rows_last_90_days;
  if (may_be_heavy_profile) {
    loaded_rows.reserve(rows.size());
  }

  // Completed downloads started before this time are removed from history
  // instead of being loaded.
  std::optional<base::Time> remove_completed_downloads_started_before;
  if (base::FeatureList::IsEnabled(
          download::features::kRemoveOldDownloadsFromHistory)) {
    remove_completed_downloads_started_before =
        now - download::features::kRemoveOldDownloadsFromHistoryMinAge.Get();
  }

  for (const history::DownloadRow& row : rows) {
    if (ShouldSkipLoadingDownload(row, &file_name_count)) {
      ScheduleRemoveDownload(row.id);
      continue;
    }

    // Checked after ShouldSkipLoadingDownload(), which must see every row to
    // keep `file_name_count` accurate. Only completed downloads are removed
    // here: DownloadManager already clears old cancelled and interrupted
    // downloads, and deletes their partial files.
    if (remove_completed_downloads_started_before.has_value() &&
        row.state == DownloadState::COMPLETE &&
        row.start_time < *remove_completed_downloads_started_before) {
      ScheduleRemoveDownload(row.id);
      continue;
    }

    base::FilePath display_name;
#if BUILDFLAG(IS_ANDROID)
    if (base::FeatureList::IsEnabled(
            download::features::kRetrieveDisplayNamesForHistoryDownloads) &&
        row.target_path.IsContentUri() && display_names) {
      auto iter = display_names->find(row.target_path.value());
      if (iter != display_names->end()) {
        display_name = iter->second;
      }
    }
#endif

    loading_id_ = history::ToContentDownloadId(row.id);
    download::DownloadItem::DownloadState history_download_state =
        history::ToContentDownloadState(row.state);
    download::DownloadInterruptReason history_reason =
        history::ToContentDownloadInterruptReason(row.interrupt_reason);
    std::vector<GURL> url_chain = row.url_chain;
    download::TruncateDataUrlAtTheEndIfNeeded(&url_chain);

    // If the serialized EmbedderDownloadData is not present in DownloadRow,
    // use the site URL to grab the appropriate StoragePartitionConfig to use
    // to create the DownloadItem. Since DownloadRow comes from the download
    // history database, it may contain entries that still use site URL.
    content::StoragePartitionConfig storage_partition_config;
    if (row.embedder_download_data.empty()) {
      storage_partition_config =
          notifier_.GetManager()->GetStoragePartitionConfigForSiteUrl(
              row.site_url);
    } else {
      storage_partition_config =
          notifier_.GetManager()
              ->SerializedEmbedderDownloadDataToStoragePartitionConfig(
                  row.embedder_download_data);
    }
    download::DownloadItem* item = notifier_.GetManager()->CreateDownloadItem(
        row.guid, loading_id_, row.current_path, row.target_path, url_chain,
        row.referrer_url, storage_partition_config, row.tab_url,
        row.tab_referrer_url, std::nullopt, row.mime_type,
        row.original_mime_type, row.start_time, row.end_time, row.etag,
        row.last_modified, row.received_bytes, row.total_bytes,
        std::string(),  // TODO(asanka): Need to persist and restore hash of
                        // partial file for an interrupted download. No need to
                        // store hash for a completed file.
        history_download_state,
        history::ToContentDownloadDangerType(row.danger_type), history_reason,
        row.opened, row.last_access_time, row.transient,
        history::ToContentReceivedSlices(row.download_slice_info),
        display_name);
    // DownloadManager returns a nullptr if it decides to remove the download
    // permanently.
    if (item == nullptr) {
      ScheduleRemoveDownload(row.id);
      continue;
    }
    CHECK_EQ(download::DownloadItem::kInvalidId, loading_id_,
             base::NotFatalUntil::M161);

    ++num_loaded;
    if (may_be_heavy_profile) {
      loaded_rows.push_back(&row);
    }
    if (now - row.start_time <= base::Days(30)) {
      ++num_loaded_last_30_days;
      if (may_be_heavy_profile) {
        loaded_rows_last_30_days.push_back(&row);
      }
    }
    if (now - row.start_time <= base::Days(90)) {
      ++num_loaded_last_90_days;
      if (may_be_heavy_profile) {
        loaded_rows_last_90_days.push_back(&row);
      }
    }

    // The download might have been in the terminal state without informing
    // history DB. If this is the case, populate the new state back to history
    // DB.
    if (item->IsDone() &&
        !download::IsDownloadDone(item->GetURL(), history_download_state,
                                  history_reason)) {
      OnDownloadUpdated(notifier_.GetManager(), item);
    }

    // The item was created already and observers were notified of its creation
    // via OnDownloadCreated(). Since we are about to possibly add extra info to
    // it (for extensions and web apps), we must notify observers again, after
    // modification, so that observers who care about the extra info may have an
    // updated view of the item.
    bool should_update_observers = false;
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    if (!row.by_ext_id.empty() && !row.by_ext_name.empty()) {
      new extensions::DownloadedByExtension(item, row.by_ext_id,
                                            row.by_ext_name);
      should_update_observers = true;
    }
#endif
#if !BUILDFLAG(IS_ANDROID)
    if (!row.by_web_app_id.empty()) {
      DownloadItemWebAppData::CreateAndAttachToItem(item, row.by_web_app_id);
      should_update_observers = true;
    }
#endif
    if (should_update_observers) {
      item->UpdateObservers();
    }

    CHECK_EQ(DownloadHistoryData::PERSISTED,
             DownloadHistoryData::Get(item)->state(),
             base::NotFatalUntil::M161);
  }

  // Indicate that the history db is initialized.
  notifier_.GetManager()->PostInitialization(
      content::DownloadManager::DOWNLOAD_INITIALIZATION_DEPENDENCY_HISTORY_DB);

  base::UmaHistogramCustomTimes("Download.History.LoadLatency",
                                base::TimeTicks::Now() - load_start_time_,
                                base::Milliseconds(1), base::Minutes(1), 50);
  base::UmaHistogramCounts100000("Download.History.DownloadCount",
                                 base::saturated_cast<int>(rows.size()));
  base::UmaHistogramCounts100000("Download.History.DownloadCount.PostRemoval",
                                 num_loaded);
  base::UmaHistogramCounts100000(
      "Download.History.DownloadCount.PostRemoval.Last30Days",
      num_loaded_last_30_days);
  base::UmaHistogramCounts100000(
      "Download.History.DownloadCount.PostRemoval.Last90Days",
      num_loaded_last_90_days);
  if (loaded_rows.size() >= g_heavy_profile_download_count_threshold) {
    RecordHeavyProfileMetrics(loaded_rows, "");
  }
  if (loaded_rows_last_30_days.size() >=
      g_heavy_profile_download_count_threshold) {
    RecordHeavyProfileMetrics(loaded_rows_last_30_days, ".Last30Days");
  }
  if (loaded_rows_last_90_days.size() >=
      g_heavy_profile_download_count_threshold) {
    RecordHeavyProfileMetrics(loaded_rows_last_90_days, ".Last90Days");
  }

  initial_history_query_complete_ = true;
  for (Observer& observer : observers_) {
    observer.OnHistoryQueryComplete();
  }
}

void DownloadHistory::MaybeAddToHistory(download::DownloadItem* item) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);

  if (!NeedToUpdateDownloadHistory(item))
    return;

  uint32_t download_id = item->GetId();
  DownloadHistoryData* data = DownloadHistoryData::Get(item);
  bool removing = removing_ids_.find(download_id) != removing_ids_.end();

  // TODO(benjhayden): Remove IsTemporary().
  if ((notifier_.GetManager() &&
       download_crx_util::IsTrustedExtensionDownload(
           Profile::FromBrowserContext(
               notifier_.GetManager()->GetBrowserContext()),
           *item)) ||
      item->IsTemporary() ||
      (data->state() != DownloadHistoryData::NOT_PERSISTED) || removing) {
    return;
  }

  data->SetState(DownloadHistoryData::PERSISTING);
  // Keep the info for in-progress download, so we can check whether history DB
  // update is needed when DownloadUpdated() is called.
  history::DownloadRow download_row = GetDownloadRow(item);
  if (item->GetState() == download::DownloadItem::IN_PROGRESS)
    data->set_info(download_row);
  else
    data->clear_info();
  history_->CreateDownload(
      download_row, base::BindOnce(&DownloadHistory::ItemAdded,
                                   weak_ptr_factory_.GetWeakPtr(), download_id,
                                   download_row));
}

void DownloadHistory::ItemAdded(uint32_t download_id,
                                const history::DownloadRow& download_row,
                                bool success) {
  if (removed_while_adding_.find(download_id) !=
      removed_while_adding_.end()) {
    removed_while_adding_.erase(download_id);
    if (success)
      ScheduleRemoveDownload(download_id);
    return;
  }

  if (!notifier_.GetManager())
    return;

  download::DownloadItem* item =
      notifier_.GetManager()->GetDownload(download_id);
  if (!item) {
    // This item will have called OnDownloadDestroyed().  If the item should
    // have been removed from history, then it would have also called
    // OnDownloadRemoved(), which would have put |download_id| in
    // removed_while_adding_, handled above.
    return;
  }

  DownloadHistoryData* data = DownloadHistoryData::Get(item);
  bool was_persisted = IsPersisted(item);

  // The sql INSERT statement failed. Avoid an infinite loop: don't
  // automatically retry. Retry adding the next time the item is updated by
  // resetting the state to NOT_PERSISTED.
  if (!success) {
    DVLOG(20) << __func__ << " INSERT failed id=" << download_id;
    data->SetState(DownloadHistoryData::NOT_PERSISTED);
    return;
  }
  data->SetState(DownloadHistoryData::PERSISTED);

  // Notify the observer about the change in the persistence state.
  if (was_persisted != IsPersisted(item)) {
    for (Observer& observer : observers_)
      observer.OnDownloadStored(item, download_row);
  }
}

void DownloadHistory::OnDownloadCreated(content::DownloadManager* manager,
                                        download::DownloadItem* item) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);

  // All downloads should pass through OnDownloadCreated exactly once.
  CHECK(!DownloadHistoryData::Get(item));
  DownloadHistoryData* data = new DownloadHistoryData(item);
  if (item->GetId() == loading_id_)
    OnDownloadRestoredFromHistory(item);
  if (item->GetState() == download::DownloadItem::IN_PROGRESS &&
      NeedToUpdateDownloadHistory(item)) {
    data->set_info(GetDownloadRow(item));
  }
  MaybeAddToHistory(item);
}

void DownloadHistory::OnDownloadUpdated(content::DownloadManager* manager,
                                        download::DownloadItem* item) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  DownloadHistoryData* data = DownloadHistoryData::Get(item);
  if (data->state() == DownloadHistoryData::NOT_PERSISTED) {
    MaybeAddToHistory(item);
    return;
  }
  if (item->IsTemporary()) {
    OnDownloadRemoved(notifier_.GetManager(), item);
    return;
  }
  if (!NeedToUpdateDownloadHistory(item))
    return;

  history::DownloadRow current_info(GetDownloadRow(item));
  ShouldUpdateHistoryResult should_update_result =
      ShouldUpdateHistory(data->info(), current_info);
  bool should_update =
      (should_update_result != ShouldUpdateHistoryResult::NO_UPDATE);
  if (should_update) {
    history_->UpdateDownload(
        current_info,
        should_update_result == ShouldUpdateHistoryResult::UPDATE_IMMEDIATELY);
    for (Observer& observer : observers_)
      observer.OnDownloadStored(item, current_info);
  }
  if (item->GetState() == download::DownloadItem::IN_PROGRESS) {
    data->set_info(current_info);
  } else {
    data->clear_info();
  }
}

void DownloadHistory::OnDownloadOpened(content::DownloadManager* manager,
                                       download::DownloadItem* item) {
  OnDownloadUpdated(manager, item);
}

void DownloadHistory::OnDownloadRemoved(content::DownloadManager* manager,
                                        download::DownloadItem* item) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);

  DownloadHistoryData* data = DownloadHistoryData::Get(item);
  if (data->state() != DownloadHistoryData::PERSISTED) {
    if (data->state() == DownloadHistoryData::PERSISTING) {
      // ScheduleRemoveDownload will be called when history_ calls ItemAdded().
      removed_while_adding_.insert(item->GetId());
    }
    return;
  }
  ScheduleRemoveDownload(item->GetId());
  // This is important: another OnDownloadRemoved() handler could do something
  // that synchronously fires an OnDownloadUpdated().
  data->SetState(DownloadHistoryData::NOT_PERSISTED);
}

void DownloadHistory::ScheduleRemoveDownload(uint32_t download_id) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);

  // For database efficiency, batch removals together if they happen all at
  // once.
  if (removing_ids_.empty()) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&DownloadHistory::RemoveDownloadsBatch,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
  removing_ids_.insert(download_id);
}

void DownloadHistory::RemoveDownloadsBatch() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  IdSet remove_ids;
  removing_ids_.swap(remove_ids);
  history_->RemoveDownloads(remove_ids);
  for (Observer& observer : observers_)
    observer.OnDownloadsRemoved(remove_ids);
}

void DownloadHistory::OnDownloadRestoredFromHistory(
    download::DownloadItem* item) {
  DownloadHistoryData* data = DownloadHistoryData::Get(item);
  data->SetState(DownloadHistoryData::PERSISTED);
  loading_id_ = download::DownloadItem::kInvalidId;
}

bool DownloadHistory::NeedToUpdateDownloadHistory(
    download::DownloadItem* item) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // Always populate new extension downloads to history.
  DownloadHistoryData* data = DownloadHistoryData::Get(item);
  extensions::DownloadedByExtension* by_ext =
      extensions::DownloadedByExtension::Get(item);
  if (by_ext && !by_ext->id().empty() && !by_ext->name().empty() &&
      data->state() != DownloadHistoryData::NOT_PERSISTED) {
    return true;
  }
#endif

  // When download DB is enabled, only downloads that are in terminal state
  // are added to or updated in history DB. Non-transient in-progress and
  // interrupted download will be stored in the in-progress DB.
  return !item->IsTransient() &&
         (item->IsSavePackageDownload() || item->IsDone());
}
