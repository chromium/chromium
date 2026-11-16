// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/scoped_decoration_hider.h"

#include "ash/wm/window_properties.h"
#include "base/check.h"
#include "base/check_op.h"
#include "ui/aura/window.h"

namespace ash {

ScopedDecorationHider::ScopedDecorationHider(aura::Window* window)
    : window_(window) {
  CHECK(window_);
  window_->AddObserver(this);
  UpdateCount(1);
}

ScopedDecorationHider::~ScopedDecorationHider() {
  if (!window_) {
    return;
  }
  window_->RemoveObserver(this);
  UpdateCount(-1);
}

void ScopedDecorationHider::OnWindowDestroying(aura::Window* window) {
  window_->RemoveObserver(this);
  window_ = nullptr;
}

void ScopedDecorationHider::UpdateCount(int delta) {
  const int new_count = window_->GetProperty(kDecorationHiderCountKey) + delta;
  CHECK_GE(new_count, 0);
  window_->SetProperty(kDecorationHiderCountKey, new_count);
}

}  // namespace ash
