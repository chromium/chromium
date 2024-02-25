// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

export const STANDARD_EASING = 'cubic-bezier(0.2, 0.0, 0, 1.0)';

export const EMPHASIZED_DECELERATE = 'cubic-bezier(0.05, 0.7, 0.1, 1.0)';

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

  getElement(selector: string): HTMLElement {
    const element = this.root_.shadowRoot!.querySelector(selector);
    assert(element);
    return element as HTMLElement;
  }

  animate(
      selector: string, keyframes: Keyframe[],
      options: KeyframeAnimationOptions,
      meetsCondition: boolean = true): Animation[] {
    if (!this.animationsEnabled_ || !meetsCondition) {
      return [];
    }

    const elements = Array.from(
        this.root_.shadowRoot!.querySelectorAll<HTMLElement>(selector));
    assert(elements.length > 0);
    return elements.map(element => {
      return element.animate(
          keyframes, Object.assign({fill: 'backwards'}, options));
    });
  }

  fadeIn(selector: string, options: KeyframeAnimationOptions): Animation[] {
    return this.animate(
        selector,
        [
          {opacity: 0},
          {opacity: 1},
        ],
        Object.assign({easing: 'linear'}, options));
  }

  fadeOut(selector: string, options: KeyframeAnimationOptions): Animation[] {
    return this.animate(
        selector,
        [
          {opacity: 1},
          {opacity: 0},
        ],
        Object.assign({easing: 'linear'}, options));
  }

  /* Fades out an element and then sets the 'display' of the element to 'none'.
   * This is useful for animations that result in an element and its children
   * becoming [hidden]. */
  fadeOutAndHide(
      selector: string, beforeDisplay: string,
      options: KeyframeAnimationOptions): Animation[] {
    return this.animate(
        selector,
        [
          {display: beforeDisplay, opacity: 1},
          {display: 'none', opacity: 0},
        ],
        Object.assign({easing: 'linear'}, options));
  }

  /* Maintains a style for a duration on an element. This is useful for
   * animations that require a fixed state (such as fixed heights). */
  maintainStyles(
      selector: string, styles: Keyframe, options: KeyframeAnimationOptions) {
    return this.animate(selector, [styles, styles], options);
  }

  scaleIn(selector: string, options: KeyframeAnimationOptions): Animation[] {
    return this.animate(
        selector,
        [
          {transform: 'scale(0)'},
          {transform: 'scale(1)'},
        ],
        Object.assign({easing: STANDARD_EASING}, options));
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
        Object.assign({easing: STANDARD_EASING}, options));
  }

  slideOut(
      selector: string, endDistance: number,
      options: KeyframeAnimationOptions): Animation[] {
    return this.animate(
        selector,
        [
          {transform: `translateY(0)`},
          {transform: `translateY(${endDistance}px)`},
        ],
        Object.assign({easing: STANDARD_EASING}, options));
  }
}
