// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DOWNLOAD_CLIENT_H_
#define CHROME_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DOWNLOAD_CLIENT_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/download/public/background_service/client.h"

class BackgroundFetchDelegateImpl;

namespace content {
class BrowserContext;
}  // namespace content

// A DownloadService client used by BackgroundFetch. Mostly this just forwards
// calls to BackgroundFetchDelegateImpl.
class BackgroundFetchDownloadClient : public download::Client {
 public:
  explicit BackgroundFetchDownloadClient(content::BrowserContext* context);
  ~BackgroundFetchDownloadClient() override;

 private:
  // Lazily initializes and returns |delegate_| as a raw pointer.
  BackgroundFetchDelegateImpl* GetDelegate();

  // download::Client implementation
  void OnServiceInitialized(
      bool state_lost,
      const std::vector<download::DownloadMetaData>& downloads) override;
  void OnServiceUnavailable() override;
  void OnDownloadStarted(
      const std::string& guid,
      const std::vector<GURL>& url_chain,
      const scoped_refptr<const net::HttpResponseHeaders>& headers) override;
  void OnDownloadUpdated(const std::string& guid,
                         uint64_t bytes_uploaded,
                         uint64_t bytes_downloaded) override;
  void OnDownloadFailed(const std::string& guid,
                        const download::CompletionInfo& info,
                        download::Client::FailureReason reason) override;
  void OnDownloadSucceeded(const std::string& guid,
                           const download::CompletionInfo& info) override;
  bool CanServiceRemoveDownloadedFile(const std::string& guid,
                                      bool force_delete) override;
  void GetUploadData(const std::string& guid,
                     download::GetUploadDataCallback callback) override;

  content::BrowserContext* browser_context_;
  base::WeakPtr<BackgroundFetchDelegateImpl> delegate_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchDownloadClient);
};

#endif  // CHROME_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DOWNLOAD_CLIENT_H_
