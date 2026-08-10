// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Class to track whether the user desires animations to be shown. This is
// exposed via `AnimationTracker.showAnimations`. For testing, `showAnimations`
// can be directly set, then `resetForTesting()` can be used to reset it during
// test teardown.
export class AnimationTracker {
  private static reducedMotion_: MediaQueryList =
      window.matchMedia('(prefers-reduced-motion: reduce)');

  static showAnimations: boolean = !AnimationTracker.reducedMotion_.matches;

  static {
    AnimationTracker.reducedMotion_.addEventListener('change', () => {
      AnimationTracker.showAnimations =
          !AnimationTracker.reducedMotion_.matches;
    });
  }

  // If tests directly modify `showAnimations`, the tests should call this
  // method during teardown to reset it.
  static resetForTesting() {
    AnimationTracker.showAnimations = !AnimationTracker.reducedMotion_.matches;
  }
}
