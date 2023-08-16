// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_ANCHORED_NUDGE_MANAGER_H_
#define ASH_PUBLIC_CPP_SYSTEM_ANCHORED_NUDGE_MANAGER_H_

#include <memory>
#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/ash_public_export.h"

namespace ash {

struct AnchoredNudgeData;
class ScopedNudgePause;

// Public interface to show anchored nudges.
class ASH_PUBLIC_EXPORT AnchoredNudgeManager {
 public:
  // Returns the singleton `AnchoredNudgeManager`.
  static AnchoredNudgeManager* Get();

  // Shows an anchored nudge, and sets its contents with the provided
  // `nudge_data`. It will persist until it is dismissed with `Cancel()`, it
  // times out, or its anchor view is deleted/hidden. It will not be created if
  // the anchor view is invisible or does not have a widget.

  // TODO(b/285023559): Add and use a `ChainedCancelCallback` class instead of a
  // `RepeatingClosure` so we don't have to manually modify the provided
  // callbacks in the manager, and we can pass `nudge_data` as a constant.
  virtual void Show(AnchoredNudgeData& nudge_data) = 0;

  // Cancels an anchored nudge with the provided `id`.
  virtual void Cancel(const std::string& id) = 0;

  // Records Nudge "TimeToAction" metric, which tracks the time from when a
  // nudge was shown to when the nudge's suggested action was performed.
  // No op if the nudge specified by `catalog_name` hasn't been shown before.
  virtual void MaybeRecordNudgeAction(NudgeCatalogName catalog_name) = 0;

  // Returns true if the nudge with `id` is shown at the moment.
  virtual bool IsNudgeShown(const std::string& id) = 0;

  // Creates a `ScopedNudgePause`, which closes all `AnchoredNudge`'s and
  // `SystemNudge`'s, and prevents more from being shown while any
  // `ScopedNudgePause` is in scope.
  virtual std::unique_ptr<ScopedNudgePause> CreateScopedPause() = 0;

 protected:
  AnchoredNudgeManager();
  virtual ~AnchoredNudgeManager();

 private:
  friend class ScopedNudgePause;

  // `Pause()` will stop all the nudges from showing up, until `Resume()` is
  // called.
  virtual void Pause() = 0;
  virtual void Resume() = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_ANCHORED_NUDGE_MANAGER_H_
