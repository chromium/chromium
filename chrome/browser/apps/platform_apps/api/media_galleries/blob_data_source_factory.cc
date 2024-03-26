// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/media_galleries/blob_data_source_factory.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/blob_reader.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chrome_apps {
namespace api {
namespace {

// Media data source that reads data from a blob in browser process.
class BlobMediaDataSource : public chrome::mojom::MediaDataSource {
 public:
  BlobMediaDataSource(
      mojo::PendingReceiver<chrome::mojom::MediaDataSource> receiver,
      content::BrowserContext* browser_context,
      const std::string& blob_uuid,
      BlobDataSourceFactory::MediaDataCallback callback)
      : receiver_(this, std::move(receiver)),
        browser_context_(browser_context),
        blob_uuid_(blob_uuid),
        callback_(callback) {}

  BlobMediaDataSource(const BlobMediaDataSource&) = delete;
  BlobMediaDataSource& operator=(const BlobMediaDataSource&) = delete;
  ~BlobMediaDataSource() override = default;

 private:
  // chrome::mojom::MediaDataSource implementation.
  void Read(int64_t position,
            int64_t length,
            chrome::mojom::MediaDataSource::ReadCallback callback) override {
    StartBlobRequest(std::move(callback), position, length);
  }

  void StartBlobRequest(chrome::mojom::MediaDataSource::ReadCallback callback,
                        int64_t position,
                        int64_t length) {
    BlobReader::Read(
        browser_context_->GetBlobRemote(blob_uuid_),
        base::BindOnce(&BlobMediaDataSource::OnBlobReaderDone,
                       weak_factory_.GetWeakPtr(), std::move(callback)),
        position, length);
  }

  void OnBlobReaderDone(chrome::mojom::MediaDataSource::ReadCallback callback,
                        std::string data,
                        int64_t /*size*/) {
    callback_.Run(std::move(callback), std::move(data));
  }

  mojo::Receiver<chrome::mojom::MediaDataSource> receiver_;

  const raw_ptr<content::BrowserContext, LeakedDanglingUntriaged>
      browser_context_;
  std::string blob_uuid_;

  BlobDataSourceFactory::MediaDataCallback callback_;

  base::WeakPtrFactory<BlobMediaDataSource> weak_factory_{this};
};

}  // namespace

BlobDataSourceFactory::BlobDataSourceFactory(
    content::BrowserContext* browser_context,
    const std::string& blob_uuid)
    : browser_context_(browser_context), blob_uuid_(blob_uuid) {}

BlobDataSourceFactory::~BlobDataSourceFactory() = default;

std::unique_ptr<chrome::mojom::MediaDataSource>
BlobDataSourceFactory::CreateMediaDataSource(
    mojo::PendingReceiver<chrome::mojom::MediaDataSource> receiver,
    MediaDataCallback media_data_callback) {
  return std::make_unique<BlobMediaDataSource>(
      std::move(receiver), browser_context_, blob_uuid_, media_data_callback);
}

}  // namespace api
}  // namespace chrome_apps
