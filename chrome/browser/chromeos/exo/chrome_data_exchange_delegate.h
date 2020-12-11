// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXO_CHROME_DATA_EXCHANGE_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_EXO_CHROME_DATA_EXCHANGE_DELEGATE_H_

#include "components/exo/data_exchange_delegate.h"

namespace chromeos {

class ChromeDataExchangeDelegate : public exo::DataExchangeDelegate {
 public:
  ChromeDataExchangeDelegate();
  ChromeDataExchangeDelegate(const ChromeDataExchangeDelegate&) = delete;
  ChromeDataExchangeDelegate& operator=(const ChromeDataExchangeDelegate&) =
      delete;
  ~ChromeDataExchangeDelegate() override;

  // DataExchangeDelegate:
  ui::EndpointType GetDataTransferEndpointType(
      aura::Window* target) const override;
  void SetSourceOnOSExchangeData(
      aura::Window* target,
      ui::OSExchangeData* os_exchange_data) const override;
  std::vector<ui::FileInfo> GetFilenames(
      aura::Window* source,
      const std::vector<uint8_t>& data) const override;
  std::string GetMimeTypeForUriList(aura::Window* target) const override;
  void SendFileInfo(aura::Window* target,
                    const std::vector<ui::FileInfo>& files,
                    SendDataCallback callback) const override;
  bool HasUrlsInPickle(const base::Pickle& pickle) const override;
  void SendPickle(aura::Window* target,
                  const base::Pickle& pickle,
                  SendDataCallback callback) override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXO_CHROME_DATA_EXCHANGE_DELEGATE_H_
