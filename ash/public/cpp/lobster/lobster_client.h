// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOBSTER_LOBSTER_CLIENT_H_
#define ASH_PUBLIC_CPP_LOBSTER_LOBSTER_CLIENT_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

class ASH_PUBLIC_EXPORT LobsterClient {
 public:
  virtual ~LobsterClient() = default;

  virtual bool IsFeatureAllowed() = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOBSTER_LOBSTER_CLIENT_H_
