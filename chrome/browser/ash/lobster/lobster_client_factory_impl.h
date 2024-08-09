// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_LOBSTER_CLIENT_FACTORY_IMPL_H_
#define CHROME_BROWSER_ASH_LOBSTER_LOBSTER_CLIENT_FACTORY_IMPL_H_

#include <memory>

#include "ash/public/cpp/lobster/lobster_client_factory.h"
#include "base/memory/raw_ptr.h"

namespace ash {
class LobsterClient;
class LobsterController;
}  // namespace ash

class LobsterClientFactoryImpl : public ash::LobsterClientFactory {
 public:
  explicit LobsterClientFactoryImpl(ash::LobsterController* controller);

  ~LobsterClientFactoryImpl() override;

  // LobsterClientFactory overrides
  std::unique_ptr<ash::LobsterClient> CreateClient() override;

 private:
  // Not owned by this class
  raw_ptr<ash::LobsterController> controller_;
};

#endif  // CHROME_BROWSER_ASH_LOBSTER_LOBSTER_CLIENT_FACTORY_IMPL_H_
