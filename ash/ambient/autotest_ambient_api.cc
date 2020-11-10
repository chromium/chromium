// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/autotest_ambient_api.h"

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/shell.h"
#include "base/callback.h"

namespace ash {

namespace {

class PhotoTransitionAnimationObserver : public AmbientViewDelegateObserver {
 public:
  PhotoTransitionAnimationObserver(int num_completions,
                                   base::OnceClosure on_complete)
      : num_completions_(num_completions),
        on_complete_(std::move(on_complete)) {
    DCHECK_GT(num_completions, 0);
    Shell::Get()->ambient_controller()->AddAmbientViewDelegateObserver(this);
  }

  PhotoTransitionAnimationObserver(const PhotoTransitionAnimationObserver&) =
      delete;

  PhotoTransitionAnimationObserver& operator=(
      const PhotoTransitionAnimationObserver&) = delete;

  ~PhotoTransitionAnimationObserver() override {
    Shell::Get()->ambient_controller()->RemoveAmbientViewDelegateObserver(this);
  }

  // AmbientViewDelegateObserver:
  void OnPhotoTransitionAnimationCompleted() override {
    --num_completions_;
    if (num_completions_ == 0) {
      std::move(on_complete_).Run();
      delete this;
    }
  }

 private:
  int num_completions_;
  base::OnceClosure on_complete_;
};

}  // namespace

AutotestAmbientApi::AutotestAmbientApi() = default;

AutotestAmbientApi::~AutotestAmbientApi() = default;

void AutotestAmbientApi::WaitForPhotoTransitionAnimationCompleted(
    int num_completions,
    base::OnceClosure on_complete) {
  new PhotoTransitionAnimationObserver(num_completions, std::move(on_complete));
}

}  // namespace ash
