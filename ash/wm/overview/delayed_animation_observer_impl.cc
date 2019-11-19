// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/delayed_animation_observer_impl.h"

#include "ash/wm/overview/overview_delegate.h"
#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace ash {

ForceDelayObserver::ForceDelayObserver(base::TimeDelta delay) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ForceDelayObserver::Finish,
                     weak_ptr_factory_.GetWeakPtr()),
      delay);
}

ForceDelayObserver::~ForceDelayObserver() = default;

void ForceDelayObserver::SetOwner(OverviewDelegate* owner) {
  owner_ = owner;
}

void ForceDelayObserver::Shutdown() {
  owner_ = nullptr;
}

void ForceDelayObserver::Finish() {
  if (owner_)
    owner_->RemoveAndDestroyEnterAnimationObserver(this);
}

EnterAnimationObserver::EnterAnimationObserver() = default;

EnterAnimationObserver::~EnterAnimationObserver() = default;

void EnterAnimationObserver::OnImplicitAnimationsCompleted() {
  if (owner_)
    owner_->RemoveAndDestroyEnterAnimationObserver(this);
}

void EnterAnimationObserver::SetOwner(OverviewDelegate* owner) {
  DCHECK(!owner_);
  owner_ = owner;
}

void EnterAnimationObserver::Shutdown() {
  owner_ = nullptr;
}

ExitAnimationObserver::ExitAnimationObserver() = default;

ExitAnimationObserver::~ExitAnimationObserver() = default;

void ExitAnimationObserver::OnImplicitAnimationsCompleted() {
  if (owner_)
    owner_->RemoveAndDestroyExitAnimationObserver(this);
}

void ExitAnimationObserver::SetOwner(OverviewDelegate* owner) {
  DCHECK(!owner_);
  owner_ = owner;
}

void ExitAnimationObserver::Shutdown() {
  owner_ = nullptr;
}

}  // namespace ash
