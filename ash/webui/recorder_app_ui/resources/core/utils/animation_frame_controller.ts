// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A simple wrapper for requestAnimationFrame for starting / stopping the
 * requestAnimationFrame loop.
 *
 * Note that user should always ensure that `stop()` is called eventually before
 * the object reference is dropped.
 *
 * TODO(pihsun): Add back ReactiveController version if there's usage for it.
 */
export class AnimationFrameController {
  private animationHandler: number|null = null;

  constructor(private readonly callback: () => void) {}

  start(): void {
    if (this.animationHandler === null) {
      this.animationHandler = requestAnimationFrame(() => {
        this.onAnimationFrame();
      });
    }
  }

  stop(): void {
    if (this.animationHandler !== null) {
      cancelAnimationFrame(this.animationHandler);
      this.animationHandler = null;
    }
  }

  private onAnimationFrame() {
    this.callback();
    this.animationHandler = requestAnimationFrame(() => {
      this.onAnimationFrame();
    });
  }
}
