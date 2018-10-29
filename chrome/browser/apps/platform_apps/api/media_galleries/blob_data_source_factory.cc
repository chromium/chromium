// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/media_galleries/blob_data_source_factory.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/blob_reader.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace chrome_apps {
namespace api {
namespace {

// Media data source that reads data from a blob in browser process.
class BlobMediaDataSource : public chrome::mojom::MediaDataSource {
 public:
  BlobMediaDataSource(chrome::mojom::MediaDataSourcePtr* interface_ptr,
                      content::BrowserContext* browser_context,
                      const std::string& blob_uuid,
                      BlobDataSourceFactory::MediaDataCallback callback)
      : binding_(this, mojo::MakeRequest(interface_ptr)),
        browser_context_(browser_context),
        blob_uuid_(blob_uuid),
        callback_(callback),
        weak_factory_(this) {}

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
    BlobReader* reader = new BlobReader(  // BlobReader is self-deleting.
        browser_context_, blob_uuid_,
        base::BindRepeating(&BlobMediaDataSource::OnBlobReaderDone,
                            weak_factory_.GetWeakPtr(),
                            base::Passed(&callback)));
    reader->SetByteRange(position, length);
    reader->Start();
  }

  void OnBlobReaderDone(chrome::mojom::MediaDataSource::ReadCallback callback,
                        std::unique_ptr<std::string> data,
                        int64_t size) {
    callback_.Run(std::move(callback), std::move(data));
  }

  mojo::Binding<chrome::mojom::MediaDataSource> binding_;

  content::BrowserContext* const browser_context_;
  std::string blob_uuid_;

  BlobDataSourceFactory::MediaDataCallback callback_;

  base::WeakPtrFactory<BlobMediaDataSource> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlobMediaDataSource);
};

}  // namespace

BlobDataSourceFactory::BlobDataSourceFactory(
    content::BrowserContext* browser_context,
    const std::string& blob_uuid)
    : browser_context_(browser_context), blob_uuid_(blob_uuid) {}

BlobDataSourceFactory::~BlobDataSourceFactory() = default;

std::unique_ptr<chrome::mojom::MediaDataSource>
BlobDataSourceFactory::CreateMediaDataSource(
    chrome::mojom::MediaDataSourcePtr* request,
    MediaDataCallback media_data_callback) {
  return std::make_unique<BlobMediaDataSource>(request, browser_context_,
                                               blob_uuid_, media_data_callback);
}

}  // namespace api
}  // namespace chrome_apps
