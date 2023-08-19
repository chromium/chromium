// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/system_nudge_pause_manager_impl.h"

#include <memory>

#include "ash/public/cpp/system/scoped_nudge_pause.h"
#include "base/check_op.h"
#include "base/observer_list.h"

namespace ash {

SystemNudgePauseManagerImpl::SystemNudgePauseManagerImpl() = default;

SystemNudgePauseManagerImpl::~SystemNudgePauseManagerImpl() = default;

std::unique_ptr<ScopedNudgePause>
SystemNudgePauseManagerImpl::CreateScopedPause() {
  return std::make_unique<ScopedNudgePause>();
}

void SystemNudgePauseManagerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SystemNudgePauseManagerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SystemNudgePauseManagerImpl::Pause() {
  ++pause_counter_;

  // Immediately closes all the nudges.
  for (auto& observer : observers_) {
    observer.OnSystemNudgePaused();
  }
}

void SystemNudgePauseManagerImpl::Resume() {
  CHECK_GT(pause_counter_, 0);
  --pause_counter_;
}

}  // namespace ash
