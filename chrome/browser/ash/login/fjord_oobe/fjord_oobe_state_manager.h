// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_FJORD_OOBE_FJORD_OOBE_STATE_MANAGER_H_
#define CHROME_BROWSER_ASH_LOGIN_FJORD_OOBE_FJORD_OOBE_STATE_MANAGER_H_

#include "chrome/browser/ash/login/fjord_oobe/proto/fjord_oobe_state.pb.h"

namespace ash {

// Singleton service that keeps track of the OOBE state for the Fjord OOBE flow.
// Should only be created for users of the Fjord OOBE flow.
class FjordOobeStateManager {
 public:
  // Manage singleton instance.
  static void Initialize();
  static void Shutdown();
  static FjordOobeStateManager* Get();

  FjordOobeStateManager(const FjordOobeStateManager&) = delete;
  FjordOobeStateManager& operator=(const FjordOobeStateManager&) = delete;

  // Returns the FjordOobeStateInfo with the current state of OOBE.
  fjord_oobe_state::proto::FjordOobeStateInfo GetFjordOobeStateInfo();
  // Sets the current OOBE state.
  void OnFjordOobeStateChanged(
      fjord_oobe_state::proto::FjordOobeStateInfo::FjordOobeState new_state);

 private:
  FjordOobeStateManager();
  ~FjordOobeStateManager() = default;

  fjord_oobe_state::proto::FjordOobeStateInfo::FjordOobeState current_state_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_FJORD_OOBE_FJORD_OOBE_STATE_MANAGER_H_
