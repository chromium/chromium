// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Animator, EMPHASIZED_DECELERATE, STANDARD_EASING} from './animator.js';

export class ComposeAppAnimator extends Animator {
  transitionOutSubmitFooter(bodyHeight: number, footerHeight: number):
      Animation[] {
    return [
      /* Freeze dialog heights while footer fade out finishes. */
      this.maintainStyles(
          '#bodyAndFooter', {
            gridTemplateAreas: '"body" "footer"',
            gridTemplateRows: `${bodyHeight}px ${footerHeight}px`,
          },
          {duration: 50}),
      this.fadeOutAndHide('#submitFooter', 'flex', {duration: 50}),
    ].flat();
  }

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

  transitionInLoading(): Animation[] {
    return this.fadeIn('#loading', {delay: 100, duration: 100});
  }

  transitionFromLoadingToCompleteResult(loadingHeight: number): Animation[] {
    const resultsHeight = this.getElement('#resultContainer').offsetHeight;
    return [
      this.fadeOutAndHide('#loading', 'block', {duration: 100}),
      this.fadeIn('#resultContainer', {duration: 100}),

      /* Transition loading height to the full result height, and hide
       * scrollbars while this happens. */
      this.maintainStyles('#body', {overflow: 'hidden'}, {duration: 400}),
      this.animate(
          '#resultContainer',
          [
            {height: `${loadingHeight}px`},
            {height: `${resultsHeight}px`},
          ],
          {duration: 400, easing: STANDARD_EASING}),

      this.slideIn('#resultOptions', -32, {duration: 400}),
      this.fadeIn('#resultOptions', {delay: 200, duration: 200}),

      this.slideIn(
          '#resultContainer .result-text', -16,
          {delay: 100, duration: 400, easing: EMPHASIZED_DECELERATE}),
      this.fadeIn('#resultContainer .result-text', {delay: 100, duration: 300}),

      this.slideIn(
          '#resultFooter', -130,
          {duration: 400, easing: EMPHASIZED_DECELERATE}),
      this.fadeIn('#resultFooter', {delay: 100, duration: 100}),
    ].flat();
  }

  transitionFromPartialToCompleteResult(): Animation[] {
    return this.fadeIn(
        '#resultOptions, #resultFooter', {delay: 100, duration: 100});
  }
}
