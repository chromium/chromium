// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Animator} from './animator.js';

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
    const inputScreenText = '#heading h1, #body, #submitFooter .footer-text';
    const inputScreenButton = '#submitButton';
    return [
      this.slideIn(inputScreenText, 8, {duration: 200}),
      this.slideIn(inputScreenButton, 48, {duration: 200}),
      this.fadeIn(inputScreenText, {delay: 100, duration: 100}),
      this.fadeIn(inputScreenButton, {delay: 100, duration: 100}),
    ].flat();
  }
}
