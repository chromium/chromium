// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Animator, STANDARD_EASING} from './animator.js';

export class ComposeTextareaAnimator extends Animator {
  transitionToEditable(): Animation[] {
    return this.fadeIn('#editButtonContainer', {duration: 100});
  }

  transitionToReadonly(): Animation[] {
    const dimensionsAnimation = this.animate(
        '#inputContainer textarea, #readonlyContainer',
        [
          {
            height: 'var(--compose-textarea-input-height)',
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
