// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_SYSTEM_NUDGE_PAUSE_MANAGER_IMPL_H_
#define ASH_SYSTEM_TOAST_SYSTEM_NUDGE_PAUSE_MANAGER_IMPL_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/system/system_nudge_pause_manager.h"
#include "base/observer_list.h"

namespace ash {

class ScopedNudgePause;

// Class managing `SystemNudgeController` as `Observer`'s. This is needed to
// keep track of `SystemNudgeController`s and respond to `ScopedNudgePause`
// activities.
class ASH_PUBLIC_EXPORT SystemNudgePauseManagerImpl
    : public SystemNudgePauseManager {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when a `SystemNudgePause` is created and starts to pause all the
    // system nudges.
    virtual void OnSystemNudgePaused() = 0;
  };

  SystemNudgePauseManagerImpl();
  SystemNudgePauseManagerImpl(const SystemNudgePauseManagerImpl&) = delete;
  SystemNudgePauseManagerImpl& operator=(const SystemNudgePauseManagerImpl&) =
      delete;
  ~SystemNudgePauseManagerImpl() override;

  int pause_counter() const { return pause_counter_; }

  // SystemNudgePauseManager:
  std::unique_ptr<ScopedNudgePause> CreateScopedPause() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class ScopedNudgePause;

  // SystemNudgePauseManager:
  void Pause() override;
  void Resume() override;

  // Keeps track of the number of `ScopedNudgePause`s.
  int pause_counter_ = 0;

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_SYSTEM_NUDGE_PAUSE_MANAGER_IMPL_H_
