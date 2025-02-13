// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SYSTEM_STATE_PROVIDER_H_
#define CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SYSTEM_STATE_PROVIDER_H_

#include "ash/public/cpp/lobster/lobster_text_input_context.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/ime/text_input_type.h"

class Profile;

namespace ash {
struct LobsterSystemState;
}  // namespace ash

// TODO(b/348280621): Complete enable/disable logic.
class LobsterSystemStateProvider {
 public:
  explicit LobsterSystemStateProvider(Profile* profile);
  ~LobsterSystemStateProvider();

  ash::LobsterSystemState GetSystemState(
      const ash::LobsterTextInputContext& text_input_context);

 private:
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SYSTEM_STATE_PROVIDER_H_
