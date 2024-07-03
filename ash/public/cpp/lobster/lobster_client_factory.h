// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOBSTER_LOBSTER_CLIENT_FACTORY_H_
#define ASH_PUBLIC_CPP_LOBSTER_LOBSTER_CLIENT_FACTORY_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/lobster/lobster_client.h"

namespace ash {

class ASH_PUBLIC_EXPORT LobsterClientFactory {
 public:
  virtual ~LobsterClientFactory() = default;

  virtual std::unique_ptr<LobsterClient> CreateClient() = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOBSTER_LOBSTER_CLIENT_FACTORY_H_
