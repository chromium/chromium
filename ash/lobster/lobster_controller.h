// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOBSTER_LOBSTER_CONTROLLER_H_
#define ASH_LOBSTER_LOBSTER_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class LobsterClientFactory;

class ASH_EXPORT LobsterController {
 public:
  LobsterController();
  ~LobsterController();

  static bool IsEnabled();

  void SetClientFactory(LobsterClientFactory* client_factory);

 private:
  // Not owned by this class.
  raw_ptr<LobsterClientFactory> client_factory_;
};

}  // namespace ash

#endif  // ASH_LOBSTER_LOBSTER_CONTROLLER_H_
