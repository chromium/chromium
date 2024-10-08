// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnail/cc/thumbnail_cache.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "base/android/application_status_listener.h"
#include "base/android/path_utils.h"
#include "base/big_endian.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/config/gpu_finch_features.h"
#include "skia/ext/image_operations.h"
#include "third_party/android_opengl/etc1/etc1.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "ui/android/resources/ui_resource_provider.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace thumbnail {
namespace {

constexpr base::TimeDelta kDefaultCaptureMinRequestTimeMs(
    base::Milliseconds(1000));

constexpr int kKiB = 1024;

// Indicates whether we prefer to have more free CPU memory over GPU memory.
constexpr bool kPreferCPUMemory = true;

// Borrowed from GetDelayForNextMemoryLog() in browser_metrics.cc.
//
// A Poisson distributed delay with a mean of `mean_time` for computing time
// delta between recording memory metrics.
base::TimeDelta ComputeDelay(base::TimeDelta mean_time) {
  double uniform = base::RandDouble();
  return -std::log(1 - uniform) * mean_time;
}

}  // anonymous namespace

ThumbnailCache::ThumbnailCache(size_t default_cache_size,
                               size_t compression_queue_max_size,
                               size_t write_queue_max_size,
                               bool save_jpeg_thumbnails)
    : etc1_file_sequenced_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      jpeg_file_sequenced_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
      etc1_helper_(GetCacheDirectory(), etc1_file_sequenced_task_runner_),
      jpeg_helper_(GetCacheDirectory(), jpeg_file_sequenced_task_runner_),
      compression_queue_max_size_(compression_queue_max_size),
      write_queue_max_size_(write_queue_max_size),
      save_jpeg_thumbnails_(save_jpeg_thumbnails),
      capture_min_request_time_ms_(kDefaultCaptureMinRequestTimeMs),
      compression_tasks_count_(0),
      write_tasks_count_(0),
      read_in_progress_(false),
      cache_(default_cache_size),
      ui_resource_provider_(nullptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  memory_pressure_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE, base::BindRepeating(&ThumbnailCache::OnMemoryPressure,
                                     base::Unretained(this)));
  ScheduleRecordCacheMetrics(base::Minutes(1));
}

ThumbnailCache::~ThumbnailCache() {
  SetUIResourceProvider(nullptr);
}

void ThumbnailCache::SetUIResourceProvider(
    base::WeakPtr<ui::UIResourceProvider> ui_resource_provider) {
  if (ui_resource_provider_.get() == ui_resource_provider.get()) {
    return;
  }

  cache_.Clear();

  ui_resource_provider_ = ui_resource_provider;
}

void ThumbnailCache::AddThumbnailCacheObserver(
    ThumbnailCacheObserver* observer) {
  if (!observers_.HasObserver(observer)) {
    observers_.AddObserver(observer);
  }
}

void ThumbnailCache::RemoveThumbnailCacheObserver(
    ThumbnailCacheObserver* observer) {
  if (observers_.HasObserver(observer)) {
    observers_.RemoveObserver(observer);
  }
}

void ThumbnailCache::Put(
    TabId tab_id,
    std::unique_ptr<ThumbnailCaptureTracker, base::OnTaskRunnerDeleter> tracker,
    const SkBitmap& bitmap,
    float thumbnail_scale) {
  if (!ui_resource_provider_ || bitmap.empty() || thumbnail_scale <= 0) {
    tracker->MarkCaptureFailed();
    return;
  }

  if (thumbnail_meta_data_.find(tab_id) == thumbnail_meta_data_.end()) {
    DVLOG(1) << "Thumbnail meta data was removed for tab id " << tab_id;
    tracker->MarkCaptureFailed();
    return;
  }

  base::Time time_stamp = thumbnail_meta_data_[tab_id].capture_time();
  std::unique_ptr<Thumbnail> thumbnail = Thumbnail::Create(
      tab_id, time_stamp, thumbnail_scale, ui_resource_provider_, this);
  thumbnail->SetBitmap(bitmap);

  RemoveFromReadQueue(tab_id);
  if (base::Contains(visible_ids_, tab_id)) {
    MakeSpaceForNewItemIfNecessary(tab_id);
    cache_.Put(tab_id, std::move(thumbnail));
    NotifyObserversOfThumbnailAddedToCache(tab_id);
  }

  CompressThumbnailIfNecessary(tab_id, std::move(tracker), time_stamp, bitmap,
                               thumbnail_scale);
}

void ThumbnailCache::Remove(TabId tab_id) {
  cache_.Remove(tab_id);
  thumbnail_meta_data_.erase(tab_id);
  RemoveFromDisk(tab_id);
  RemoveFromReadQueue(tab_id);
}

Thumbnail* ThumbnailCache::Get(TabId tab_id, bool force_disk_read) {
  Thumbnail* thumbnail = cache_.Get(tab_id);
  if (thumbnail) {
    thumbnail->CreateUIResource();
    return thumbnail;
  }

  if (force_disk_read && primary_tab_id_ != tab_id &&
      base::Contains(visible_ids_, tab_id) &&
      !base::Contains(read_queue_, tab_id)) {
    read_queue_.push_back(tab_id);
    ReadNextThumbnail();
  }

  return nullptr;
}

void ThumbnailCache::InvalidateThumbnailIfChanged(TabId tab_id,
                                                  const GURL& url) {
  auto meta_data_iter = thumbnail_meta_data_.find(tab_id);
  if (meta_data_iter == thumbnail_meta_data_.end()) {
    thumbnail_meta_data_[tab_id] = ThumbnailMetaData(base::Time(), url);
  } else if (!url.is_empty() && meta_data_iter->second.url() != url) {
    Remove(tab_id);
  }
}

base::FilePath ThumbnailCache::GetCacheDirectory() {
  static const base::NoDestructor<base::FilePath> cache_dir([] {
    base::FilePath path;
    base::android::GetThumbnailCacheDirectory(&path);
    return path;
  }());
  return *cache_dir;
}

bool ThumbnailCache::CheckAndUpdateThumbnailMetaData(TabId tab_id,
                                                     const GURL& url,
                                                     bool force_update) {
  base::Time current_time = base::Time::Now();
  auto meta_data_iter = thumbnail_meta_data_.find(tab_id);
  if (!force_update && meta_data_iter != thumbnail_meta_data_.end() &&
      meta_data_iter->second.url() == url &&
      (current_time - meta_data_iter->second.capture_time()) <
          capture_min_request_time_ms_) {
    return false;
  }

  thumbnail_meta_data_[tab_id] = ThumbnailMetaData(current_time, url);
  return true;
}

bool ThumbnailCache::IsInVisibleIds(TabId tab_id) {
  return primary_tab_id_ == tab_id || base::Contains(visible_ids_, tab_id);
}

void ThumbnailCache::UpdateVisibleIds(const std::vector<TabId>& priority,
                                      TabId primary_tab_id) {
  bool needs_update = false;
  if (primary_tab_id_ != primary_tab_id) {
    // The primary screen-filling tab (if any) is not pushed onto the read
    // queue, under the assumption that it either has a live layer or will have
    // one very soon.
    primary_tab_id_ = primary_tab_id;
    needs_update = true;
  }

  size_t ids_size = std::min(priority.size(), cache_.MaximumCacheSize());
  if (visible_ids_.size() != ids_size) {
    needs_update = true;
  } else {
    // Early out if called with the same input as last time (We only care
    // about the first mCache.MaximumCacheSize() entries).
    auto visible_iter = visible_ids_.begin();
    auto priority_iter = priority.begin();
    while (visible_iter != visible_ids_.end() &&
           priority_iter != priority.end()) {
      if (*priority_iter != *visible_iter || !cache_.Get(*priority_iter)) {
        needs_update = true;
        break;
      }
      visible_iter++;
      priority_iter++;
    }
  }

  if (!needs_update) {
    PruneCache();
    return;
  }

  read_queue_.clear();
  visible_ids_.clear();
  size_t count = 0;
  auto iter = priority.begin();
  while (iter != priority.end() && count < ids_size) {
    TabId tab_id = *iter;
    visible_ids_.push_back(tab_id);
    if (!cache_.Get(tab_id) && primary_tab_id_ != tab_id &&
        !base::Contains(read_queue_, tab_id)) {
      read_queue_.push_back(tab_id);
    }
    iter++;
    count++;
  }

  ReadNextThumbnail();

  PruneCache();
}

void ThumbnailCache::PruneCache() {
  // Intentionally ignore `primary_tab_id_` as it should have a live layer. If
  // that isn't true or may be slow the caller should include it in
  // `visible_ids_`.
  base::flat_set<TabId> ids_to_keep(
      std::vector<TabId>(visible_ids_.begin(), visible_ids_.end()));
  std::vector<TabId> ids_to_remove;

  for (const auto& entry : cache_) {
    if (!base::Contains(ids_to_keep, entry.first)) {
      ids_to_remove.push_back(entry.first);
    }
  }
  for (TabId id : ids_to_remove) {
    cache_.Remove(id);
  }
}

void ThumbnailCache::DecompressEtc1ThumbnailFromFile(
    TabId tab_id,
    base::OnceCallback<void(bool, const SkBitmap&)> post_decompress_callback) {
  auto decompress_task = base::BindOnce(
      &thumbnail::Etc1ThumbnailHelper::Decompress, etc1_helper_.GetWeakPtr(),
      std::move(post_decompress_callback));
  etc1_helper_.Read(
      tab_id, base::BindPostTaskToCurrentDefault(std::move(decompress_task)));
}

void ThumbnailCache::ScheduleRecordCacheMetrics(base::TimeDelta mean_delay) {
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ThumbnailCache::RecordCacheMetrics,
                     weak_factory_.GetWeakPtr()),
      ComputeDelay(mean_delay));
}

void ThumbnailCache::RecordCacheMetrics() {
  base::UmaHistogramCounts100("Android.ThumbnailCache.InMemoryCacheEntries",
                              cache_.size());
  base::UmaHistogramMemoryKB("Android.ThumbnailCache.InMemoryCacheSize",
                             ComputeCacheSize(cache_) / kKiB);
  ScheduleRecordCacheMetrics(base::Minutes(5));
}

// static
size_t ThumbnailCache::ComputeCacheSize(ExpiringThumbnailCache& cache) {
  return std::accumulate(
      cache.begin(), cache.end(), 0U,
      [](size_t acc, const std::pair<TabId, Thumbnail*>& it) {
        return acc + it.second->size_in_bytes();
      });
}

void ThumbnailCache::RemoveFromDisk(TabId tab_id) {
  jpeg_helper_.Delete(tab_id);
  etc1_helper_.Delete(tab_id);
}

void ThumbnailCache::WriteEtc1ThumbnailIfNecessary(
    TabId tab_id,
    sk_sp<SkPixelRef> compressed_data,
    float scale,
    const gfx::Size& content_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!compressed_data || write_tasks_count_ >= write_queue_max_size_) {
    return;
  }

  write_tasks_count_++;

  base::OnceClosure post_write_task = base::BindOnce(
      &ThumbnailCache::PostWriteEtc1Task, weak_factory_.GetWeakPtr());
  etc1_helper_.Write(tab_id, compressed_data, scale, content_size,
                     std::move(post_write_task));
}

void ThumbnailCache::WriteJpegThumbnailIfNecessary(
    TabId tab_id,
    std::unique_ptr<ThumbnailCaptureTracker, base::OnTaskRunnerDeleter> tracker,
    std::vector<uint8_t> compressed_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (compressed_data.empty() || write_tasks_count_ >= write_queue_max_size_) {
    if (tracker) {
      tracker->MarkJpegFailed();
    }
    return;
  }

  write_tasks_count_++;

  auto post_write_task =
      base::BindOnce(&ThumbnailCache::PostWriteJpegTask,
                     weak_factory_.GetWeakPtr(), std::move(tracker));
  jpeg_helper_.Write(tab_id, std::move(compressed_data),
                     std::move(post_write_task));
}

void ThumbnailCache::SaveAsJpeg(
    TabId tab_id,
    std::unique_ptr<ThumbnailCaptureTracker, base::OnTaskRunnerDeleter> tracker,
    const SkBitmap& bitmap) {
  base::OnceCallback<void(std::vector<uint8_t>)> post_jpeg_compression_task =
      base::BindOnce(&ThumbnailCache::WriteJpegThumbnailIfNecessary,
                     weak_factory_.GetWeakPtr(), tab_id, std::move(tracker));

  jpeg_helper_.Compress(bitmap, std::move(post_jpeg_compression_task));
}

void ThumbnailCache::CompressThumbnailIfNecessary(
    TabId tab_id,
    std::unique_ptr<ThumbnailCaptureTracker, base::OnTaskRunnerDeleter> tracker,
    const base::Time& time_stamp,
    const SkBitmap& bitmap,
    float scale) {
  if (compression_tasks_count_ >= compression_queue_max_size_) {
    RemoveOnMatchedTimeStamp(tab_id, time_stamp);
    tracker->MarkCaptureFailed();
    return;
  }

  compression_tasks_count_++;

  base::OnceCallback<void(sk_sp<SkPixelRef>, const gfx::Size&)>
      post_compression_task =
          base::BindOnce(&ThumbnailCache::PostEtc1CompressionTask,
                         weak_factory_.GetWeakPtr(), tab_id, time_stamp, scale);

  etc1_helper_.Compress(bitmap,
                        ui_resource_provider_->SupportsETC1NonPowerOfTwo(),
                        std::move(post_compression_task));

  if (save_jpeg_thumbnails_) {
    SaveAsJpeg(tab_id, std::move(tracker), bitmap);
  }
}

void ThumbnailCache::ReadNextThumbnail() {
  if (read_queue_.empty() || read_in_progress_) {
    return;
  }

  TabId tab_id = read_queue_.front();
  read_in_progress_ = true;

  base::OnceCallback<void(sk_sp<SkPixelRef>, float, const gfx::Size&)>
      post_read_task = base::BindOnce(&ThumbnailCache::PostEtc1ReadTask,
                                      weak_factory_.GetWeakPtr(), tab_id);

  etc1_helper_.Read(
      tab_id, base::BindPostTaskToCurrentDefault(std::move(post_read_task)));
}

void ThumbnailCache::MakeSpaceForNewItemIfNecessary(TabId tab_id) {
  if (cache_.Get(tab_id) || !base::Contains(visible_ids_, tab_id) ||
      cache_.size() < cache_.MaximumCacheSize()) {
    return;
  }

  TabId key_to_remove;
  bool found_key_to_remove = false;

  // 1. Find a cached item not in this list
  for (auto& item : cache_) {
    if (!base::Contains(visible_ids_, item.first)) {
      key_to_remove = item.first;
      found_key_to_remove = true;
      break;
    }
  }

  if (!found_key_to_remove) {
    // 2. Find the least important id we can remove.
    for (const TabId& id : base::Reversed(visible_ids_)) {
      if (cache_.Get(id)) {
        key_to_remove = id;
        found_key_to_remove = true;
        break;
      }
    }
  }

  if (found_key_to_remove) {
    cache_.Remove(key_to_remove);
  }
}

void ThumbnailCache::RemoveFromReadQueue(TabId tab_id) {
  auto read_iter = base::ranges::find(read_queue_, tab_id);
  if (read_iter != read_queue_.end()) {
    read_queue_.erase(read_iter);
  }
}

void ThumbnailCache::OnUIResourcesWereEvicted() {
  if (visible_ids_.empty()) {
    cache_.Clear();
  } else {
    TabId last_tab = visible_ids_.front();
    std::unique_ptr<Thumbnail> thumbnail = cache_.Remove(last_tab);
    cache_.Clear();

    // Keep the thumbnail for app resume if it wasn't uploaded yet.
    if (thumbnail.get() && !thumbnail->ui_resource_id()) {
      cache_.Put(last_tab, std::move(thumbnail));
    }
  }
}

void ThumbnailCache::SetCaptureMinRequestTimeForTesting(int timeMs) {
  capture_min_request_time_ms_ = base::Milliseconds(timeMs);
}

void ThumbnailCache::InvalidateCachedThumbnail(Thumbnail* thumbnail) {
  DCHECK(thumbnail);
  TabId tab_id = thumbnail->tab_id();
  cc::UIResourceId uid = thumbnail->ui_resource_id();

  Thumbnail* cached_thumbnail = cache_.Get(tab_id);
  if (cached_thumbnail && cached_thumbnail->ui_resource_id() == uid) {
    cache_.Remove(tab_id);
  }
}

void ThumbnailCache::PostWriteJpegTask(
    std::unique_ptr<ThumbnailCaptureTracker, base::OnTaskRunnerDeleter> tracker,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (tracker) {
    if (success) {
      tracker->SetWroteJpeg();
    } else {
      tracker->MarkJpegFailed();
    }
  }

  write_tasks_count_--;
}

void ThumbnailCache::PostWriteEtc1Task() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  write_tasks_count_--;
}

void ThumbnailCache::PostEtc1CompressionTask(TabId tab_id,
                                             const base::Time& time_stamp,
                                             float scale,
                                             sk_sp<SkPixelRef> compressed_data,
                                             const gfx::Size& content_size) {
  compression_tasks_count_--;
  if (!compressed_data) {
    RemoveOnMatchedTimeStamp(tab_id, time_stamp);
    return;
  }

  Thumbnail* thumbnail = cache_.Get(tab_id);
  if (thumbnail) {
    if (thumbnail->time_stamp() != time_stamp) {
      return;
    }
    thumbnail->SetCompressedBitmap(compressed_data, content_size);
    // Don't upload the texture if we are being paused/stopped because
    // the context will go away anyways.
    if (base::android::ApplicationStatusListener::GetState() ==
        base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES) {
      thumbnail->CreateUIResource();
      NotifyObserversOfThumbnailAddedToCache(tab_id);
    }
  }
  WriteEtc1ThumbnailIfNecessary(tab_id, std::move(compressed_data), scale,
                                content_size);
}

void ThumbnailCache::PostEtc1ReadTask(TabId tab_id,
                                      sk_sp<SkPixelRef> compressed_data,
                                      float scale,
                                      const gfx::Size& content_size) {
  read_in_progress_ = false;

  auto iter = base::ranges::find(read_queue_, tab_id);
  if (iter == read_queue_.end()) {
    ReadNextThumbnail();
    return;
  }

  read_queue_.erase(iter);

  if (!cache_.Get(tab_id) && compressed_data) {
    auto meta_iter = thumbnail_meta_data_.find(tab_id);
    base::Time time_stamp = base::Time::Now();
    if (meta_iter != thumbnail_meta_data_.end()) {
      time_stamp = meta_iter->second.capture_time();
    }

    if (base::Contains(visible_ids_, tab_id)) {
      MakeSpaceForNewItemIfNecessary(tab_id);
      std::unique_ptr<Thumbnail> thumbnail = Thumbnail::Create(
          tab_id, time_stamp, scale, ui_resource_provider_, this);
      thumbnail->SetCompressedBitmap(std::move(compressed_data), content_size);
      if (kPreferCPUMemory) {
        thumbnail->CreateUIResource();
      }

      cache_.Put(tab_id, std::move(thumbnail));
      NotifyObserversOfThumbnailAddedToCache(tab_id);
      NotifyObserversOfThumbnailRead(tab_id);
    }
  }

  ReadNextThumbnail();
}

void ThumbnailCache::NotifyObserversOfThumbnailAddedToCache(TabId tab_id) {
  for (ThumbnailCacheObserver& observer : observers_) {
    observer.OnThumbnailAddedToCache(tab_id);
  }
}

void ThumbnailCache::NotifyObserversOfThumbnailRead(TabId tab_id) {
  for (ThumbnailCacheObserver& observer : observers_) {
    observer.OnFinishedThumbnailRead(tab_id);
  }
}

void ThumbnailCache::RemoveOnMatchedTimeStamp(TabId tab_id,
                                              const base::Time& time_stamp) {
  // We remove the cached version if it matches the tab_id and the time_stamp.
  Thumbnail* thumbnail = cache_.Get(tab_id);
  if (thumbnail && thumbnail->time_stamp() == time_stamp) {
    Remove(tab_id);
  }
}

ThumbnailCache::ThumbnailMetaData::ThumbnailMetaData(
    const base::Time& current_time,
    GURL url)
    : capture_time_(current_time), url_(std::move(url)) {}

void ThumbnailCache::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  if (level == base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    cache_.Clear();
  }
}

}  // namespace thumbnail
