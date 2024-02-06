// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isMac} from '//resources/js/platform.js';

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

  transitionInDialog() {
    return [
      this.slideIn('.dialog:not([hidden])', -8, {duration: 200}),
      this.fadeIn('.dialog:not([hidden])', {duration: 200}),
    ];
  }

  transitionInLoading(): Animation[] {
    return this.fadeIn('#loading', {delay: 100, duration: 100});
  }

  transitionFromEditingToLoading(bodyHeight: number): Animation[] {
    return [
      // Shrink #bodyAndFooter from the current expanded height of the edit UI
      // to the loading state.
      this.animate(
          '#bodyAndFooter',
          [
            {height: `${bodyHeight}px`},
            {height: `var(--compose-loading-body-and-footer-height)`},
          ],
          {duration: 250, easing: STANDARD_EASING}, !isMac),

      // Fade out the edit form.
      this.fadeOutAndHide('#editContainer', 'flex', {duration: 250}),

      // The footer for the edit form fades out faster.
      this.fadeOut('#editContainer .footer', {duration: 50}),
      this.maintainStyles(
          '#editContainer .footer', {opacity: 0},
          {delay: 50, duration: 200, fill: 'none'}),
    ].flat();
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
          {duration: 400, easing: STANDARD_EASING}, !isMac),

      this.slideIn('#resultOptions', -32, {duration: 400}),
      this.fadeIn('#resultOptions', {delay: 200, duration: 200}),

      this.slideIn(
          '#resultText', -16,
          {delay: 100, duration: 400, easing: EMPHASIZED_DECELERATE}),
      this.fadeIn('#resultText', {delay: 100, duration: 300}),
      this.animate(
          '#resultText',
          [
            {color: 'var(--compose-result-text-color-while-loading)'},
            {color: 'var(--compose-result-text-color)'},
          ],
          {delay: 400, duration: 100}),

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

  transitionFromResultToLoading(bodyHeight: number, resultsHeight: number):
      Animation[] {
    const loadingHeight = this.getElement('#loading').offsetHeight;
    return [
      this.fadeOutAndHide('#resultContainer', 'flex', {duration: 250}),

      // Fade out result options and keep faded out for the rest of animation.
      this.fadeOut('#resultOptions', {duration: 50}),
      this.maintainStyles(
          '#resultOptions', {opacity: 0},
          {delay: 50, duration: 200, fill: 'none'}),

      this.fadeIn('#loading', {delay: 100, duration: 100}),

      this.animate(
          '#bodyAndFooter',
          [
            {height: `${bodyHeight}px`},
            {height: 'var(--compose-loading-body-and-footer-height)'},
          ],
          {duration: 250, easing: STANDARD_EASING}, !isMac),
      this.animate(
          '#resultContainer',
          [
            {
              height: `${resultsHeight}px`,
              overflow: 'hidden',
              alignItems: 'flex-end',
            },
            {
              height: `${loadingHeight}px`,
              overflow: 'hidden',
              alignItems: 'flex-end',
            },
          ],
          {duration: 250, easing: STANDARD_EASING}, !isMac),
    ].flat();
  }

  transitionFromResultToEditing(resultContainerHeight: number): Animation[] {
    // Keep results body and footer visible while its contents animates out.
    const maintainResultsVisibility = this.maintainStyles(
        '#body, .footer', {
          overflow: 'hidden',
          visibility: 'visible',
        },
        {duration: 200});

    const bodyGapHeightAnimation = this.animate(
        '#body',
        [
          {gap: '8px'},
          {gap: '0px'},
        ],
        {duration: 200, easing: STANDARD_EASING});

    const resultContainerHeightAnimation = this.animate(
        '#resultContainer',
        [
          {
            height: `${resultContainerHeight}px`,
            overflow: 'hidden',
            alignItems: 'flex-end',
          },
          {
            height: '0px',
            overflow: 'hidden',
            alignItems: 'flex-end',
          },
        ],
        {duration: 200, easing: STANDARD_EASING});

    return [
      maintainResultsVisibility,
      bodyGapHeightAnimation,
      resultContainerHeightAnimation,

      // Fade out result UI and keep faded out for the rest of animation.
      this.fadeOut('#resultContainer, #resultFooter', {duration: 100}),
      this.maintainStyles(
          '#resultContainer, #resultFooter', {opacity: 0},
          {delay: 100, duration: 100, fill: 'none'}),

      // Fade in edit form.
      this.fadeIn('#editContainer', {duration: 200}),
      this.fadeIn('#editContainer .footer', {delay: 100, duration: 100}),
    ].flat();
  }

  transitionFromEditingToResult(resultContainerHeight: number): Animation[] {
    const bodyGapHeightAnimation = this.animate(
        '#body',
        [
          {gap: '0px'},
          {gap: '8px'},
        ],
        {duration: 200, easing: STANDARD_EASING});

    const resultContainerHeightAnimation = this.animate(
        '#resultContainer',
        [
          {
            height: '0px',
            overflow: 'hidden',
            alignItems: 'flex-end',
          },
          {
            height: `${resultContainerHeight}px`,
            overflow: 'hidden',
            alignItems: 'flex-end',
          },
        ],
        {duration: 200, easing: STANDARD_EASING});

    return [
      bodyGapHeightAnimation,
      resultContainerHeightAnimation,

      // Fade out edit form.
      this.fadeOutAndHide('#editContainer', 'flex', {duration: 200}),
      this.fadeOutAndHide('#editContainer .footer', 'flex', {duration: 100}),
      this.maintainStyles(
          '#editContainer .footer', {opacity: 0},
          {delay: 100, duration: 100, fill: 'none'}),

      // Fade in result UI.
      this.fadeIn(
          '#resultContainer, #resultFooter', {delay: 100, duration: 100}),
    ].flat();
  }
}
