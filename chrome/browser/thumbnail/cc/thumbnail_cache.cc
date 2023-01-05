// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnail/cc/thumbnail_cache.h"

#include <cmath>
#include <utility>

#include "base/android/application_status_listener.h"
#include "base/android/path_utils.h"
#include "base/big_endian.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/cxx17_backports.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
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

namespace {

const float kApproximationScaleFactor = 4.f;
const base::TimeDelta kDefaultCaptureMinRequestTimeMs(base::Milliseconds(1000));

const int kCompressedKey = 0xABABABAB;
const int kCurrentExtraVersion = 1;

// Indicates whether we prefer to have more free CPU memory over GPU memory.
const bool kPreferCPUMemory = true;

unsigned int NextPowerOfTwo(int a) {
  DCHECK(a >= 0);
  auto x = static_cast<unsigned int>(a);
  --x;
  x |= x >> 1u;
  x |= x >> 2u;
  x |= x >> 4u;
  x |= x >> 8u;
  x |= x >> 16u;
  return x + 1;
}

unsigned int RoundUpMod4(int a) {
  DCHECK(a >= 0);
  auto x = static_cast<unsigned int>(a);
  return (x + 3u) & ~3u;
}

gfx::Size GetEncodedSize(const gfx::Size& bitmap_size, bool supports_npot) {
  DCHECK(bitmap_size.width() >= 0);
  DCHECK(bitmap_size.height() >= 0);
  DCHECK(!bitmap_size.IsEmpty());
  if (!supports_npot) {
    return gfx::Size(NextPowerOfTwo(bitmap_size.width()),
                     NextPowerOfTwo(bitmap_size.height()));
  } else {
    return gfx::Size(RoundUpMod4(bitmap_size.width()),
                     RoundUpMod4(bitmap_size.height()));
  }
}

template <typename T>
bool ReadBigEndianFromFile(base::File& file, T* out) {
  uint8_t buffer[sizeof(T)];
  if (file.ReadAtCurrentPos(reinterpret_cast<char*>(buffer), sizeof(T)) !=
      sizeof(T)) {
    return false;
  }
  base::ReadBigEndian(buffer, out);
  return true;
}

template <typename T>
bool WriteBigEndianToFile(base::File& file, T val) {
  char buffer[sizeof(T)];
  base::WriteBigEndian(buffer, val);
  return file.WriteAtCurrentPos(buffer, sizeof(T)) == sizeof(T);
}

bool ReadBigEndianFloatFromFile(base::File& file, float* out) {
  char buffer[sizeof(float)];
  if (file.ReadAtCurrentPos(buffer, sizeof(buffer)) != sizeof(buffer)) {
    return false;
  }

#if defined(ARCH_CPU_LITTLE_ENDIAN)
  for (size_t i = 0; i < sizeof(float) / 2; i++) {
    char tmp = buffer[i];
    buffer[i] = buffer[sizeof(float) - 1 - i];
    buffer[sizeof(float) - 1 - i] = tmp;
  }
#endif
  memcpy(out, buffer, sizeof(buffer));

  return true;
}

bool WriteBigEndianFloatToFile(base::File& file, float val) {
  char buffer[sizeof(float)];
  memcpy(buffer, &val, sizeof(buffer));

#if defined(ARCH_CPU_LITTLE_ENDIAN)
  for (size_t i = 0; i < sizeof(float) / 2; i++) {
    char tmp = buffer[i];
    buffer[i] = buffer[sizeof(float) - 1 - i];
    buffer[sizeof(float) - 1 - i] = tmp;
  }
#endif
  return file.WriteAtCurrentPos(buffer, sizeof(buffer)) == sizeof(buffer);
}

// TODO(khushalsagar): This is a hack to ensure correct byte size computation
// for SkPixelRefs wrapping encoded data for ETC1 compressed bitmaps. We ideally
// shouldn't be using SkPixelRefs to wrap encoded data.
size_t ETC1RowBytes(int width) {
  DCHECK_EQ(width & 1, 0);
  return width / 2;
}

}  // anonymous namespace

ThumbnailCache::ThumbnailCache(size_t default_cache_size,
                               size_t approximation_cache_size,
                               size_t compression_queue_max_size,
                               size_t write_queue_max_size,
                               bool use_approximation_thumbnail,
                               bool save_jpeg_thumbnails,
                               double jpeg_aspect_ratio)
    : file_sequenced_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      compression_queue_max_size_(compression_queue_max_size),
      write_queue_max_size_(write_queue_max_size),
      use_approximation_thumbnail_(use_approximation_thumbnail),
      save_jpeg_thumbnails_(save_jpeg_thumbnails),
      capture_min_request_time_ms_(kDefaultCaptureMinRequestTimeMs),
      compression_tasks_count_(0),
      write_tasks_count_(0),
      read_in_progress_(false),
      cache_(default_cache_size),
      approximation_cache_(approximation_cache_size),
      ui_resource_provider_(nullptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  memory_pressure_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE, base::BindRepeating(&ThumbnailCache::OnMemoryPressure,
                                     base::Unretained(this)));
}

ThumbnailCache::~ThumbnailCache() {
  SetUIResourceProvider(nullptr);
}

void ThumbnailCache::SetUIResourceProvider(
    base::WeakPtr<ui::UIResourceProvider> ui_resource_provider) {
  if (ui_resource_provider_.get() == ui_resource_provider.get()) {
    return;
  }

  approximation_cache_.Clear();
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

void ThumbnailCache::Put(TabId tab_id,
                         const SkBitmap& bitmap,
                         float thumbnail_scale,
                         double jpeg_aspect_ratio) {
  if (!ui_resource_provider_ || bitmap.empty() || thumbnail_scale <= 0) {
    return;
  }

  if (thumbnail_meta_data_.find(tab_id) == thumbnail_meta_data_.end()) {
    DVLOG(1) << "Thumbnail meta data was removed for tab id " << tab_id;
    return;
  }

  base::Time time_stamp = thumbnail_meta_data_[tab_id].capture_time();
  std::unique_ptr<Thumbnail> thumbnail = Thumbnail::Create(
      tab_id, time_stamp, thumbnail_scale, ui_resource_provider_, this);
  thumbnail->SetBitmap(bitmap);

  RemoveFromReadQueue(tab_id);
  MakeSpaceForNewItemIfNecessary(tab_id);
  cache_.Put(tab_id, std::move(thumbnail));

  if (use_approximation_thumbnail_) {
    std::pair<SkBitmap, float> approximation =
        CreateApproximation(bitmap, thumbnail_scale);
    std::unique_ptr<Thumbnail> approx_thumbnail = Thumbnail::Create(
        tab_id, time_stamp, approximation.second, ui_resource_provider_, this);
    approx_thumbnail->SetBitmap(approximation.first);
    approximation_cache_.Put(tab_id, std::move(approx_thumbnail));
  }
  CompressThumbnailIfNecessary(tab_id, time_stamp, bitmap, thumbnail_scale,
                               jpeg_aspect_ratio);
}

void ThumbnailCache::Remove(TabId tab_id) {
  cache_.Remove(tab_id);
  approximation_cache_.Remove(tab_id);
  thumbnail_meta_data_.erase(tab_id);
  RemoveFromDisk(tab_id);
  RemoveFromReadQueue(tab_id);
}

Thumbnail* ThumbnailCache::Get(TabId tab_id,
                               bool force_disk_read,
                               bool allow_approximation) {
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

  if (allow_approximation) {
    thumbnail = approximation_cache_.Get(tab_id);
    if (thumbnail) {
      thumbnail->CreateUIResource();
      return thumbnail;
    }
  }

  return nullptr;
}

void ThumbnailCache::InvalidateThumbnailIfChanged(TabId tab_id,
                                                  const GURL& url) {
  auto meta_data_iter = thumbnail_meta_data_.find(tab_id);
  if (meta_data_iter == thumbnail_meta_data_.end()) {
    thumbnail_meta_data_[tab_id] = ThumbnailMetaData(base::Time(), url);
  } else if (meta_data_iter->second.url() != url) {
    Remove(tab_id);
  }
}

base::FilePath ThumbnailCache::GetCacheDirectory() {
  base::FilePath path;
  base::android::GetThumbnailCacheDirectory(&path);
  return path;
}

base::FilePath ThumbnailCache::GetFilePath(TabId tab_id) {
  base::FilePath path = GetCacheDirectory();
  return path.Append(base::NumberToString(tab_id));
}

base::FilePath ThumbnailCache::GetJpegFilePath(TabId tab_id) {
  return GetFilePath(tab_id).AddExtension(".jpeg");
}

bool ThumbnailCache::CheckAndUpdateThumbnailMetaData(TabId tab_id,
                                                     const GURL& url) {
  base::Time current_time = base::Time::Now();
  auto meta_data_iter = thumbnail_meta_data_.find(tab_id);
  if (meta_data_iter != thumbnail_meta_data_.end() &&
      meta_data_iter->second.url() == url &&
      (current_time - meta_data_iter->second.capture_time()) <
          capture_min_request_time_ms_) {
    return false;
  }

  thumbnail_meta_data_[tab_id] = ThumbnailMetaData(current_time, url);
  return true;
}

void ThumbnailCache::UpdateVisibleIds(const TabIdList& priority,
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
}

void ThumbnailCache::ForkToSaveAsJpeg(
    base::OnceCallback<void(bool, const SkBitmap&)> callback,
    int tab_id,
    double jpeg_aspect_ratio,
    bool result,
    const SkBitmap& bitmap) {
  if (result && !bitmap.isNull()) {
    SaveAsJpeg(tab_id, bitmap, jpeg_aspect_ratio);
  }
  std::move(callback).Run(result, bitmap);
}

void ThumbnailCache::DecompressThumbnailFromFile(
    TabId tab_id,
    double jpeg_aspect_ratio,
    base::OnceCallback<void(bool, const SkBitmap&)> post_decompress_callback) {
  base::OnceCallback<void(bool, const SkBitmap&)> transcoding_callback;
  if (save_jpeg_thumbnails_) {
    transcoding_callback = base::BindOnce(
        &ThumbnailCache::ForkToSaveAsJpeg, weak_factory_.GetWeakPtr(),
        std::move(post_decompress_callback), tab_id, jpeg_aspect_ratio);
  } else {
    transcoding_callback = std::move(post_decompress_callback);
  }

  base::OnceCallback<void(sk_sp<SkPixelRef>, float, const gfx::Size&)>
      decompress_task = base::BindOnce(&ThumbnailCache::DecompressionTask,
                                       std::move(transcoding_callback));

  file_sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ThumbnailCache::ReadTask, true, tab_id,
                                std::move(decompress_task)));
}

void ThumbnailCache::RemoveFromDisk(TabId tab_id) {
  file_sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ThumbnailCache::RemoveFromDiskTask, tab_id));
}

void ThumbnailCache::RemoveFromDiskTask(TabId tab_id) {
  base::FilePath file_path = GetFilePath(tab_id);
  if (base::PathExists(file_path)) {
    base::DeleteFile(file_path);
  }
  base::FilePath jpeg_file_path = GetJpegFilePath(tab_id);
  if (base::PathExists(jpeg_file_path)) {
    base::DeleteFile(jpeg_file_path);
  }
}

void ThumbnailCache::WriteThumbnailIfNecessary(
    TabId tab_id,
    sk_sp<SkPixelRef> compressed_data,
    float scale,
    const gfx::Size& content_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (write_tasks_count_ >= write_queue_max_size_) {
    return;
  }

  write_tasks_count_++;

  base::OnceClosure post_write_task = base::BindOnce(
      &ThumbnailCache::PostWriteTask, weak_factory_.GetWeakPtr());
  file_sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ThumbnailCache::WriteTask, tab_id, compressed_data, scale,
                     content_size, std::move(post_write_task)));
}

void ThumbnailCache::WriteJpegThumbnailIfNecessary(
    TabId tab_id,
    std::vector<uint8_t> compressed_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (compressed_data.empty()) {
    return;
  }
  if (write_tasks_count_ >= write_queue_max_size_) {
    return;
  }

  write_tasks_count_++;

  base::OnceClosure post_write_task = base::BindOnce(
      &ThumbnailCache::PostWriteTask, weak_factory_.GetWeakPtr());
  file_sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ThumbnailCache::WriteJpegTask, tab_id,
                     std::move(compressed_data), std::move(post_write_task)));
}

void ThumbnailCache::SaveAsJpeg(TabId tab_id,
                                const SkBitmap& bitmap,
                                double jpeg_aspect_ratio) {
  base::OnceCallback<void(std::vector<uint8_t>)> post_jpeg_compression_task =
      base::BindOnce(&ThumbnailCache::WriteJpegThumbnailIfNecessary,
                     weak_factory_.GetWeakPtr(), tab_id);

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ThumbnailCache::JpegProcessingTask, jpeg_aspect_ratio,
                     bitmap, std::move(post_jpeg_compression_task)));
}

void ThumbnailCache::CompressThumbnailIfNecessary(TabId tab_id,
                                                  const base::Time& time_stamp,
                                                  const SkBitmap& bitmap,
                                                  float scale,
                                                  double jpeg_aspect_ratio) {
  if (compression_tasks_count_ >= compression_queue_max_size_) {
    RemoveOnMatchedTimeStamp(tab_id, time_stamp);
    return;
  }

  compression_tasks_count_++;

  base::OnceCallback<void(sk_sp<SkPixelRef>, const gfx::Size&)>
      post_compression_task =
          base::BindOnce(&ThumbnailCache::PostCompressionTask,
                         weak_factory_.GetWeakPtr(), tab_id, time_stamp, scale);

  gfx::Size raw_data_size(bitmap.width(), bitmap.height());
  gfx::Size encoded_size = GetEncodedSize(
      raw_data_size, ui_resource_provider_->SupportsETC1NonPowerOfTwo());

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ThumbnailCache::CompressionTask, bitmap, encoded_size,
                     std::move(post_compression_task)));

  if (save_jpeg_thumbnails_) {
    SaveAsJpeg(tab_id, bitmap, jpeg_aspect_ratio);
  }
}

void ThumbnailCache::ReadNextThumbnail() {
  if (read_queue_.empty() || read_in_progress_) {
    return;
  }

  TabId tab_id = read_queue_.front();
  read_in_progress_ = true;

  base::OnceCallback<void(sk_sp<SkPixelRef>, float, const gfx::Size&)>
      post_read_task = base::BindOnce(&ThumbnailCache::PostReadTask,
                                      weak_factory_.GetWeakPtr(), tab_id);

  file_sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ThumbnailCache::ReadTask, false, tab_id,
                                std::move(post_read_task)));
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
    approximation_cache_.Clear();
  } else {
    TabId last_tab = visible_ids_.front();
    std::unique_ptr<Thumbnail> thumbnail = cache_.Remove(last_tab);
    cache_.Clear();
    std::unique_ptr<Thumbnail> approximation =
        approximation_cache_.Remove(last_tab);
    approximation_cache_.Clear();

    // Keep the thumbnail for app resume if it wasn't uploaded yet.
    if (thumbnail.get() && !thumbnail->ui_resource_id()) {
      cache_.Put(last_tab, std::move(thumbnail));
    }
    if (approximation.get() && !approximation->ui_resource_id()) {
      approximation_cache_.Put(last_tab, std::move(approximation));
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

  cached_thumbnail = approximation_cache_.Get(tab_id);
  if (cached_thumbnail && cached_thumbnail->ui_resource_id() == uid) {
    approximation_cache_.Remove(tab_id);
  }
}

namespace {

bool WriteToFile(base::File& file,
                 const gfx::Size& content_size,
                 const float scale,
                 sk_sp<SkPixelRef> compressed_data) {
  if (!file.IsValid()) {
    return false;
  }

  if (!WriteBigEndianToFile(file, kCompressedKey)) {
    return false;
  }

  if (!WriteBigEndianToFile(file, content_size.width())) {
    return false;
  }

  if (!WriteBigEndianToFile(file, content_size.height())) {
    return false;
  }

  // Write ETC1 header.
  CHECK(compressed_data->width() >= 0);
  CHECK(compressed_data->height() >= 0);
  unsigned width = static_cast<unsigned>(compressed_data->width());
  unsigned height = static_cast<unsigned>(compressed_data->height());

  unsigned char etc1_buffer[ETC_PKM_HEADER_SIZE];
  etc1_pkm_format_header(etc1_buffer, width, height);

  int header_bytes_written = file.WriteAtCurrentPos(
      reinterpret_cast<char*>(etc1_buffer), ETC_PKM_HEADER_SIZE);
  if (header_bytes_written != ETC_PKM_HEADER_SIZE) {
    return false;
  }

  int data_size = etc1_get_encoded_data_size(width, height);
  int pixel_bytes_written = file.WriteAtCurrentPos(
      reinterpret_cast<char*>(compressed_data->pixels()), data_size);
  if (pixel_bytes_written != data_size) {
    return false;
  }

  if (!WriteBigEndianToFile(file, kCurrentExtraVersion)) {
    return false;
  }

  if (!WriteBigEndianFloatToFile(file, 1.f / scale)) {
    return false;
  }

  return true;
}

}  // anonymous namespace

void ThumbnailCache::WriteTask(TabId tab_id,
                               sk_sp<SkPixelRef> compressed_data,
                               float scale,
                               const gfx::Size& content_size,
                               base::OnceClosure post_write_task) {
  DCHECK(compressed_data);

  base::FilePath file_path = GetFilePath(tab_id);

  base::File file(file_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);

  bool success = WriteToFile(file, content_size, scale, compressed_data);

  file.Close();

  if (!success) {
    base::DeleteFile(file_path);
  }

  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                               std::move(post_write_task));
}

void ThumbnailCache::WriteJpegTask(TabId tab_id,
                                   std::vector<uint8_t> compressed_data,
                                   base::OnceClosure post_write_task) {
  DCHECK(!compressed_data.empty());

  base::FilePath file_path = GetJpegFilePath(tab_id);
  base::File file(file_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);

  bool success = file.IsValid();
  if (success) {
    int bytes_written =
        file.Write(0, reinterpret_cast<const char*>(compressed_data.data()),
                   compressed_data.size());
    success &= bytes_written == static_cast<int>(compressed_data.size());
    file.Close();
  }

  if (!success) {
    base::DeleteFile(file_path);
  }

  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                               std::move(post_write_task));
}

void ThumbnailCache::PostWriteTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  write_tasks_count_--;
}

void ThumbnailCache::CompressionTask(
    SkBitmap raw_data,
    gfx::Size encoded_size,
    base::OnceCallback<void(sk_sp<SkPixelRef>, const gfx::Size&)>
        post_compression_task) {
  sk_sp<SkPixelRef> compressed_data;
  gfx::Size content_size;

  if (!raw_data.empty()) {
    gfx::Size raw_data_size(raw_data.width(), raw_data.height());
    size_t pixel_size = 4;  // Pixel size is 4 bytes for kARGB_8888_Config.
    size_t stride = pixel_size * raw_data_size.width();

    size_t encoded_bytes =
        etc1_get_encoded_data_size(encoded_size.width(), encoded_size.height());
    SkImageInfo info =
        SkImageInfo::Make(encoded_size.width(), encoded_size.height(),
                          kUnknown_SkColorType, kUnpremul_SkAlphaType);
    sk_sp<SkData> etc1_pixel_data(SkData::MakeUninitialized(encoded_bytes));
    sk_sp<SkPixelRef> etc1_pixel_ref(SkMallocPixelRef::MakeWithData(
        info, ETC1RowBytes(encoded_size.width()), std::move(etc1_pixel_data)));

    bool success = etc1_encode_image(
        reinterpret_cast<unsigned char*>(raw_data.getPixels()),
        raw_data_size.width(), raw_data_size.height(), pixel_size, stride,
        reinterpret_cast<unsigned char*>(etc1_pixel_ref->pixels()),
        encoded_size.width(), encoded_size.height());
    etc1_pixel_ref->setImmutable();

    if (success) {
      compressed_data = std::move(etc1_pixel_ref);
      content_size = raw_data_size;
    }
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(post_compression_task),
                                std::move(compressed_data), content_size));
}

void ThumbnailCache::JpegProcessingTask(
    double jpeg_aspect_ratio,
    SkBitmap bitmap,
    base::OnceCallback<void(std::vector<uint8_t>)> post_processing_task) {
  // We want to show thumbnails in a specific aspect ratio. Therefore, the
  // thumbnail saved needs to be cropped to the target aspect ratio, otherwise
  // it would be vertically center-aligned and the top would be hidden in
  // portrait mode, or it would be shown in the wrong aspect ratio in
  // landscape mode.
  int scale = 2;
  double aspect_ratio = base::clamp(jpeg_aspect_ratio, 0.5, 2.0);

  int width = std::min(bitmap.width() / scale,
                       (int)(bitmap.height() * aspect_ratio / scale));
  int height = std::min(bitmap.height() / scale,
                        (int)(bitmap.width() / aspect_ratio / scale));
  // When cropping the thumbnails, we want to keep the top center portion.
  int begin_x = (bitmap.width() / scale - width) / 2;
  int end_x = begin_x + width;
  SkIRect dest_subset = {begin_x, 0, end_x, height};

  SkBitmap result_bitmap = skia::ImageOperations::Resize(
      bitmap, skia::ImageOperations::RESIZE_BETTER, bitmap.width() / scale,
      bitmap.height() / scale, dest_subset);

  constexpr int kCompressionQuality = 97;
  std::vector<uint8_t> data;
  const bool result =
      gfx::JPEGCodec::Encode(result_bitmap, kCompressionQuality, &data);
  DCHECK(result);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(post_processing_task), std::move(data)));
}

void ThumbnailCache::PostCompressionTask(TabId tab_id,
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
    }
  }
  WriteThumbnailIfNecessary(tab_id, std::move(compressed_data), scale,
                            content_size);
}

namespace {

bool ReadFromFile(base::File& file,
                  gfx::Size* out_content_size,
                  float* out_scale,
                  sk_sp<SkPixelRef>* out_pixels) {
  if (!file.IsValid()) {
    return false;
  }

  int key = 0;
  if (!ReadBigEndianFromFile(file, &key)) {
    return false;
  }

  if (key != kCompressedKey) {
    return false;
  }

  int content_width = 0;
  if (!ReadBigEndianFromFile(file, &content_width) || content_width <= 0) {
    return false;
  }

  int content_height = 0;
  if (!ReadBigEndianFromFile(file, &content_height) || content_height <= 0) {
    return false;
  }

  out_content_size->SetSize(content_width, content_height);

  // Read ETC1 header.
  int header_bytes_read = 0;
  unsigned char etc1_buffer[ETC_PKM_HEADER_SIZE];
  header_bytes_read = file.ReadAtCurrentPos(
      reinterpret_cast<char*>(etc1_buffer), ETC_PKM_HEADER_SIZE);
  if (header_bytes_read != ETC_PKM_HEADER_SIZE) {
    return false;
  }

  if (!etc1_pkm_is_valid(etc1_buffer)) {
    return false;
  }

  int raw_width = 0;
  raw_width = etc1_pkm_get_width(etc1_buffer);
  if (raw_width <= 0) {
    return false;
  }

  int raw_height = 0;
  raw_height = etc1_pkm_get_height(etc1_buffer);
  if (raw_height <= 0) {
    return false;
  }

  // Do some simple sanity check validation.  We can't have thumbnails larger
  // than the max display size of the screen.  We also can't have etc1 texture
  // data larger than the next power of 2 up from that.
  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel();
  int max_dimension = std::max(display_size.width(), display_size.height());

  if (content_width > max_dimension || content_height > max_dimension ||
      static_cast<size_t>(raw_width) > NextPowerOfTwo(max_dimension) ||
      static_cast<size_t>(raw_height) > NextPowerOfTwo(max_dimension)) {
    return false;
  }

  int data_size = etc1_get_encoded_data_size(raw_width, raw_height);
  sk_sp<SkData> etc1_pixel_data(SkData::MakeUninitialized(data_size));

  int pixel_bytes_read = file.ReadAtCurrentPos(
      reinterpret_cast<char*>(etc1_pixel_data->writable_data()), data_size);

  if (pixel_bytes_read != data_size) {
    return false;
  }

  SkImageInfo info = SkImageInfo::Make(
      raw_width, raw_height, kUnknown_SkColorType, kUnpremul_SkAlphaType);

  *out_pixels = SkMallocPixelRef::MakeWithData(info, ETC1RowBytes(raw_width),
                                               std::move(etc1_pixel_data));

  int extra_data_version = 0;
  if (!ReadBigEndianFromFile(file, &extra_data_version)) {
    return false;
  }

  *out_scale = 1.f;
  if (extra_data_version == 1) {
    if (!ReadBigEndianFloatFromFile(file, out_scale)) {
      return false;
    }

    if (*out_scale == 0.f) {
      return false;
    }

    *out_scale = 1.f / *out_scale;
  }

  return true;
}

}  // anonymous namespace

void ThumbnailCache::ReadTask(
    bool decompress,
    TabId tab_id,
    base::OnceCallback<void(sk_sp<SkPixelRef>, float, const gfx::Size&)>
        post_read_task) {
  gfx::Size content_size;
  float scale = 0.f;
  sk_sp<SkPixelRef> compressed_data;
  base::FilePath file_path = GetFilePath(tab_id);

  if (base::PathExists(file_path)) {
    base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);

    bool valid_contents =
        ReadFromFile(file, &content_size, &scale, &compressed_data);
    file.Close();

    if (!valid_contents) {
      content_size.SetSize(0, 0);
      scale = 0.f;
      compressed_data.reset();
      base::DeleteFile(file_path);
    }
  }

  if (decompress) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE},
        base::BindOnce(std::move(post_read_task), std::move(compressed_data),
                       scale, content_size));
  } else {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(post_read_task), std::move(compressed_data),
                       scale, content_size));
  }
}

void ThumbnailCache::PostReadTask(TabId tab_id,
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

    MakeSpaceForNewItemIfNecessary(tab_id);
    std::unique_ptr<Thumbnail> thumbnail = Thumbnail::Create(
        tab_id, time_stamp, scale, ui_resource_provider_, this);
    thumbnail->SetCompressedBitmap(std::move(compressed_data), content_size);
    if (kPreferCPUMemory) {
      thumbnail->CreateUIResource();
    }

    cache_.Put(tab_id, std::move(thumbnail));
    NotifyObserversOfThumbnailRead(tab_id);
  }

  ReadNextThumbnail();
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
  Thumbnail* approx_thumbnail = approximation_cache_.Get(tab_id);
  if ((thumbnail && thumbnail->time_stamp() == time_stamp) ||
      (approx_thumbnail && approx_thumbnail->time_stamp() == time_stamp)) {
    Remove(tab_id);
  }
}

void ThumbnailCache::DecompressionTask(
    base::OnceCallback<void(bool, const SkBitmap&)> post_decompression_callback,
    sk_sp<SkPixelRef> compressed_data,
    float scale,
    const gfx::Size& content_size) {
  SkBitmap raw_data_small;
  bool success = false;

  if (compressed_data) {
    gfx::Size buffer_size =
        gfx::Size(compressed_data->width(), compressed_data->height());

    SkBitmap raw_data;
    raw_data.allocPixels(SkImageInfo::MakeN32(
        buffer_size.width(), buffer_size.height(), kOpaque_SkAlphaType));
    success = etc1_decode_image(
        reinterpret_cast<unsigned char*>(compressed_data->pixels()),
        reinterpret_cast<unsigned char*>(raw_data.getPixels()),
        buffer_size.width(), buffer_size.height(), raw_data.bytesPerPixel(),
        raw_data.rowBytes());
    raw_data.setImmutable();

    if (!success) {
      // Leave raw_data_small empty for consistency with other failure modes.
    } else if (content_size == buffer_size) {
      // Shallow copy the pixel reference.
      raw_data_small = raw_data;
    } else {
      // The content size is smaller than the buffer size (likely because of
      // a power-of-two rounding), so deep copy the bitmap.
      raw_data_small.allocPixels(SkImageInfo::MakeN32(
          content_size.width(), content_size.height(), kOpaque_SkAlphaType));
      SkCanvas small_canvas(raw_data_small);
      small_canvas.drawImage(raw_data.asImage(), 0, 0);
      raw_data_small.setImmutable();
    }
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(post_decompression_callback), success,
                                raw_data_small));
}

ThumbnailCache::ThumbnailMetaData::ThumbnailMetaData(
    const base::Time& current_time,
    GURL url)
    : capture_time_(current_time), url_(std::move(url)) {}

std::pair<SkBitmap, float> ThumbnailCache::CreateApproximation(
    const SkBitmap& bitmap,
    float scale) {
  DCHECK(!bitmap.empty());
  DCHECK_GT(scale, 0);
  float new_scale = 1.f / kApproximationScaleFactor;

  gfx::Size dst_size = gfx::ScaleToFlooredSize(
      gfx::Size(bitmap.width(), bitmap.height()), new_scale);
  SkBitmap dst_bitmap;
  dst_bitmap.allocPixels(SkImageInfo::Make(dst_size.width(), dst_size.height(),
                                           bitmap.info().colorType(),
                                           bitmap.info().alphaType()));
  dst_bitmap.eraseColor(0);
  SkCanvas canvas(dst_bitmap);
  canvas.scale(new_scale, new_scale);
  canvas.drawImage(bitmap.asImage(), 0, 0);
  dst_bitmap.setImmutable();

  return std::make_pair(dst_bitmap, new_scale * scale);
}

void ThumbnailCache::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  if (level == base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    cache_.Clear();
    approximation_cache_.Clear();
  }
}
