// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/test_support/fake_render_tuning_delegate.h"

namespace performance_manager {

FakeRenderTuningDelegate::FakeRenderTuningDelegate(bool* render_tuning_enabled)
    : render_tuning_enabled_(render_tuning_enabled) {}

FakeRenderTuningDelegate::~FakeRenderTuningDelegate() = default;

void FakeRenderTuningDelegate::EnableRenderBatterySaverMode() {
  *render_tuning_enabled_ = true;
}

void FakeRenderTuningDelegate::DisableRenderBatterySaverMode() {
  *render_tuning_enabled_ = false;
}

}  // namespace performance_manager
