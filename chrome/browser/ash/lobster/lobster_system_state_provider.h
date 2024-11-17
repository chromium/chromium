// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SYSTEM_STATE_PROVIDER_H_
#define CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SYSTEM_STATE_PROVIDER_H_

#include "base/memory/raw_ptr.h"

class Profile;

namespace ash {
struct LobsterSystemState;
}  // namespace ash

// TODO(b/348280621): Complete enable/disable logic.
class LobsterSystemStateProvider {
 public:
  explicit LobsterSystemStateProvider(Profile* profile);
  ~LobsterSystemStateProvider();

  ash::LobsterSystemState GetSystemState();

 private:
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SYSTEM_STATE_PROVIDER_H_
