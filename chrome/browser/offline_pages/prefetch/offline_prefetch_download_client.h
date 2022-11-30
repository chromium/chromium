// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_OFFLINE_PREFETCH_DOWNLOAD_CLIENT_H_
#define CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_OFFLINE_PREFETCH_DOWNLOAD_CLIENT_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/download/public/background_service/client.h"

class SimpleFactoryKey;

namespace download {
struct CompletionInfo;
struct DownloadMetaData;
}  // namespace download

namespace offline_pages {

class PrefetchDownloader;

class OfflinePrefetchDownloadClient : public download::Client {
 public:
  explicit OfflinePrefetchDownloadClient(SimpleFactoryKey* simple_factory_key);

  OfflinePrefetchDownloadClient(const OfflinePrefetchDownloadClient&) = delete;
  OfflinePrefetchDownloadClient& operator=(
      const OfflinePrefetchDownloadClient&) = delete;

  ~OfflinePrefetchDownloadClient() override;

 private:
  // Overridden from Client:
  void OnServiceInitialized(
      bool state_lost,
      const std::vector<download::DownloadMetaData>& downloads) override;
  void OnServiceUnavailable() override;
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

  raw_ptr<SimpleFactoryKey> simple_factory_key_;
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_OFFLINE_PREFETCH_DOWNLOAD_CLIENT_H_
