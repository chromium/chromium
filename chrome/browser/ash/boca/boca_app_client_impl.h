// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_BOCA_APP_CLIENT_IMPL_H_
#define CHROME_BROWSER_ASH_BOCA_BOCA_APP_CLIENT_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "chromeos/ash/components/boca/boca_app_client.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace signin {
class IdentityManager;
}

namespace ash::boca {
class BocaAppClientImpl : public BocaAppClient {
 public:
  BocaAppClientImpl();
  BocaAppClientImpl(const BocaAppClientImpl&) = delete;
  BocaAppClientImpl& operator=(const BocaAppClientImpl&) = delete;
  ~BocaAppClientImpl() override;

  // ash::BocaAppClient
  signin::IdentityManager* GetIdentityManager() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  std::string GetDeviceId() override;
};
}  // namespace ash::boca

#endif  // CHROME_BROWSER_ASH_BOCA_BOCA_APP_CLIENT_IMPL_H_
