// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnail/cc/etc1_thumbnail_helper.h"

#include "base/big_endian.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "third_party/android_opengl/etc1/etc1.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "ui/display/screen.h"

namespace thumbnail {
namespace {

const int kCompressedKey = 0xABABABAB;
const int kCurrentExtraVersion = 1;

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

void CompressTask(SkBitmap raw_data,
                  gfx::Size encoded_size,
                  base::OnceCallback<void(sk_sp<SkPixelRef>, const gfx::Size&)>
                      post_compression_task) {
  sk_sp<SkPixelRef> compressed_data;
  gfx::Size content_size;

  if (!raw_data.empty()) {
    gfx::Size raw_data_size(raw_data.width(), raw_data.height());
    constexpr size_t kPixelSize = 4;  // For kARGB_8888_Config.
    size_t stride = kPixelSize * raw_data_size.width();

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
        raw_data_size.width(), raw_data_size.height(), kPixelSize, stride,
        reinterpret_cast<unsigned char*>(etc1_pixel_ref->pixels()),
        encoded_size.width(), encoded_size.height());
    etc1_pixel_ref->setImmutable();

    if (success) {
      compressed_data = std::move(etc1_pixel_ref);
      content_size = raw_data_size;
    }
  }

  std::move(post_compression_task)
      .Run(std::move(compressed_data), content_size);
}

void WriteTask(base::FilePath file_path,
               sk_sp<SkPixelRef> compressed_data,
               float scale,
               const gfx::Size& content_size,
               base::OnceClosure post_write_task) {
  DCHECK(compressed_data);

  base::File file(file_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);

  bool success = WriteToFile(file, content_size, scale, compressed_data);

  file.Close();

  if (!success) {
    base::DeleteFile(file_path);
  }

  std::move(post_write_task).Run();
}

void ReadTask(
    base::FilePath file_path,
    base::OnceCallback<void(sk_sp<SkPixelRef>, float, const gfx::Size&)>
        post_read_task) {
  gfx::Size content_size;
  float scale = 0.f;
  sk_sp<SkPixelRef> compressed_data;

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

  std::move(post_read_task)
      .Run(std::move(compressed_data), scale, content_size);
}

void DeleteTask(base::FilePath file_path) {
  if (base::PathExists(file_path)) {
    base::DeleteFile(file_path);
  }
}

void DecompressTask(
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

  std::move(post_decompression_callback).Run(success, raw_data_small);
}

}  // anonymous namespace

Etc1ThumbnailHelper::Etc1ThumbnailHelper(
    const base::FilePath& base_path,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner)
    : base_path_(base_path),
      default_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      file_task_runner_(file_task_runner) {}

Etc1ThumbnailHelper::~Etc1ThumbnailHelper() {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
}

void Etc1ThumbnailHelper::Compress(
    SkBitmap raw_data,
    gfx::Size encoded_size,
    base::OnceCallback<void(sk_sp<SkPixelRef>, const gfx::Size&)>
        post_compression_task) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&CompressTask, raw_data, encoded_size,
                     base::BindPostTask(default_task_runner_,
                                        std::move(post_compression_task))));
}

void Etc1ThumbnailHelper::Write(TabId tab_id,
                                sk_sp<SkPixelRef> compressed_data,
                                float scale,
                                const gfx::Size& content_size,
                                base::OnceClosure post_write_task) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  base::FilePath file_path = GetFilePath(tab_id);
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WriteTask, file_path, compressed_data, scale,
                     content_size,
                     base::BindPostTask(default_task_runner_,
                                        std::move(post_write_task))));
}

// Note: When calling this function, please check that the correct thread is
// being bound to for post_read_task
void Etc1ThumbnailHelper::Read(
    TabId tab_id,
    base::OnceCallback<void(sk_sp<SkPixelRef>, float, const gfx::Size&)>
        post_read_task) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  base::FilePath file_path = GetFilePath(tab_id);
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ReadTask, file_path, std::move(post_read_task)));
}

void Etc1ThumbnailHelper::Delete(TabId tab_id) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  base::FilePath file_path = GetFilePath(tab_id);
  file_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(&DeleteTask, file_path));
}

void Etc1ThumbnailHelper::Decompress(
    base::OnceCallback<void(bool, const SkBitmap&)> post_decompression_callback,
    sk_sp<SkPixelRef> compressed_data,
    float scale,
    const gfx::Size& content_size) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&DecompressTask,
                     base::BindPostTask(default_task_runner_,
                                        std::move(post_decompression_callback)),
                     compressed_data, scale, content_size));
}

base::FilePath Etc1ThumbnailHelper::GetFilePath(TabId tab_id) {
  return base_path_.Append(base::NumberToString(tab_id));
}

}  // namespace thumbnail
