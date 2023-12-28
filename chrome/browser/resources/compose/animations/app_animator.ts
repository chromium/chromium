// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Animator, STANDARD_EASING} from './animator.js';

export class ComposeAppAnimator extends Animator {
  transitionToConsent(): Animation[] {
    const consentScreenText = '#consentHeading h1, #consentContainer';
    const consentScreenButtons = '#closeButtonConsent, #consentFooter';
    return [
      this.scaleIn('#consentIconContainer', {duration: 250}),
      this.slideIn(consentScreenText, -8, {duration: 250}),
      this.fadeIn(consentScreenText, {delay: 50, duration: 100}),
      this.fadeIn(consentScreenButtons, {delay: 100, duration: 100}),
    ].flat();
  }

  transitionToInput(): Animation[] {
    // Need to queue the fade out animation first to prevent the consent
    // dialog from becoming immediately hidden. Otherwise, height calculations
    // below will have values of 0 since the dialog is hidden.
    const consentFadeOut =
        this.fadeOutAndHide('#consentDialog', 'flex', {duration: 100});

    const contentMoveDistance = 48;
    const consentContainerHeight =
        this.getElement('#consentContainer').offsetHeight;
    const consentContainerHeightAnimation = this.animate(
        '#consentContainer',
        [
          {height: `${consentContainerHeight}px`},
          {height: `${consentContainerHeight - contentMoveDistance}px`},
        ],
        {duration: 200, easing: STANDARD_EASING});

    const inputScreenText = '#heading h1, #body, #submitFooter .footer-text';
    return [
      consentFadeOut,
      consentContainerHeightAnimation,
      this.slideOut(
          '#consentHeading h1, #consentContainer', -8, {duration: 200}),
      this.fadeOut('#consentFooter', {duration: 100}),
      this.slideIn(inputScreenText, 8, {duration: 200}),
      this.slideIn('#submitButton', contentMoveDistance, {duration: 200}),
      this.fadeIn(inputScreenText, {delay: 100, duration: 100}),
      this.fadeIn('#submitButton', {delay: 100, duration: 100}),
    ].flat();
  }
}
