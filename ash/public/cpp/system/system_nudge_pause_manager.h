// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_SYSTEM_NUDGE_PAUSE_MANAGER_H_
#define ASH_PUBLIC_CPP_SYSTEM_SYSTEM_NUDGE_PAUSE_MANAGER_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

class ScopedNudgePause;
class SystemNudgeController;

// Public interface to manage `SystemNudgeController`. This is needed for
// `ScopedNudgePause` since we need a singleton class to manage all the
// `SystemNudgeController`s to respond to `ScopedNudgePause`'s construction or
// destruction.
class ASH_PUBLIC_EXPORT SystemNudgePauseManager {
 public:
  // Returns the singleton `SystemNudgePauseManager`.
  static SystemNudgePauseManager* Get();

  // Creates a `ScopedNudgePause`.
  virtual std::unique_ptr<ScopedNudgePause> CreateScopedPause() = 0;

 protected:
  SystemNudgePauseManager();
  virtual ~SystemNudgePauseManager();

 private:
  friend class ScopedNudgePause;

  // `Pause()` will stop all the nudges from showing up, until `Resume()` is
  // called.
  virtual void Pause() = 0;
  virtual void Resume() = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_SYSTEM_NUDGE_PAUSE_MANAGER_H_
