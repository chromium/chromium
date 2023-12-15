// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

const STANDARD_EASING = 'cubic-bezier(0.2, 0.0, 0, 1.0)';

/**
 * Generic animator class that has common animations and util methods.
 */
export class Animator {
  private root_: HTMLElement;
  private animationsEnabled_: boolean;

  constructor(root: HTMLElement, animationsEnabled: boolean) {
    this.root_ = root;
    this.animationsEnabled_ = animationsEnabled;
  }

  animate(
      selector: string, keyframes: Keyframe[],
      options: KeyframeAnimationOptions): Animation[] {
    if (!this.animationsEnabled_) {
      return [];
    }
    const elements = Array.from(
        this.root_.shadowRoot!.querySelectorAll<HTMLElement>(selector));
    assert(elements.length > 0);
    return elements.map(element => {
      return element.animate(keyframes, Object.assign({fill: 'both'}, options));
    });
  }

  fadeIn(selector: string, options: KeyframeAnimationOptions): Animation[] {
    return this.animate(
        selector,
        [
          {opacity: 0},
          {opacity: 1},
        ],
        Object.assign({easing: 'linear', fill: 'both'}, options));
  }

  scaleIn(selector: string, options: KeyframeAnimationOptions): Animation[] {
    return this.animate(
        selector,
        [
          {transform: 'scale(0)'},
          {transform: 'scale(1)'},
        ],
        Object.assign({easing: STANDARD_EASING, fill: 'both'}, options));
  }

  slideIn(
      selector: string, startDistance: number,
      options: KeyframeAnimationOptions): Animation[] {
    return this.animate(
        selector,
        [
          {transform: `translateY(${startDistance}px)`},
          {transform: `translateY(0)`},
        ],
        Object.assign({easing: STANDARD_EASING, fill: 'both'}, options));
  }
}
