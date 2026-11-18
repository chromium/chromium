// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_API_MEDIA_GALLERIES_BLOB_DATA_SOURCE_FACTORY_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_API_MEDIA_GALLERIES_BLOB_DATA_SOURCE_FACTORY_H_

#include <memory>

#include "components/media_gallery_util/public/mojom/media_parser.mojom.h"
#include "components/media_gallery_util/safe_media_metadata_parser.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace chrome_apps {
namespace api {

// Factory to provide media data source for extension media gallery API.
// Internally it will read media data from a blob in browser process.
class BlobDataSourceFactory
    : public SafeMediaMetadataParser::MediaDataSourceFactory {
 public:
  explicit BlobDataSourceFactory(mojo::Remote<blink::mojom::Blob> blob);
  BlobDataSourceFactory(const BlobDataSourceFactory&) = delete;
  BlobDataSourceFactory& operator=(const BlobDataSourceFactory&) = delete;
  ~BlobDataSourceFactory() override;

 private:
  // SafeMediaMetadataParser::MediaDataSourceFactory implementation.
  std::unique_ptr<chrome::mojom::MediaDataSource> CreateMediaDataSource(
      mojo::PendingReceiver<chrome::mojom::MediaDataSource> receiver,
      MediaDataCallback media_data_callback) override;

  mojo::Remote<blink::mojom::Blob> blob_;
  MediaDataCallback callback_;
};

}  // namespace api
}  // namespace chrome_apps

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_API_MEDIA_GALLERIES_BLOB_DATA_SOURCE_FACTORY_H_
