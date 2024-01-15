// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_IMAGE_DOWNLOAD_CLIENT_H_
#define CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_IMAGE_DOWNLOAD_CLIENT_H_

#include <optional>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/download/public/background_service/client.h"

namespace download {
struct CompletionInfo;
struct DownloadMetaData;
}  // namespace download

class Profile;

namespace plugin_vm {

class PluginVmInstaller;

class PluginVmImageDownloadClient : public download::Client {
 public:
  explicit PluginVmImageDownloadClient(Profile* profile);

  PluginVmImageDownloadClient(const PluginVmImageDownloadClient&) = delete;
  PluginVmImageDownloadClient& operator=(const PluginVmImageDownloadClient&) =
      delete;

  ~PluginVmImageDownloadClient() override;

 private:
  raw_ptr<Profile> profile_ = nullptr;
  int64_t content_length_ = -1;
  int response_code_ = -1;

  PluginVmInstaller* GetInstaller();
  // Returns false for cancelled downloads.
  bool IsCurrentDownload(const std::string& guid);

  // download::Client implementation.
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
                        const download::CompletionInfo& completion_info,
                        download::Client::FailureReason reason) override;
  void OnDownloadSucceeded(
      const std::string& guid,
      const download::CompletionInfo& completion_info) override;
  bool CanServiceRemoveDownloadedFile(const std::string& guid,
                                      bool force_delete) override;
  void GetUploadData(const std::string& guid,
                     download::GetUploadDataCallback callback) override;

  std::optional<double> GetFractionComplete(int64_t bytes_downloaded);
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_IMAGE_DOWNLOAD_CLIENT_H_
