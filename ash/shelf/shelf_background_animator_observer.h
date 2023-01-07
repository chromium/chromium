// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_BACKGROUND_ANIMATOR_OBSERVER_H_
#define ASH_SHELF_SHELF_BACKGROUND_ANIMATOR_OBSERVER_H_

#include "ash/ash_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

// Observer for the ShelfBackgroundAnimator class.
class ASH_EXPORT ShelfBackgroundAnimatorObserver {
 public:
  // Called when the Shelf's background should be updated.
  virtual void UpdateShelfBackground(SkColor color) {}

  // Called when the ShelfBackgroundAnimator's animation is ended.
  virtual void OnShelfBackgroundAnimationEnded() {}

 protected:
  virtual ~ShelfBackgroundAnimatorObserver() {}
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_BACKGROUND_ANIMATOR_OBSERVER_H_
