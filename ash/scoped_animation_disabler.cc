// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scoped_animation_disabler.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/base/class_property.h"

namespace ash {

ScopedAnimationDisabler::ScopedAnimationDisabler(aura::Window* window)
    : window_(window) {
  needs_disable_ = !window_->GetProperty(aura::client::kAnimationsDisabledKey);
  if (needs_disable_)
    window_->SetProperty(aura::client::kAnimationsDisabledKey, true);
}

ScopedAnimationDisabler::~ScopedAnimationDisabler() {
  if (needs_disable_)
    window_->ClearProperty(aura::client::kAnimationsDisabledKey);
}

}  // namespace ash