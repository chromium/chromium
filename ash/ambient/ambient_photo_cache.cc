// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_photo_cache.h"

#include "ash/ambient/ambient_constants.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

namespace {

void ToImageSkia(base::OnceCallback<void(const gfx::ImageSkia&)> callback,
                 const SkBitmap& image) {
  if (image.isNull()) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(image);
  image_skia.MakeThreadSafe();

  std::move(callback).Run(image_skia);
}

// Helper function to extract response code from |SimpleURLLoader|.
int GetResponseCode(network::SimpleURLLoader* simple_loader) {
  if (simple_loader->ResponseInfo() && simple_loader->ResponseInfo()->headers)
    return simple_loader->ResponseInfo()->headers->response_code();
  else
    return -1;
}

std::unique_ptr<network::SimpleURLLoader> CreateSimpleURLLoader(
    const std::string& url) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(url);
  resource_request->method = "GET";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  return network::SimpleURLLoader::Create(std::move(resource_request),
                                          NO_TRAFFIC_ANNOTATION_YET);
}

// Implementation of |AmbientPhotoCache|.
class AmbientPhotoCacheImpl : public AmbientPhotoCache {
 public:
  AmbientPhotoCacheImpl()
      : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {}
  ~AmbientPhotoCacheImpl() override = default;

  // AmbientPhotoCache:
  void DownloadPhoto(const std::string& url,
                     base::OnceCallback<void(std::unique_ptr<std::string>)>
                         callback) override {
    auto simple_loader = CreateSimpleURLLoader(url);
    auto* loader_ptr = simple_loader.get();
    auto loader_factory = AmbientClient::Get()->GetURLLoaderFactory();
    loader_ptr->DownloadToString(
        loader_factory.get(),
        base::BindOnce(&AmbientPhotoCacheImpl::OnUrlDownloaded,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       std::move(simple_loader), loader_factory),
        kMaxImageSizeInBytes);
  }

  void DownloadPhotoToFile(const std::string& url,
                           base::OnceCallback<void(base::FilePath)> callback,
                           const base::FilePath& file_path) override {
    auto simple_loader = CreateSimpleURLLoader(url);
    auto loader_factory = AmbientClient::Get()->GetURLLoaderFactory();
    auto* loader_ptr = simple_loader.get();
    auto* loader_factory_ptr = loader_factory.get();

    // Create a temporary file path as target for download to guard against race
    // conditions in reading.
    base::FilePath temp_path = file_path.DirName().Append(
        base::UnguessableToken::Create().ToString() + kPhotoFileExt);

    // Download to temp file first to guarantee entire image is written without
    // errors before attempting to read it.
    loader_ptr->DownloadToFile(
        loader_factory_ptr,
        base::BindOnce(&AmbientPhotoCacheImpl::OnUrlDownloadedToFile,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       std::move(simple_loader), std::move(loader_factory),
                       file_path),
        temp_path);
  }

  void DecodePhoto(
      std::unique_ptr<std::string> data,
      base::OnceCallback<void(const gfx::ImageSkia&)> callback) override {
    std::vector<uint8_t> image_bytes(data->begin(), data->end());
    data_decoder::DecodeImageIsolated(
        image_bytes, data_decoder::mojom::ImageCodec::DEFAULT,
        /*shrink_to_fit=*/true, data_decoder::kDefaultMaxSizeInBytes,
        /*desired_image_frame_size=*/gfx::Size(),
        base::BindOnce(&ToImageSkia, std::move(callback)));
  }

 private:
  void OnUrlDownloaded(
      base::OnceCallback<void(std::unique_ptr<std::string>)> callback,
      std::unique_ptr<network::SimpleURLLoader> simple_loader,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      std::unique_ptr<std::string> response_body) {
    if (simple_loader->NetError() == net::OK && response_body) {
      std::move(callback).Run(std::move(response_body));
      return;
    }

    LOG(ERROR) << "Downloading to string failed with error code: "
               << GetResponseCode(simple_loader.get()) << " with network error"
               << simple_loader->NetError();
    std::move(callback).Run(std::make_unique<std::string>());
  }

  void OnUrlDownloadedToFile(
      base::OnceCallback<void(base::FilePath)> callback,
      std::unique_ptr<network::SimpleURLLoader> simple_loader,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      const base::FilePath& desired_path,
      base::FilePath temp_path) {
    if (simple_loader->NetError() != net::OK || temp_path.empty()) {
      LOG(ERROR) << "Downloading to file failed with error code: "
                 << GetResponseCode(simple_loader.get())
                 << " with network error" << simple_loader->NetError();

      if (!temp_path.empty()) {
        // Clean up temporary file.
        task_runner_->PostTask(FROM_HERE, base::BindOnce(
                                              [](const base::FilePath& path) {
                                                base::DeleteFile(path);
                                              },
                                              temp_path));
      }
      std::move(callback).Run(base::FilePath());
      return;
    }

    // Swap the temporary file to the desired path, and then run the callback.
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(
            [](const base::FilePath& to_path, const base::FilePath& from_path) {
              if (!base::ReplaceFile(from_path, to_path,
                                     /*error=*/nullptr)) {
                LOG(ERROR)
                    << "Unable to move downloaded file to ambient directory";
                // Clean up the files.
                base::DeleteFile(from_path);
                base::DeleteFile(to_path);
                return base::FilePath();
              }
              return to_path;
            },
            desired_path, temp_path),
        std::move(callback));
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<AmbientPhotoCacheImpl> weak_factory_{this};
};

}  // namespace

// static
std::unique_ptr<AmbientPhotoCache> AmbientPhotoCache::Create() {
  return std::make_unique<AmbientPhotoCacheImpl>();
}

}  // namespace ash
