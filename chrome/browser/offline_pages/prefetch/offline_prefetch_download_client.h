// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_OFFLINE_PREFETCH_DOWNLOAD_CLIENT_H_
#define CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_OFFLINE_PREFETCH_DOWNLOAD_CLIENT_H_

#include "base/macros.h"
#include "components/download/public/background_service/client.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace download {
struct CompletionInfo;
struct DownloadMetaData;
}  // namespace download

namespace offline_pages {

class PrefetchDownloader;

class OfflinePrefetchDownloadClient : public download::Client {
 public:
  explicit OfflinePrefetchDownloadClient(content::BrowserContext* context);
  ~OfflinePrefetchDownloadClient() override;

 private:
  // Overridden from Client:
  void OnServiceInitialized(
      bool state_lost,
      const std::vector<download::DownloadMetaData>& downloads) override;
  void OnServiceUnavailable() override;
  download::Client::ShouldDownload OnDownloadStarted(
      const std::string& guid,
      const std::vector<GURL>& url_chain,
      const scoped_refptr<const net::HttpResponseHeaders>& headers) override;
  void OnDownloadUpdated(const std::string& guid,
                         uint64_t bytes_downloaded) override;
  void OnDownloadFailed(const std::string& guid,
                        const download::CompletionInfo& completion_info,
                        download::Client::FailureReason reason) override;
  void OnDownloadSucceeded(
      const std::string& guid,
      const download::CompletionInfo& completion_info) override;
  bool CanServiceRemoveDownloadedFile(const std::string& guid,
                                      bool force_delete) override;
  void GetUploadData(const std::string& guid,
                     download::GetUploadDataCallback callback) override;

  PrefetchDownloader* GetPrefetchDownloader() const;

  content::BrowserContext* context_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePrefetchDownloadClient);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_OFFLINE_PREFETCH_DOWNLOAD_CLIENT_H_
