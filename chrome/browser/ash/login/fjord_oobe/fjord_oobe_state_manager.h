// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_FJORD_OOBE_FJORD_OOBE_STATE_MANAGER_H_
#define CHROME_BROWSER_ASH_LOGIN_FJORD_OOBE_FJORD_OOBE_STATE_MANAGER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ash/login/fjord_oobe/proto/fjord_oobe_state.pb.h"

namespace ash {

// Singleton service that keeps track of the OOBE state for the Fjord OOBE flow.
// Should only be created for users of the Fjord OOBE flow.
class FjordOobeStateManager {
 public:
  // Observers of FjordOobeStateManager are notified when the OOBE state has
  // changed.
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override = default;

    virtual void OnFjordOobeStateChanged(
        fjord_oobe_state::proto::FjordOobeStateInfo new_state) = 0;
  };

  // Manage singleton instance.
  static void Initialize();
  static void Shutdown();
  static FjordOobeStateManager* Get();

  FjordOobeStateManager(const FjordOobeStateManager&) = delete;
  FjordOobeStateManager& operator=(const FjordOobeStateManager&) = delete;

  // Returns the FjordOobeStateInfo with the current state of OOBE.
  fjord_oobe_state::proto::FjordOobeStateInfo GetFjordOobeStateInfo();

  // Sets the current OOBE state.
  void SetFjordOobeState(
      fjord_oobe_state::proto::FjordOobeStateInfo::FjordOobeState new_state);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  FjordOobeStateManager();
  ~FjordOobeStateManager();

  fjord_oobe_state::proto::FjordOobeStateInfo::FjordOobeState current_state_;

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_FJORD_OOBE_FJORD_OOBE_STATE_MANAGER_H_
