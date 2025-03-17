// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SYSTEM_STATE_PROVIDER_H_
#define CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SYSTEM_STATE_PROVIDER_H_

#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "ash/public/cpp/lobster/lobster_text_input_context.h"

class LobsterSystemStateProvider {
 public:
  virtual ~LobsterSystemStateProvider() = default;

  virtual ash::LobsterSystemState GetSystemState(
      const ash::LobsterTextInputContext& text_input_context) = 0;
};

#endif  // CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SYSTEM_STATE_PROVIDER_H_
