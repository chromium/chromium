// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FWUPD_FWUPD_DOWNLOAD_CLIENT_IMPL_H_
#define CHROME_BROWSER_ASH_FWUPD_FWUPD_DOWNLOAD_CLIENT_IMPL_H_

#include "ash/public/cpp/fwupd_download_client.h"
#include "base/memory/scoped_refptr.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

// Class to provide Profile based information to the ash client.
class FwupdDownloadClientImpl : public ash::FwupdDownloadClient {
 public:
  FwupdDownloadClientImpl();
  FwupdDownloadClientImpl(const FwupdDownloadClientImpl&) = delete;
  FwupdDownloadClientImpl& operator=(const FwupdDownloadClientImpl&) = delete;
  ~FwupdDownloadClientImpl() override;

  // ash::FwupdDownloadClient:
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FWUPD_FWUPD_DOWNLOAD_CLIENT_IMPL_H_
