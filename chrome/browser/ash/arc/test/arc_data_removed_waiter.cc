// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/test/arc_data_removed_waiter.h"

#include "base/run_loop.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"

namespace arc {

ArcDataRemovedWaiter::ArcDataRemovedWaiter() {
  DCHECK(ArcSessionManager::Get());
  ArcSessionManager::Get()->AddObserver(this);
}

ArcDataRemovedWaiter::~ArcDataRemovedWaiter() {
  ArcSessionManager::Get()->RemoveObserver(this);
}

void ArcDataRemovedWaiter::Wait() {
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();
}

void ArcDataRemovedWaiter::OnArcDataRemoved() {
  if (!run_loop_)
    return;
  run_loop_->Quit();
}

}  // namespace arc
