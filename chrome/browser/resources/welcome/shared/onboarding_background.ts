// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This element contains a set of SVGs that together acts as an
 * animated and responsive background for any page that contains it.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '../strings.m.js';

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './onboarding_background.css.js';
import {getHtml} from './onboarding_background.html.js';

export interface OnboardingBackgroundElement {
  $: {
    logo: HTMLElement,
  };
}

export class OnboardingBackgroundElement extends CrLitElement {
  static get is() {
    return 'onboarding-background';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      forcePaused_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  private animations_: Animation[] = [];
  private forcePaused_: boolean =
      window.matchMedia('(prefers-reduced-motion: reduce)').matches;

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
      const element = this.shadowRoot!.querySelector<HTMLElement>(`#${id}`);
      assert(element);
      this.createLineAnimation_(element, width);
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

  protected getPlayPauseIcon_(): string {
    return this.forcePaused_ ? 'welcome:play' : 'welcome:pause';
  }

  protected getPlayPauseLabel_(): string {
    return loadTimeData.getString(
        this.forcePaused_ ? 'landingPlayAnimations' : 'landingPauseAnimations');
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

  protected onLogoClick_() {
    this.$.logo.animate(
        {
          transform: [
            'translate(-50%, -50%)',
            'translate(-50%, -50%) rotate(-10turn)',
          ],
        },
        {
          duration: 500,
          easing: 'cubic-bezier(1, 0, 0, 1)',
        });
  }

  protected onPlayPauseClick_() {
    if (this.forcePaused_) {
      this.play();
    } else {
      this.pause();
    }

    this.forcePaused_ = !this.forcePaused_;
  }

  pause() {
    this.animations_.forEach(animation => animation.pause());
  }

  play() {
    if (this.forcePaused_) {
      return;
    }

    this.animations_.forEach(animation => animation.play());
  }
}
customElements.define(
    OnboardingBackgroundElement.is, OnboardingBackgroundElement);
