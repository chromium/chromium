// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_components/cr_lottie/cr_lottie.js';

import type {CrLottieElement} from 'chrome://resources/cr_components/cr_lottie/cr_lottie.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory as finishOrContinueMojoProxyFactory} from '../finish_or_continue.mojom-webui.js';
import type {BrowserProxy as FinishOrContinueBrowserProxy} from '../finish_or_continue.mojom-webui.js';
import type {IntroBrowserProxy} from '../intro_browser_proxy.js';
import {IntroBrowserProxyImpl} from '../intro_browser_proxy.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {MatchMediaProxy} from './match_media_proxy.js';
import {MatchMediaProxyImpl} from './match_media_proxy.js';

export interface FinishOrContinueAppElement {
  $: {
    buttonContainer: HTMLElement,
    continueEducationButton: HTMLElement,
    startBrowsingButton: HTMLElement,
    leftAnimation: CrLottieElement,
    rightAnimation: CrLottieElement,
    bottomAnimation: CrLottieElement,
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
  private matchMediaProxy_: MatchMediaProxy = MatchMediaProxyImpl.getInstance();
  private browserProxy_: FinishOrContinueBrowserProxy =
      finishOrContinueMojoProxyFactory.getInstance();
  private introBrowserProxy_: IntroBrowserProxy =
      IntroBrowserProxyImpl.getInstance();
  private darkModeListener_: (e: MediaQueryListEvent) => void;
  private matchMedia_: MediaQueryList;
  private listenerIds_: number[] = [];

  constructor() {
    super();

    const params = new URLSearchParams(window.location.search);
    this.isFeatureShowcaseEligible_ = params.get('showcase') === 'true';

    this.matchMedia_ =
        this.matchMediaProxy_.matchMedia('(prefers-color-scheme: dark)');
    this.isDarkMode_ = this.matchMedia_.matches;
    this.darkModeListener_ = (e) => {
      this.isDarkMode_ = e.matches;
    };
  }

  override connectedCallback() {
    super.connectedCallback();
    this.matchMedia_.addEventListener('change', this.darkModeListener_);

    this.listenerIds_.push(
        this.introBrowserProxy_.callbackRouter.toggleAnimations.addListener(
            (active: boolean) => this.toggleAnimations_(active)));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.matchMedia_.removeEventListener('change', this.darkModeListener_);

    this.listenerIds_.forEach(
        id => this.introBrowserProxy_.callbackRouter.removeListener(id));
    this.listenerIds_ = [];
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

  private toggleAnimations_(active: boolean) {
    if (this.disableAnimations_) {
      return;
    }

    this.$.leftAnimation.setPlay(active);
    this.$.rightAnimation.setPlay(active);
    this.$.bottomAnimation.setPlay(active);
  }

  protected onStartBrowsingClick_() {
    this.browserProxy_.handler.startBrowsing();
  }

  protected onContinueEducationClick_() {
    this.browserProxy_.handler.continueEducation();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'finish-or-continue-app': FinishOrContinueAppElement;
  }
}

customElements.define(
    FinishOrContinueAppElement.is, FinishOrContinueAppElement);
