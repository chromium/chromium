// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Animator, STANDARD_EASING} from './animator.js';

export class ComposeTextareaAnimator extends Animator {
  transitionToEditable(): Animation[] {
    return this.fadeIn('#editButtonContainer', {duration: 100});
  }

  transitionToEditing(bodyHeight: number): Animation[] {
    const dimensionsAnimation = this.animate(
        '#inputContainer textarea, #readonlyContainer',
        [
          {
            height: 'var(--compose-textarea-readonly-height)',
            padding: 'var(--compose-textarea-readonly-padding)',
          },
          {
            height: `${bodyHeight}px`,
            padding: 'var(--compose-textarea-input-padding)',
          },
        ],
        {duration: 200, easing: STANDARD_EASING});

    const colorAnimation = this.animate(
        '#inputContainer textarea, #readonlyContainer',
        [
          {
            background: 'var(--compose-textarea-readonly-background)',
            outlineColor: 'transparent',
          },
          {
            background: 'transparent',
          },
        ],
        {delay: 100, duration: 100, easing: 'linear'});

    return [
      dimensionsAnimation,
      colorAnimation,

      // Fade out edit button and keep it faded out for rest of animations.
      this.fadeOut('#editButton', {duration: 100}),
      this.maintainStyles(
          '#editButton', {opacity: 0},
          {delay: 100, duration: 100, fill: 'none'}),
    ].flat();
  }

  transitionToResult(bodyHeight: number): Animation[] {
    const dimensionsAnimation = this.animate(
        '#inputContainer textarea, #readonlyContainer',
        [
          {
            height: `${bodyHeight}px`,
            padding: 'var(--compose-textarea-input-padding)',
          },
          {
            height: 'var(--compose-textarea-readonly-height)',
            padding: 'var(--compose-textarea-readonly-padding)',
          },
        ],
        {duration: 200, easing: STANDARD_EASING});

    const colorAnimation = this.animate(
        '#inputContainer textarea, #readonlyContainer',
        [
          {
            background: 'transparent',
          },
          {
            background: 'var(--compose-textarea-readonly-background)',
            outlineColor: 'transparent',
          },
        ],
        {duration: 100, easing: 'linear'});

    return [
      dimensionsAnimation,
      colorAnimation,

      // Hide scrollbar resulting from shrinking textarea.
      this.maintainStyles(
          '#inputContainer textarea', {overflow: 'hidden'}, {duration: 200}),
      // Fade in edit button.
      this.fadeIn('#editButton', {delay: 100, duration: 100}),
    ].flat();
  }

  transitionToReadonly(fromHeight?: number): Animation[] {
    const fromHeightValue =
        fromHeight ? `${fromHeight}px` : 'var(--compose-textarea-input-height)';
    const dimensionsAnimation = this.animate(
        '#inputContainer textarea, #readonlyContainer',
        [
          {
            height: fromHeightValue,
            padding: 'var(--compose-textarea-input-padding)',
          },
          {
            height: 'var(--compose-textarea-readonly-height)',
            padding: 'var(--compose-textarea-readonly-padding)',
          },
        ],
        {duration: 250, easing: STANDARD_EASING});

    const colorAnimation = this.animate(
        '#inputContainer textarea, #readonlyContainer',
        [
          {
            background: 'transparent',
          },
          {
            background: 'var(--compose-textarea-readonly-background)',
            outlineColor: 'transparent',
          },
        ],
        {duration: 100, easing: 'linear'});

    return [
      this.fadeOutAndHide('#inputContainer', 'flex', {duration: 250}),
      this.fadeIn('#readonlyContainer', {duration: 250}),
      dimensionsAnimation,
      colorAnimation,
    ].flat();
  }
}
