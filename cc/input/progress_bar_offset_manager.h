// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_PROGRESS_BAR_OFFSET_MANAGER_H_
#define CC_INPUT_PROGRESS_BAR_OFFSET_MANAGER_H_

#include "cc/cc_export.h"

namespace cc {

// Manages the position of the composited progress bar.
class CC_EXPORT ProgressBarOffsetManager {
 public:
  virtual ~ProgressBarOffsetManager();

  ProgressBarOffsetManager& operator=(const ProgressBarOffsetManager&) = delete;

  void OnLoadProgressChanged(float progress);

 private:
  // A value between 0 and 1, where 1 means the page is completely loaded.
  float load_progress_ = 0.f;

  // TODO(https://crbug.com/434769819) Implement animation logic.
};

}  // namespace cc

#endif  // CC_INPUT_PROGRESS_BAR_OFFSET_MANAGER_H_
