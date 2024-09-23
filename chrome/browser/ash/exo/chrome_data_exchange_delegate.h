// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXO_CHROME_DATA_EXCHANGE_DELEGATE_H_
#define CHROME_BROWSER_ASH_EXO_CHROME_DATA_EXCHANGE_DELEGATE_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "components/exo/data_exchange_delegate.h"
#include "storage/browser/file_system/file_system_url.h"

namespace ash {

// Get all FileSystemURLs in `pickle`.
std::vector<storage::FileSystemURL> GetFileSystemUrlsFromPickle(
    const base::Pickle& pickle);

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
  std::string GetMimeTypeForUriList(ui::EndpointType target) const override;
  bool HasUrlsInPickle(const base::Pickle& pickle) const override;
  std::vector<ui::FileInfo> ParseFileSystemSources(
      const ui::DataTransferEndpoint* source,
      const base::Pickle& pickle) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_EXO_CHROME_DATA_EXCHANGE_DELEGATE_H_
