// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Animator, STANDARD_EASING} from './animator.js';

export class ComposeAppAnimator extends Animator {
  transitionToFirstRun(): Animation[] {
    const firstRunScreenText = '#firstRunHeading h1, #firstRunContainer';
    const firstRunScreenButtons = '#firstRunCloseButton, #firstRunFooter';
    return [
      this.scaleIn('#firstRunIconContainer', {duration: 250}),
      this.slideIn(firstRunScreenText, -8, {duration: 250}),
      this.fadeIn(firstRunScreenText, {delay: 50, duration: 100}),
      this.fadeIn(firstRunScreenButtons, {delay: 100, duration: 100}),
    ].flat();
  }

  transitionToInput(): Animation[] {
    // Need to queue the fade out animation first to prevent the FRE
    // dialog from becoming immediately hidden. Otherwise, height calculations
    // below will have values of 0 since the dialog is hidden.
    const firstRunFadeOut =
        this.fadeOutAndHide('#firstRunDialog', 'flex', {duration: 100});

    const contentMoveDistance = 48;
    const firstRunContainerHeight =
        this.getElement('#firstRunContainer').offsetHeight;
    const firstRunContainerHeightAnimation = this.animate(
        '#firstRunContainer',
        [
          {height: `${firstRunContainerHeight}px`},
          {height: `${firstRunContainerHeight - contentMoveDistance}px`},
        ],
        {duration: 200, easing: STANDARD_EASING});

    const inputScreenText = '#heading h1, #body, #submitFooter .footer-text';
    return [
      firstRunFadeOut,
      firstRunContainerHeightAnimation,
      this.slideOut(
          '#firstRunHeading h1, #firstRunContainer', -8, {duration: 200}),
      this.fadeOut('#firstRunFooter', {duration: 100}),
      this.slideIn(inputScreenText, 8, {duration: 200}),
      this.slideIn('#submitButton', contentMoveDistance, {duration: 200}),
      this.fadeIn(inputScreenText, {delay: 100, duration: 100}),
      this.fadeIn('#submitButton', {delay: 100, duration: 100}),
    ].flat();
  }
}
