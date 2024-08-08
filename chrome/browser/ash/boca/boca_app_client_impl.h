// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_BOCA_APP_CLIENT_IMPL_H_
#define CHROME_BROWSER_ASH_BOCA_BOCA_APP_CLIENT_IMPL_H_

#include "base/observer_list.h"
#include "chromeos/ash/components/boca/boca_app_client.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace ash {
class BocaAppClientImpl : public ash::BocaAppClient {
 public:
  BocaAppClientImpl();
  BocaAppClientImpl(const BocaAppClientImpl&) = delete;
  BocaAppClientImpl& operator=(const BocaAppClientImpl&) = delete;
  ~BocaAppClientImpl() override;

  // ash::BocaAppClient
  signin::IdentityManager* GetIdentityManager() override;

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;

 private:
  base::ObserverList<Observer> observers_;
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BOCA_BOCA_APP_CLIENT_IMPL_H_
