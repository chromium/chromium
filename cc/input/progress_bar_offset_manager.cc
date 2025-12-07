// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/progress_bar_offset_manager.h"

namespace cc {

ProgressBarOffsetManager::~ProgressBarOffsetManager() = default;

void ProgressBarOffsetManager::OnLoadProgressChanged(float progress) {
  load_progress_ = progress;
}

}  // namespace cc
