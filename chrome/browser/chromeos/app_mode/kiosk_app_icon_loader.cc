// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_icon_loader.h"

#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/image_decoder.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

class IconImageRequest : public ImageDecoder::ImageRequest {
 public:
  IconImageRequest(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                   KioskAppIconLoader::ResultCallback result_callback)
      : ImageRequest(task_runner), result_callback_(result_callback) {}

  void OnImageDecoded(const SkBitmap& decoded_image) override {
    gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(decoded_image);
    image.MakeThreadSafe();
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(result_callback_, image));
    delete this;
  }

  void OnDecodeImageFailed() override {
    LOG(ERROR) << "Failed to decode icon image.";
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(result_callback_, base::Optional<gfx::ImageSkia>()));
    delete this;
  }

 private:
  ~IconImageRequest() override = default;
  KioskAppIconLoader::ResultCallback result_callback_;
};

void LoadOnBlockingPool(
    const base::FilePath& icon_path,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    KioskAppIconLoader::ResultCallback result_callback) {
  DCHECK(callback_task_runner->RunsTasksInCurrentSequence());

  std::string data;
  if (!base::ReadFileToString(base::FilePath(icon_path), &data)) {
    LOG(ERROR) << "Failed to read icon file.";
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(result_callback, base::Optional<gfx::ImageSkia>()));
    return;
  }

  // IconImageRequest will delete itself on completion of ImageDecoder callback.
  IconImageRequest* image_request =
      new IconImageRequest(callback_task_runner, result_callback);
  ImageDecoder::Start(image_request,
                      std::vector<uint8_t>(data.begin(), data.end()));
}

KioskAppIconLoader::KioskAppIconLoader(Delegate* delegate)
    : delegate_(delegate) {}

KioskAppIconLoader::~KioskAppIconLoader() = default;

void KioskAppIconLoader::Start(const base::FilePath& icon_path) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&LoadOnBlockingPool, icon_path, task_runner,
                     base::Bind(&KioskAppIconLoader::OnImageDecodingFinished,
                                weak_factory_.GetWeakPtr())));
}

void KioskAppIconLoader::OnImageDecodingFinished(
    base::Optional<gfx::ImageSkia> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (result.has_value()) {
    delegate_->OnIconLoadSuccess(result.value());
  } else {
    delegate_->OnIconLoadFailure();
  }
}

}  // namespace chromeos
