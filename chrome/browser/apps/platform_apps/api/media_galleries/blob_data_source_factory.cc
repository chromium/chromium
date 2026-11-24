// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/media_galleries/blob_data_source_factory.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/blob_reader.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace chrome_apps {
namespace api {
namespace {

// Media data source that reads data from a blob in browser process.
class BlobMediaDataSource : public chrome::mojom::MediaDataSource {
 public:
  BlobMediaDataSource(
      mojo::PendingReceiver<chrome::mojom::MediaDataSource> receiver,
      mojo::PendingRemote<blink::mojom::Blob> blob,
      BlobDataSourceFactory::MediaDataCallback callback)
      : receiver_(this, std::move(receiver)),
        blob_(std::move(blob)),
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
    mojo::PendingRemote<blink::mojom::Blob> blob;
    blob_->Clone(blob.InitWithNewPipeAndPassReceiver());
    BlobReader::Read(
        std::move(blob),
        base::BindOnce(&BlobMediaDataSource::OnBlobReaderDone,
                       weak_factory_.GetWeakPtr(), std::move(callback)),
        position, length);
  }

  void OnBlobReaderDone(chrome::mojom::MediaDataSource::ReadCallback callback,
                        std::string data,
                        int64_t /*size*/) {
    // BlobReader does not expose read status separately from an empty blob.
    // Preserve its existing behavior until it can report failures.
    callback_.Run(std::move(callback), /*success=*/true, std::move(data));
  }

  mojo::Receiver<chrome::mojom::MediaDataSource> receiver_;

  mojo::Remote<blink::mojom::Blob> blob_;

  BlobDataSourceFactory::MediaDataCallback callback_;

  base::WeakPtrFactory<BlobMediaDataSource> weak_factory_{this};
};

}  // namespace

BlobDataSourceFactory::BlobDataSourceFactory(
    mojo::Remote<blink::mojom::Blob> blob)
    : blob_(std::move(blob)) {}

BlobDataSourceFactory::~BlobDataSourceFactory() = default;

std::unique_ptr<chrome::mojom::MediaDataSource>
BlobDataSourceFactory::CreateMediaDataSource(
    mojo::PendingReceiver<chrome::mojom::MediaDataSource> receiver,
    MediaDataCallback media_data_callback) {
  mojo::PendingRemote<blink::mojom::Blob> blob;
  blob_->Clone(blob.InitWithNewPipeAndPassReceiver());
  return std::make_unique<BlobMediaDataSource>(
      std::move(receiver), std::move(blob), media_data_callback);
}

}  // namespace api
}  // namespace chrome_apps
