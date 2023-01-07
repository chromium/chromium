// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAIL_CC_THUMBNAIL_CACHE_H_
#define CHROME_BROWSER_THUMBNAIL_CC_THUMBNAIL_CACHE_H_

#include <stddef.h>

#include <list>
#include <map>
#include <set>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/thumbnail/cc/scoped_ptr_expiring_cache.h"
#include "chrome/browser/thumbnail/cc/thumbnail.h"
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

typedef std::list<TabId> TabIdList;

class ThumbnailCacheObserver {
 public:
  virtual void OnFinishedThumbnailRead(TabId tab_id) = 0;
};

class ThumbnailCache : ThumbnailDelegate {
 public:
  ThumbnailCache(size_t default_cache_size,
                 size_t approximation_cache_size,
                 size_t compression_queue_max_size,
                 size_t write_queue_max_size,
                 bool use_approximation_thumbnail,
                 bool save_jpeg_thumbnails,
                 double jpeg_aspect_ratio);

  ThumbnailCache(const ThumbnailCache&) = delete;
  ThumbnailCache& operator=(const ThumbnailCache&) = delete;

  ~ThumbnailCache() override;

  void SetUIResourceProvider(
      base::WeakPtr<ui::UIResourceProvider> ui_resource_provider);

  void AddThumbnailCacheObserver(ThumbnailCacheObserver* observer);
  void RemoveThumbnailCacheObserver(ThumbnailCacheObserver* observer);

  void Put(TabId tab_id,
           const SkBitmap& bitmap,
           float thumbnail_scale,
           double jpeg_aspect_ratio);
  void Remove(TabId tab_id);
  Thumbnail* Get(TabId tab_id, bool force_disk_read, bool allow_approximation);

  void InvalidateThumbnailIfChanged(TabId tab_id, const GURL& url);
  bool CheckAndUpdateThumbnailMetaData(TabId tab_id, const GURL& url);
  void UpdateVisibleIds(const TabIdList& priority, TabId primary_tab_id);
  void DecompressThumbnailFromFile(
      TabId tab_id,
      double jpeg_aspect_ratio,
      base::OnceCallback<void(bool, const SkBitmap&)> post_decompress_callback);

  // Called when resident textures were evicted, which requires paging
  // in bitmaps.
  void OnUIResourcesWereEvicted();
  void SetCaptureMinRequestTimeForTesting(int timeMs);

  // ThumbnailDelegate implementation
  void InvalidateCachedThumbnail(Thumbnail* thumbnail) override;
  static base::FilePath GetCacheDirectory();
  static base::FilePath GetFilePath(TabId tab_id);
  static base::FilePath GetJpegFilePath(TabId tab_id);

 private:
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

  void RemoveFromDisk(TabId tab_id);
  static void RemoveFromDiskTask(TabId tab_id);
  void WriteThumbnailIfNecessary(TabId tab_id,
                                 sk_sp<SkPixelRef> compressed_data,
                                 float scale,
                                 const gfx::Size& content_size);
  void WriteJpegThumbnailIfNecessary(TabId tab_id,
                                     std::vector<uint8_t> compressed_data);
  void SaveAsJpeg(TabId tab_id,
                  const SkBitmap& bitmap,
                  double jpeg_aspect_ratio);
  void ForkToSaveAsJpeg(
      base::OnceCallback<void(bool, const SkBitmap&)> callback,
      int tab_id,
      double jpeg_aspect_ratio,
      bool result,
      const SkBitmap& bitmap);

  void CompressThumbnailIfNecessary(TabId tab_id,
                                    const base::Time& time_stamp,
                                    const SkBitmap& bitmap,
                                    float scale,
                                    double jpeg_aspect_ratio);
  void ReadNextThumbnail();
  void MakeSpaceForNewItemIfNecessary(TabId tab_id);
  void RemoveFromReadQueue(TabId tab_id);
  static void WriteTask(TabId tab_id,
                        sk_sp<SkPixelRef> compressed_data,
                        float scale,
                        const gfx::Size& content_size,
                        base::OnceClosure post_write_task);
  static void WriteJpegTask(TabId tab_id,
                            std::vector<uint8_t> compressed_data,
                            base::OnceClosure post_write_task);
  void PostWriteTask();
  static void CompressionTask(
      SkBitmap raw_data,
      gfx::Size encoded_size,
      base::OnceCallback<void(sk_sp<SkPixelRef>, const gfx::Size&)>
          post_compression_task);
  static void JpegProcessingTask(
      double jpeg_aspect_ratio,
      SkBitmap bitmap,
      base::OnceCallback<void(std::vector<uint8_t>)> post_processing_task);
  void PostCompressionTask(TabId tab_id,
                           const base::Time& time_stamp,
                           float scale,
                           sk_sp<SkPixelRef> compressed_data,
                           const gfx::Size& content_size);
  static void DecompressionTask(
      base::OnceCallback<void(bool, const SkBitmap&)> post_decompress_callback,
      sk_sp<SkPixelRef> compressed_data,
      float scale,
      const gfx::Size& encoded_size);
  static void ReadTask(
      bool decompress,
      TabId tab_id,
      base::OnceCallback<void(sk_sp<SkPixelRef>, float, const gfx::Size&)>
          post_read_task);
  void PostReadTask(TabId tab_id,
                    sk_sp<SkPixelRef> compressed_data,
                    float scale,
                    const gfx::Size& content_size);
  void NotifyObserversOfThumbnailRead(TabId tab_id);
  void RemoveOnMatchedTimeStamp(TabId tab_id, const base::Time& time_stamp);
  static std::pair<SkBitmap, float> CreateApproximation(const SkBitmap& bitmap,
                                                        float scale);

  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

  const scoped_refptr<base::SequencedTaskRunner> file_sequenced_task_runner_;

  const size_t compression_queue_max_size_;
  const size_t write_queue_max_size_;
  const bool use_approximation_thumbnail_;
  const bool save_jpeg_thumbnails_;
  base::TimeDelta capture_min_request_time_ms_;

  size_t compression_tasks_count_;
  size_t write_tasks_count_;
  bool read_in_progress_;

  ExpiringThumbnailCache cache_;
  ExpiringThumbnailCache approximation_cache_;
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

#endif  // CHROME_BROWSER_THUMBNAIL_CC_THUMBNAIL_CACHE_H_
