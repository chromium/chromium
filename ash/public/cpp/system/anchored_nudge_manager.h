// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_ANCHORED_NUDGE_MANAGER_H_
#define ASH_PUBLIC_CPP_SYSTEM_ANCHORED_NUDGE_MANAGER_H_

#include <memory>
#include <string>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

struct AnchoredNudgeData;

// Public interface to show anchored nudges.
class ASH_PUBLIC_EXPORT AnchoredNudgeManager {
 public:
  // Returns the singleton `AnchoredNudgeManager`.
  static AnchoredNudgeManager* Get();

  // Shows an anchored nudge, and sets its contents with the provided `data`.
  // It will persist until it is dismissed with `Cancel()`, it times out, or its
  // anchor view is deleted/hidden. It will not be created if the anchor view is
  // invisible or does not have a widget.
  virtual void Show(const AnchoredNudgeData& nudge_data) = 0;

  // Cancels an anchored nudge with the provided `id`.
  virtual void Cancel(const std::string& id) = 0;

 protected:
  AnchoredNudgeManager();
  virtual ~AnchoredNudgeManager();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_ANCHORED_NUDGE_MANAGER_H_
