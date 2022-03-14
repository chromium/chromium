// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This element contains a set of SVGs that together acts as an
 * animated and responsive background for any page that contains it.
 */

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getTemplate} from './onboarding_background.html.js';

/** @polymer */
export class OnboardingBackgroundElement extends PolymerElement {
  static get is() {
    return 'onboarding-background';
  }

  static get template() {
    return getTemplate();
  }

  private animations_: Animation[] = [];

  override connectedCallback() {
    super.connectedCallback();
    const details: Array<[string, number]> = [
      ['blue-line', 60],
      ['green-line', 68],
      ['red-line', 45],
      ['grey-line', 68],
      ['yellow-line', 49],
    ];
    details.forEach(([id, width]) => {
      this.createLineAnimation_(
          (this.shadowRoot!.querySelector(`#${id}`) as HTMLElement), width);
    });
  }

  private createLineAnimation_(lineContainer: HTMLElement, width: number) {
    const line = lineContainer.firstElementChild as HTMLElement;
    const lineFill = line.firstElementChild as HTMLElement;
    const pointOptions = {
      endDelay: 3250,
      fill: 'forwards' as FillMode,
      duration: 750,
    };

    const startPointAnimation = lineFill.animate(
        [
          {width: '0px'},
          {width: `${width}px`},
        ],
        Object.assign({}, pointOptions, {easing: 'cubic-bezier(.6,0,0,1)'}));
    startPointAnimation.pause();
    this.animations_.push(startPointAnimation);
    this.loopAnimation_(startPointAnimation);

    const endPointWidthAnimation = line.animate(
        [
          {width: `${width}px`},
          {width: '0px'},
        ],
        Object.assign(
            {}, pointOptions, {easing: 'cubic-bezier(.66,0,.86,.25)'}));
    endPointWidthAnimation.pause();
    this.animations_.push(endPointWidthAnimation);
    this.loopAnimation_(endPointWidthAnimation);

    const endPointTransformAnimation = line.animate(
        [
          {transform: `translateX(0)`},
          {transform: `translateX(${width}px)`},
        ],
        Object.assign({}, pointOptions, {
          easing: 'cubic-bezier(.66,0,.86,.25)',
        }));
    endPointTransformAnimation.pause();
    this.animations_.push(endPointTransformAnimation);
    this.loopAnimation_(endPointTransformAnimation);

    const lineTransformAnimation = lineContainer.animate(
        [
          {transform: `translateX(0)`},
          {transform: `translateX(40px)`},
        ],
        {
          composite: 'add',  // There is already a rotate on the line.
          duration: 1500,
          easing: 'cubic-bezier(0,.56,.46,1)',
          endDelay: 2500,
          fill: 'forwards',
        });
    lineTransformAnimation.pause();
    this.animations_.push(lineTransformAnimation);
    this.loopAnimation_(lineTransformAnimation);
  }

  private loopAnimation_(animation: Animation) {
    // Animations that have a delay after them can only be looped by re-playing
    // them as soon as they finish. The |endDelay| property of JS animations
    // only works if |iterations| is 1, and the |delay| property runs before
    // the animation even plays.
    animation.onfinish = () => {
      animation.play();
    };
  }

  pause() {
    this.animations_.forEach(animation => animation.pause());
  }

  play() {
    this.animations_.forEach(animation => animation.play());
  }
}
customElements.define(
    OnboardingBackgroundElement.is, OnboardingBackgroundElement);
