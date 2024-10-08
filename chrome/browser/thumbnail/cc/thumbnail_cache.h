// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAIL_CC_THUMBNAIL_CACHE_H_
#define CHROME_BROWSER_THUMBNAIL_CC_THUMBNAIL_CACHE_H_

#include <stddef.h>

#include <list>
#include <map>
#include <set>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/thumbnail/cc/etc1_thumbnail_helper.h"
#include "chrome/browser/thumbnail/cc/jpeg_thumbnail_helper.h"
#include "chrome/browser/thumbnail/cc/scoped_ptr_expiring_cache.h"
#include "chrome/browser/thumbnail/cc/thumbnail.h"
#include "chrome/browser/thumbnail/cc/thumbnail_capture_tracker.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/android/resources/ui_resource_provider.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "url/gurl.h"

namespace base {
class Time;
}

namespace thumbnail {

typedef std::list<TabId> TabIdList;

class ThumbnailCacheObserver {
 public:
  virtual void OnThumbnailAddedToCache(TabId tab_id) = 0;
  virtual void OnFinishedThumbnailRead(TabId tab_id) = 0;
};

class ThumbnailCache : ThumbnailDelegate {
 public:
  ThumbnailCache(size_t default_cache_size,
                 size_t compression_queue_max_size,
                 size_t write_queue_max_size,
                 bool save_jpeg_thumbnails);

  ThumbnailCache(const ThumbnailCache&) = delete;
  ThumbnailCache& operator=(const ThumbnailCache&) = delete;

  ~ThumbnailCache() override;

  void SetUIResourceProvider(
      base::WeakPtr<ui::UIResourceProvider> ui_resource_provider);

  void AddThumbnailCacheObserver(ThumbnailCacheObserver* observer);
  void RemoveThumbnailCacheObserver(ThumbnailCacheObserver* observer);

  void Put(TabId tab_id,
           std::unique_ptr<ThumbnailCaptureTracker, base::OnTaskRunnerDeleter>
               tracker,
           const SkBitmap& bitmap,
           float thumbnail_scale);
  void Remove(TabId tab_id);
  Thumbnail* Get(TabId tab_id, bool force_disk_read);

  void InvalidateThumbnailIfChanged(TabId tab_id, const GURL& url);
  bool CheckAndUpdateThumbnailMetaData(TabId tab_id,
                                       const GURL& url,
                                       bool force_update);
  bool IsInVisibleIds(TabId tab_id);
  void UpdateVisibleIds(const std::vector<TabId>& priority,
                        TabId primary_tab_id);
  void DecompressEtc1ThumbnailFromFile(
      TabId tab_id,
      base::OnceCallback<void(bool, const SkBitmap&)> post_decompress_callback);

  // Called when resident textures were evicted, which requires paging
  // in bitmaps.
  void OnUIResourcesWereEvicted();
  void SetCaptureMinRequestTimeForTesting(int timeMs);

  // ThumbnailDelegate implementation
  void InvalidateCachedThumbnail(Thumbnail* thumbnail) override;
  static base::FilePath GetCacheDirectory();

 private:
  friend class ThumbnailCacheTest;

  class ThumbnailMetaData {
   public:
    ThumbnailMetaData() = default;
    ThumbnailMetaData(const base::Time& current_time, GURL url);
    const GURL& url() const { return url_; }
    base::Time capture_time() const { return capture_time_; }

   private:
    base::Time capture_time_;
    GURL url_;
  };

  using ExpiringThumbnailCache = ScopedPtrExpiringCache<TabId, Thumbnail>;
  using ThumbnailMetaDataMap = std::map<TabId, ThumbnailMetaData>;

  void ScheduleRecordCacheMetrics(base::TimeDelta mean_delay);
  void RecordCacheMetrics();
  static size_t ComputeCacheSize(ExpiringThumbnailCache& cache);
  void PruneCache();

  void RemoveFromDisk(TabId tab_id);
  void WriteEtc1ThumbnailIfNecessary(TabId tab_id,
                                     sk_sp<SkPixelRef> compressed_data,
                                     float scale,
                                     const gfx::Size& content_size);
  void WriteJpegThumbnailIfNecessary(
      TabId tab_id,
      std::unique_ptr<ThumbnailCaptureTracker, base::OnTaskRunnerDeleter>
          tracker,
      std::vector<uint8_t> compressed_data);
  void SaveAsJpeg(TabId tab_id,
                  std::unique_ptr<ThumbnailCaptureTracker,
                                  base::OnTaskRunnerDeleter> tracker,
                  const SkBitmap& bitmap);
  void PostWriteJpegTask(std::unique_ptr<ThumbnailCaptureTracker,
                                         base::OnTaskRunnerDeleter> tracker,
                         bool success);

  void CompressThumbnailIfNecessary(
      TabId tab_id,
      std::unique_ptr<ThumbnailCaptureTracker, base::OnTaskRunnerDeleter>
          tracker,
      const base::Time& time_stamp,
      const SkBitmap& bitmap,
      float scale);
  void ReadNextThumbnail();
  void MakeSpaceForNewItemIfNecessary(TabId tab_id);
  void RemoveFromReadQueue(TabId tab_id);
  void PostWriteEtc1Task();
  void PostEtc1CompressionTask(TabId tab_id,
                               const base::Time& time_stamp,
                               float scale,
                               sk_sp<SkPixelRef> compressed_data,
                               const gfx::Size& content_size);
  void PostEtc1ReadTask(TabId tab_id,
                        sk_sp<SkPixelRef> compressed_data,
                        float scale,
                        const gfx::Size& content_size);
  void NotifyObserversOfThumbnailAddedToCache(TabId tab_id);
  void NotifyObserversOfThumbnailRead(TabId tab_id);
  void RemoveOnMatchedTimeStamp(TabId tab_id, const base::Time& time_stamp);

  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

  // Default priority as most of the time there is a placeholder available.
  const scoped_refptr<base::SequencedTaskRunner>
      etc1_file_sequenced_task_runner_;
  // USER_VISIBLE priority as almost always used for Tab Switcher UI.
  const scoped_refptr<base::SequencedTaskRunner>
      jpeg_file_sequenced_task_runner_;
  thumbnail::Etc1ThumbnailHelper etc1_helper_;
  thumbnail::JpegThumbnailHelper jpeg_helper_;

  const size_t compression_queue_max_size_;
  const size_t write_queue_max_size_;
  const bool save_jpeg_thumbnails_;
  base::TimeDelta capture_min_request_time_ms_;

  // TODO(crbug.com/40885026): Determine if these limits are still relevant.
  // Remove or tune accordingly (i.e. split by jpeg and etc1).
  size_t compression_tasks_count_;
  size_t write_tasks_count_;
  bool read_in_progress_;

  ExpiringThumbnailCache cache_;
  base::ObserverList<ThumbnailCacheObserver>::Unchecked observers_;
  ThumbnailMetaDataMap thumbnail_meta_data_;
  TabIdList read_queue_;
  TabIdList visible_ids_;
  TabId primary_tab_id_ = -1;

  base::WeakPtr<ui::UIResourceProvider> ui_resource_provider_;
  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_;
  base::WeakPtrFactory<ThumbnailCache> weak_factory_{this};
};

}  // namespace thumbnail

#endif  // CHROME_BROWSER_THUMBNAIL_CC_THUMBNAIL_CACHE_H_
