// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/start_animation_observer.h"

namespace ash {

StartAnimationObserver::StartAnimationObserver() = default;

StartAnimationObserver::~StartAnimationObserver() = default;

void StartAnimationObserver::OnImplicitAnimationsCompleted() {
  if (owner_)
    owner_->RemoveAndDestroyStartAnimationObserver(this);
}

void StartAnimationObserver::SetOwner(WindowSelectorDelegate* owner) {
  DCHECK(!owner_);
  owner_ = owner;
}

void StartAnimationObserver::Shutdown() {
  owner_ = nullptr;
}

}  // namespace ash