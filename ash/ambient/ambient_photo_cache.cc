// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_photo_cache.h"

#include "ash/ambient/ambient_constants.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
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

bool CreateDirIfNotExists(const base::FilePath& path) {
  return base::DirectoryExists(path) || base::CreateDirectory(path);
}

// Writes |data| to |path| if |data| is not nullptr and is not empty. If |data|
// is nullptr or empty, will delete any existing file at |path|.
bool WriteOrDeleteFile(const base::FilePath& path,
                       const std::string* const data) {
  if (!data || data->empty())
    return base::DeleteFile(path);

  if (!CreateDirIfNotExists(path.DirName())) {
    LOG(ERROR) << "Cannot create ambient mode directory.";
    return false;
  }

  if (base::SysInfo::AmountOfFreeDiskSpace(path.DirName()) <
      kMaxReservedAvailableDiskSpaceByte) {
    LOG(ERROR) << "Not enough disk space left.";
    return false;
  }

  // Create a temp file.
  base::FilePath temp_file;
  if (!base::CreateTemporaryFileInDir(path.DirName(), &temp_file)) {
    LOG(ERROR) << "Cannot create a temporary file.";
    return false;
  }

  // Write to the tmp file.
  const int size = data->size();
  int written_size = base::WriteFile(temp_file, data->data(), size);
  if (written_size != size) {
    LOG(ERROR) << "Cannot write the temporary file.";
    base::DeleteFile(temp_file);
    return false;
  }

  // Replace the current file with the temp file.
  if (!base::ReplaceFile(temp_file, path, /*error=*/nullptr)) {
    LOG(ERROR) << "Cannot replace the temporary file.";
    base::DeleteFile(temp_file);
    return false;
  }

  return true;
}

base::FilePath GetPhotoPath(int cache_index,
                            const base::FilePath& root_path,
                            bool is_related = false) {
  std::string file_ext;

  // "_r.img" for related files, ".img" otherwise
  if (is_related)
    file_ext += kRelatedPhotoSuffix;

  file_ext += kPhotoFileExt;

  return root_path.Append(base::NumberToString(cache_index) + file_ext);
}

base::FilePath GetDetailsPath(int cache_index,
                              const base::FilePath& root_path) {
  return GetPhotoPath(cache_index, root_path)
      .RemoveExtension()
      .AddExtension(kPhotoDetailsFileExt);
}

// -----------------AmbientPhotoCacheImpl---------------------------------------

class AmbientPhotoCacheImpl : public AmbientPhotoCache {
 public:
  explicit AmbientPhotoCacheImpl(base::FilePath path)
      : root_directory_(path),
        task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {}
  ~AmbientPhotoCacheImpl() override = default;

  // AmbientPhotoCache:
  void DownloadPhoto(const std::string& url,
                     base::OnceCallback<void(std::unique_ptr<std::string>)>
                         callback) override {
    std::unique_ptr<network::SimpleURLLoader> simple_loader =
        CreateSimpleURLLoader(url);
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
        AmbientClient::Get()->GetURLLoaderFactory();
    auto* loader_ptr = simple_loader.get();

    loader_ptr->DownloadToString(
        loader_factory.get(),
        base::BindOnce(&AmbientPhotoCacheImpl::OnUrlDownloaded,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       std::move(simple_loader), loader_factory),
        kMaxImageSizeInBytes);
  }

  void DownloadPhotoToFile(const std::string& url,
                           int cache_index,
                           bool is_related,
                           base::OnceCallback<void(bool)> callback) override {
    auto file_path = GetPhotoPath(cache_index, root_directory_, is_related);
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(
            [](const base::FilePath& path) {
              if (!CreateDirIfNotExists(path))
                LOG(ERROR) << "Cannot create ambient mode directory";
            },
            root_directory_),
        base::BindOnce(&AmbientPhotoCacheImpl::DownloadPhotoToFileInternal,
                       weak_factory_.GetWeakPtr(), url, std::move(callback),
                       file_path));
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

  void WriteFiles(int cache_index,
                  const std::string* const image,
                  const std::string* const details,
                  const std::string* const related_image,
                  base::OnceClosure callback) override {
    DCHECK_LT(cache_index, kMaxNumberOfCachedImages);
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(
            [](int cache_index, const base::FilePath& root_path,
               const std::string* const image, const std::string* const details,
               const std::string* const related_image) {
              bool success = true;

              auto image_path = GetPhotoPath(cache_index, root_path);
              success = success && WriteOrDeleteFile(image_path, image);

              auto details_path = GetDetailsPath(cache_index, root_path);
              success = success && WriteOrDeleteFile(details_path, details);

              auto related_image_path =
                  GetPhotoPath(cache_index, root_path, /*is_related=*/true);
              success = success &&
                        WriteOrDeleteFile(related_image_path, related_image);

              if (!success) {
                LOG(WARNING) << "Error writing files";
                base::DeleteFile(image_path);
                base::DeleteFile(details_path);
                base::DeleteFile(related_image_path);
              }
            },
            cache_index, root_directory_, image, details, related_image),
        std::move(callback));
  }

  void ReadFiles(int cache_index,
                 base::OnceCallback<void(PhotoCacheEntry)> callback) override {
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(
            [](int cache_index, const base::FilePath& root_path) {
              auto image = std::make_unique<std::string>();
              auto details = std::make_unique<std::string>();
              auto related_image = std::make_unique<std::string>();

              auto image_path = GetPhotoPath(cache_index, root_path);

              if (!base::ReadFileToString(image_path, image.get()))
                image->clear();

              auto details_path = GetDetailsPath(cache_index, root_path);

              if (!base::ReadFileToString(details_path, details.get()))
                details->clear();

              auto related_path =
                  GetPhotoPath(cache_index, root_path, /*is_related=*/true);

              if (!base::ReadFileToString(related_path, related_image.get()))
                related_image->clear();

              return PhotoCacheEntry(std::move(image), std::move(details),
                                     std::move(related_image));
            },
            cache_index, root_directory_),
        std::move(callback));
  }

  void Clear() override {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(
                               [](const base::FilePath& file_path) {
                                 base::DeletePathRecursively(file_path);
                               },
                               root_directory_));
  }

 private:
  void DownloadPhotoToFileInternal(const std::string& url,
                                   base::OnceCallback<void(bool)> callback,
                                   const base::FilePath& file_path) {
    std::unique_ptr<network::SimpleURLLoader> simple_loader =
        CreateSimpleURLLoader(url);
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
        AmbientClient::Get()->GetURLLoaderFactory();
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
      base::OnceCallback<void(bool)> callback,
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
      std::move(callback).Run(false);
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
                return false;
              }
              return true;
            },
            desired_path, temp_path),
        std::move(callback));
  }

  const base::FilePath root_directory_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<AmbientPhotoCacheImpl> weak_factory_{this};
};

}  // namespace

// ---------------- PhotoCacheRead --------------------------------------------

PhotoCacheEntry::PhotoCacheEntry() = default;

PhotoCacheEntry::PhotoCacheEntry(std::unique_ptr<std::string> image,
                                 std::unique_ptr<std::string> details,
                                 std::unique_ptr<std::string> related_image)
    : image(std::move(image)),
      details(std::move(details)),
      related_image(std::move(related_image)) {}

PhotoCacheEntry::PhotoCacheEntry(PhotoCacheEntry&&) = default;

PhotoCacheEntry::~PhotoCacheEntry() = default;

void PhotoCacheEntry::reset() {
  image.reset();
  details.reset();
  related_image.reset();
}

// -------------- AmbientPhotoCache --------------------------------------------

// static
std::unique_ptr<AmbientPhotoCache> AmbientPhotoCache::Create(
    base::FilePath root_path) {
  return std::make_unique<AmbientPhotoCacheImpl>(root_path);
}

}  // namespace ash
