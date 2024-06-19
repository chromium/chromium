// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ReactiveController} from 'chrome://resources/mwc/lit/index.js';

export class AnimationFrameController implements ReactiveController {
  private animationHandler: number|null = null;

  constructor(private readonly callback: () => void) {}

  hostConnected(): void {
    this.animationHandler = requestAnimationFrame(() => {
      this.onAnimationFrame();
    });
  }

  private onAnimationFrame() {
    this.callback();
    this.animationHandler = requestAnimationFrame(() => {
      this.onAnimationFrame();
    });
  }

  hostDisconnected(): void {
    if (this.animationHandler !== null) {
      cancelAnimationFrame(this.animationHandler);
      this.animationHandler = null;
    }
  }
}
