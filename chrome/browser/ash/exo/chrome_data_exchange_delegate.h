// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXO_CHROME_DATA_EXCHANGE_DELEGATE_H_
#define CHROME_BROWSER_ASH_EXO_CHROME_DATA_EXCHANGE_DELEGATE_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "components/exo/data_exchange_delegate.h"

namespace ash {

// Translate paths from |source| VM to valid paths in the host. Invalid paths
// are ignored.
std::vector<base::FilePath> TranslateVMPathsToHost(
    ui::EndpointType source,
    const std::vector<ui::FileInfo>& vm_paths);

// Share |files| with |target| VM, and translate |files| to be "file://" URLs
// which can be used inside the vm. |callback| is invoked with translated
// "file://" URLs.
void ShareWithVMAndTranslateToFileUrls(
    ui::EndpointType target,
    const std::vector<base::FilePath>& files,
    base::OnceCallback<void(std::vector<std::string>)> callback);

class ChromeDataExchangeDelegate : public exo::DataExchangeDelegate {
 public:
  ChromeDataExchangeDelegate();
  ChromeDataExchangeDelegate(const ChromeDataExchangeDelegate&) = delete;
  ChromeDataExchangeDelegate& operator=(const ChromeDataExchangeDelegate&) =
      delete;
  ~ChromeDataExchangeDelegate() override;

  // DataExchangeDelegate:
  ui::EndpointType GetDataTransferEndpointType(
      aura::Window* window) const override;
  std::vector<ui::FileInfo> GetFilenames(
      ui::EndpointType source,
      const std::vector<uint8_t>& data) const override;
  std::string GetMimeTypeForUriList(ui::EndpointType target) const override;
  void SendFileInfo(ui::EndpointType target,
                    const std::vector<ui::FileInfo>& files,
                    SendDataCallback callback) const override;
  bool HasUrlsInPickle(const base::Pickle& pickle) const override;
  void SendPickle(ui::EndpointType target,
                  const base::Pickle& pickle,
                  SendDataCallback callback) override;
  std::vector<ui::FileInfo> ParseFileSystemSources(
      const ui::DataTransferEndpoint* source,
      const base::Pickle& pickle) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_EXO_CHROME_DATA_EXCHANGE_DELEGATE_H_
