// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_lottie/cr_lottie.js';
import '/strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {IntroBrowserProxy} from '../browser_proxy.js';
import {IntroBrowserProxyImpl} from '../browser_proxy.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export interface FinishOrContinueAppElement {
  $: {
    buttonContainer: HTMLElement,
    continueEducationButton: HTMLElement,
    startBrowsingButton: HTMLElement,
  };
}

export class FinishOrContinueAppElement extends CrLitElement {
  static get is() {
    return 'finish-or-continue-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isDarkMode_: {type: Boolean},
      disableAnimations_: {type: Boolean},
    };
  }

  protected accessor isDarkMode_: boolean = false;
  protected accessor disableAnimations_: boolean =
      loadTimeData.getBoolean('disableAnimations');

  private isFeatureShowcaseEligible_: boolean = false;
  private browserProxy_: IntroBrowserProxy =
      IntroBrowserProxyImpl.getInstance();
  private darkModeListener_: (e: MediaQueryListEvent) => void;
  private matchMedia_: MediaQueryList;

  constructor() {
    super();

    const params = new URLSearchParams(window.location.search);
    this.isFeatureShowcaseEligible_ = params.get('showcase') === 'true';

    this.matchMedia_ =
        this.browserProxy_.matchMedia('(prefers-color-scheme: dark)');
    this.isDarkMode_ = this.matchMedia_.matches;
    this.darkModeListener_ = (e) => {
      this.isDarkMode_ = e.matches;
    };
  }

  override connectedCallback() {
    super.connectedCallback();
    this.matchMedia_.addEventListener('change', this.darkModeListener_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.matchMedia_.removeEventListener('change', this.darkModeListener_);
  }

  protected getContinueEducationButtonLabel_(): string {
    return this.isFeatureShowcaseEligible_
        ? loadTimeData.getString('seeMoreTipsButtonLabel')
        : loadTimeData.getString('seeWhatsNewButtonLabel');
  }

  protected getAnimationUrl_(position: 'left'|'right'|'bottom'): string {
    return (this.isDarkMode_) ?
        `chrome://intro/animations/finish_or_continue_dark_${position}.json` :
        `chrome://intro/animations/finish_or_continue_light_${position}.json`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'finish-or-continue-app': FinishOrContinueAppElement;
  }
}

customElements.define(
    FinishOrContinueAppElement.is, FinishOrContinueAppElement);
