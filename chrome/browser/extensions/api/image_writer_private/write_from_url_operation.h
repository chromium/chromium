// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_WRITE_FROM_URL_OPERATION_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_WRITE_FROM_URL_OPERATION_H_

#include <stdint.h>

#include "chrome/browser/extensions/api/image_writer_private/operation.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace extensions {
namespace image_writer {

class OperationManager;

// Encapsulates a write of an image accessed via URL.
class WriteFromUrlOperation : public Operation {
 public:
  WriteFromUrlOperation(
      base::WeakPtr<OperationManager> manager,
      const ExtensionId& extension_id,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote,
      GURL url,
      const std::string& hash,
      const std::string& storage_unit_id,
      const base::FilePath& download_folder);
  void StartImpl() override;

 protected:
  friend class OperationForTest;
  friend class WriteFromUrlOperationForTest;

  ~WriteFromUrlOperation() override;

  // Sets the image_path to the correct location to download to.
  void GetDownloadTarget(base::OnceClosure continuation);

  // Downloads the |url| to the currently configured |image_path|.  Should not
  // be called without calling |GetDownloadTarget| first.
  void Download(base::OnceClosure continuation);

  // Verifies the download matches |hash|.  If the hash is empty, this stage is
  // skipped.
  void VerifyDownload(base::OnceClosure continuation);

 private:
  void DestroySimpleURLLoader();
  void OnResponseStarted(const GURL& final_url,
                         const network::mojom::URLResponseHead& response_head);
  void OnDataDownloaded(uint64_t current);
  void OnSimpleLoaderComplete(base::FilePath file_path);
  void VerifyDownloadCompare(base::OnceClosure continuation,
                             const std::string& download_hash);
  void VerifyDownloadComplete(base::OnceClosure continuation);

  // Arguments
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      url_loader_factory_remote_;
  GURL url_;
  const std::string hash_;

  // Local state
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  base::OnceClosure download_continuation_;
  int total_response_bytes_ = -1;
};

} // namespace image_writer
} // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_WRITE_FROM_URL_OPERATION_H_
