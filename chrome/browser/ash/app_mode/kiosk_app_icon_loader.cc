// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_app_icon_loader.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/constants.mojom.h"
#include "services/data_decoder/public/cpp/decode_image.h"

using content::BrowserThread;

namespace ash {

namespace {

std::optional<gfx::ImageSkia> CreateResultFromBitmap(const SkBitmap& bitmap) {
  if (bitmap.isNull()) {
    LOG(ERROR) << "Failed to decode icon image.";
    return std::nullopt;
  }
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  image.MakeThreadSafe();
  return image;
}

void LoadOnBlockingPool(const base::FilePath& icon_path,
                        KioskAppIconLoader::ResultCallback result_callback) {
  std::string data;
  if (!base::ReadFileToString(base::FilePath(icon_path), &data)) {
    LOG(ERROR) << "Failed to read icon file.";
    std::move(result_callback).Run(std::nullopt);
    return;
  }

  data_decoder::DecodeImageIsolated(
      base::as_byte_span(data), data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/false,
      static_cast<int64_t>(IPC::mojom::kChannelMaximumMessageSize),
      /*desired_image_frame_size=*/gfx::Size(),
      base::BindOnce(&CreateResultFromBitmap).Then(std::move(result_callback)));
}

}  // namespace

KioskAppIconLoader::KioskAppIconLoader() = default;

KioskAppIconLoader::~KioskAppIconLoader() = default;

void KioskAppIconLoader::Start(const base::FilePath& icon_path,
                               ResultCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);

  // `Start` must not be called multiple times.
  CHECK(!started_);
  started_ = true;

  ResultCallback reply_callback = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&KioskAppIconLoader::OnImageDecoded,
                     weak_factory_.GetWeakPtr(), std::move(callback)));

  base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})
      ->PostTask(FROM_HERE, base::BindOnce(&LoadOnBlockingPool, icon_path,
                                           std::move(reply_callback)));
}

void KioskAppIconLoader::OnImageDecoded(ResultCallback callback,
                                        std::optional<gfx::ImageSkia> result) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);

  std::move(callback).Run(result);
}

}  // namespace ash
