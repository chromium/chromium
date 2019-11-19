// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DEFERRED_CLIENT_WRAPPER_H_
#define CHROME_BROWSER_DOWNLOAD_DEFERRED_CLIENT_WRAPPER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/download/public/background_service/client.h"
#include "components/download/public/background_service/clients.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

class SimpleFactoryKey;
class Profile;

namespace download {

using ClientFactory = base::OnceCallback<std::unique_ptr<Client>(Profile*)>;

class DeferredClientWrapper : public Client {
 public:
  DeferredClientWrapper(DownloadClient client_id,
                        ClientFactory factory,
                        SimpleFactoryKey* key);
  ~DeferredClientWrapper() override;

  // Client implementation.
  void OnServiceInitialized(
      bool state_lost,
      const std::vector<DownloadMetaData>& downloads) override;
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
                        FailureReason reason) override;
  void OnDownloadSucceeded(const std::string& guid,
                           const CompletionInfo& completion_info) override;
  bool CanServiceRemoveDownloadedFile(const std::string& guid,
                                      bool force_delete) override;
  void GetUploadData(const std::string& guid,
                     GetUploadDataCallback callback) override;

 private:
  // Forwarding functions
  void ForwardOnServiceInitialized(
      bool state_lost,
      const std::vector<DownloadMetaData>& downloads);
  void ForwardOnServiceUnavailable();
  void ForwardOnDownloadStarted(
      const std::string& guid,
      const std::vector<GURL>& url_chain,
      const scoped_refptr<const net::HttpResponseHeaders>& headers);
  void ForwardOnDownloadUpdated(const std::string& guid,
                                uint64_t bytes_uploaded,
                                uint64_t bytes_downloaded);
  void ForwardOnDownloadFailed(const std::string& guid,
                               const download::CompletionInfo& info,
                               FailureReason reason);
  void ForwardOnDownloadSucceeded(const std::string& guid,
                                  const CompletionInfo& completion_info);
  void ForwardCanServiceRemoveDownloadedFile(const std::string& guid,
                                             bool force_delete);
  void ForwardGetUploadData(const std::string& guid,
                            GetUploadDataCallback callback);

  void RunDeferredClosures(bool force_inflate);
  void DoRunDeferredClosures();
  void InflateClient(Profile* profile);

  std::unique_ptr<download::Client> wrapped_client_;
  std::vector<base::OnceClosure> deferred_closures_;
  ClientFactory client_factory_;
  SimpleFactoryKey* key_;

#if defined(OS_ANDROID)
  // This is needed to record UMA for when DownloadClientWrapper requests a full
  // browser start while the browser is running in reduced mode. Reduced mode is
  // only on Android so it is ifdefed out on other platforms to prevent the
  // compiler from complaining that it is unused.
  DownloadClient client_id_;
  bool full_browser_requested_;
  void LaunchFullBrowser();
#endif

  base::WeakPtrFactory<DeferredClientWrapper> weak_ptr_factory_{this};
};

}  // namespace download

#endif  // CHROME_BROWSER_DOWNLOAD_DEFERRED_CLIENT_WRAPPER_H_
