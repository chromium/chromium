// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FIRST_RUN_FIRST_RUN_HELPER_IMPL_H_
#define ASH_FIRST_RUN_FIRST_RUN_HELPER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/first_run_helper.h"
#include "ash/session/session_observer.h"
#include "base/callback.h"
#include "base/macros.h"

namespace ash {

class DesktopCleaner;

// Interface used by first-run tutorial to manipulate and retrieve information
// about shell elements.
class ASH_EXPORT FirstRunHelperImpl : public FirstRunHelper,
                                      public SessionObserver {
 public:
  explicit FirstRunHelperImpl(base::OnceClosure on_cancelled);
  ~FirstRunHelperImpl() override;

  // FirstRunHelper:
  gfx::Rect GetAppListButtonBounds() override;
  gfx::Rect OpenTrayBubble() override;
  void CloseTrayBubble() override;

  // SessionObserver:
  void OnLockStateChanged(bool locked) override;
  void OnChromeTerminating() override;

  void FlushForTesting();

 private:
  // Notifies the client to cancel the tutorial.
  void Cancel();

  base::OnceClosure on_cancelled_;

  std::unique_ptr<DesktopCleaner> cleaner_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunHelperImpl);
};

}  // namespace ash

#endif  // ASH_FIRST_RUN_FIRST_RUN_HELPER_IMPL_H_
