// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/scoped_decoration_hider.h"

#include <utility>

#include "ash/wm/window_properties.h"
#include "base/check.h"
#include "base/check_op.h"
#include "ui/aura/window.h"

namespace ash {

ScopedDecorationHider::ScopedDecorationHider(aura::Window* window) {
  CHECK(window);
  window_ = window->GetWeakPtrAsWindow();
  UpdateCount(1);
}

ScopedDecorationHider::ScopedDecorationHider(
    ScopedDecorationHider&& other) noexcept
    : window_(std::move(other.window_)) {}

ScopedDecorationHider& ScopedDecorationHider::operator=(
    ScopedDecorationHider&& other) noexcept {
  if (this != &other) {
    Reset();
    window_ = std::move(other.window_);
  }
  return *this;
}

ScopedDecorationHider::~ScopedDecorationHider() {
  Reset();
}

void ScopedDecorationHider::Reset() {
  if (window_ && !window_->is_destroying()) {
    UpdateCount(-1);
  }
  window_ = nullptr;
}

void ScopedDecorationHider::UpdateCount(int delta) {
  const int new_count = window_->GetProperty(kDecorationHiderCountKey) + delta;
  CHECK_GE(new_count, 0);
  window_->SetProperty(kDecorationHiderCountKey, new_count);
}

}  // namespace ash
