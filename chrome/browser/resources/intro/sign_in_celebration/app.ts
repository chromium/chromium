// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_lottie/cr_lottie.js';
import '/strings.m.js';

import type {CrLottieElement} from 'chrome://resources/cr_elements/cr_lottie/cr_lottie.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SignInCelebrationUserInfo} from '../sign_in_celebration.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {SignInCelebrationBrowserProxyImpl} from './sign_in_celebration_browser_proxy.js';
import type {SignInCelebrationBrowserProxy} from './sign_in_celebration_browser_proxy.js';

export interface SignInCelebrationAppElement {
  $: {
    avatarAnimation: CrLottieElement,
    avatar: HTMLImageElement,
    title: HTMLElement,
  };
}

const SignInCelebrationAppElementBase = I18nMixinLit(CrLitElement);

export class SignInCelebrationAppElement extends
    SignInCelebrationAppElementBase {
  static get is() {
    return 'sign-in-celebration-app';
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
      userInfo_: {type: Object},
    };
  }

  protected accessor isDarkMode_: boolean = false;
  protected accessor disableAnimations_: boolean =
      loadTimeData.getBoolean('disableAnimations');
  protected accessor userInfo_: SignInCelebrationUserInfo = {
    avatarUrl: loadTimeData.getString('accountPicturePlaceholderUrl'),
    title: '',
  };

  private browserProxy_: SignInCelebrationBrowserProxy =
      SignInCelebrationBrowserProxyImpl.getInstance();
  private darkModeListener_: (e: MediaQueryListEvent) => void;
  private matchMedia_: MediaQueryList;
  private listenerIds_: number[] = [];

  constructor() {
    super();
    this.matchMedia_ =
        this.browserProxy_.matchMedia('(prefers-color-scheme: dark)');
    this.isDarkMode_ = this.matchMedia_.matches;
    this.darkModeListener_ = (e: MediaQueryListEvent) => {
      this.isDarkMode_ = e.matches;
    };
  }

  override connectedCallback() {
    super.connectedCallback();
    this.matchMedia_.addEventListener('change', this.darkModeListener_);

    this.listenerIds_.push(
        this.browserProxy_.callbackRouter.onSignInCelebrationUserInfoUpdated
            .addListener(userInfo => this.updateUserInfo_(userInfo)));

    this.browserProxy_.handler.getSignInCelebrationUserInfo().then(
        res => this.updateUserInfo_(res.userInfo));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.matchMedia_.removeEventListener('change', this.darkModeListener_);

    this.listenerIds_.forEach(
        id => assert(this.browserProxy_.callbackRouter.removeListener(id)));
    this.listenerIds_ = [];
  }

  protected getAnimationUrl_(): string {
    return this.isDarkMode_ ?
        'animations/avatar_sign_in_celebration_dark.json' :
        'animations/avatar_sign_in_celebration.json';
  }

  private updateUserInfo_(userInfo: SignInCelebrationUserInfo) {
    this.userInfo_ = userInfo;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'sign-in-celebration-app': SignInCelebrationAppElement;
  }
}

customElements.define(
    SignInCelebrationAppElement.is, SignInCelebrationAppElement);
