// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chrome/browser/thumbnail/cc/etc1_thumbnail_helper.h"

#include <array>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "third_party/android_opengl/etc1/etc1.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/android/resources/etc1_utils.h"

namespace thumbnail {
namespace {

void CompressTask(SkBitmap raw_data,
                  bool supports_etc_non_power_of_two,
                  base::OnceCallback<void(sk_sp<SkPixelRef>, const gfx::Size&)>
                      post_compression_task) {
  sk_sp<SkPixelRef> compressed_data =
      ui::Etc1::CompressBitmap(raw_data, supports_etc_non_power_of_two);
  gfx::Size content_size = compressed_data
                               ? gfx::Size(raw_data.width(), raw_data.height())
                               : gfx::Size();
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

  bool success =
      ui::Etc1::WriteToFile(&file, content_size, scale, compressed_data);

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
        ui::Etc1::ReadFromFile(&file, &content_size, &scale, &compressed_data);
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
    bool supports_etc_non_power_of_two,
    base::OnceCallback<void(sk_sp<SkPixelRef>, const gfx::Size&)>
        post_compression_task) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&CompressTask, raw_data, supports_etc_non_power_of_two,
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
