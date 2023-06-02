// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_DOWNLOAD_CLIENT_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_DOWNLOAD_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/bruschetta/bruschetta_installer.h"
#include "components/download/public/background_service/client.h"

class Profile;

namespace bruschetta {

class BruschettaDownloadClient : public download::Client {
 public:
  explicit BruschettaDownloadClient(Profile* profile);

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
                        FailureReason reason) override;
  void OnDownloadSucceeded(
      const std::string& guid,
      const download::CompletionInfo& completion_info) override;
  bool CanServiceRemoveDownloadedFile(const std::string& guid,
                                      bool force_delete) override;
  void GetUploadData(const std::string& guid,
                     download::GetUploadDataCallback callback) override;

  static void SetInstallerInstance(BruschettaInstaller* instance);

 private:
  // base::raw_ptr can't be used as a static member variable
  static BruschettaInstaller* installer_;

  bool MaybeCancelDownload(const std::string& guid);

  const raw_ptr<Profile> profile_;
};

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_DOWNLOAD_CLIENT_H_
