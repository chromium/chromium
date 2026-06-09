// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_lottie/cr_lottie.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '/strings.m.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrLottieElement} from 'chrome://resources/cr_elements/cr_lottie/cr_lottie.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {IntroBrowserProxy} from './browser_proxy.js';
import {IntroBrowserProxyImpl} from './browser_proxy.js';
import type {IntroBrowserProxy as IntroMojoBrowserProxy} from './intro_browser_proxy.js';
import {IntroBrowserProxyImpl as IntroMojoBrowserProxyImpl} from './intro_browser_proxy.js';
import {getCss} from './sign_in_promo_refresh.css.js';
import {getHtml} from './sign_in_promo_refresh.html.js';

// LINT.IfChange(Variation)
export enum Variation {
  DEFAULT = 0,
  DONT_SIGN_IN_IN_TOP_RIGHT_CORNER = 1,
  DONT_SIGN_IN_ON_GAIA = 2,
}
// LINT.ThenChange(//components/signin/public/base/signin_switches.h:FirstRunDesktopSignInPromoVariation)

export interface SignInPromoRefreshElement {
  $: {
    leftAnimation: CrLottieElement,
    rightAnimation: CrLottieElement,
    bottomAnimation: CrLottieElement,
    acceptSignInButton: CrButtonElement,
    buttonRow: HTMLElement,
    declineSignInButton: CrButtonElement,
    disclaimerText: HTMLElement,
    managedDeviceDisclaimer: HTMLElement,
  };
}

export interface BenefitCard {
  title: string;
  description: string;
  iconId: string;
}

const SignInPromoRefreshElementBase =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export class SignInPromoRefreshElement extends SignInPromoRefreshElementBase {
  static get is(): string {
    return 'sign-in-promo-refresh';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * The list of benefits the user will get when signed in to Chrome
       */
      benefitCards_: {type: Array},

      managedDeviceDisclaimer_: {type: String},
      isDeviceManaged_: {type: Boolean},
      anyButtonClicked_: {type: Boolean},
      usePrimaryAndTonalButtonsForPromos_: {type: Boolean},
      shouldDisableAnimations_: {type: Boolean},
      isDarkMode_: {type: Boolean},
    };
  }

  protected accessor benefitCards_: BenefitCard[];
  protected accessor managedDeviceDisclaimer_: string = '';
  protected accessor isDeviceManaged_: boolean =
      loadTimeData.getBoolean('isDeviceManaged');
  protected accessor usePrimaryAndTonalButtonsForPromos_: boolean =
      loadTimeData.getBoolean('usePrimaryAndTonalButtonsForPromos');
  // Animations are disabled if the feature is disabled (there is no mechanism
  // to stop animations) or if we are using "disable animations" test flag.
  protected accessor shouldDisableAnimations_: boolean =
      loadTimeData.getBoolean('disableAnimations') ||
      !loadTimeData.getBoolean('isFirstRunDesktopRevampEnabled');
  protected accessor isDarkMode_: boolean;
  private accessor anyButtonClicked_: boolean = false;
  private browserProxy_: IntroBrowserProxy =
      IntroBrowserProxyImpl.getInstance();
  private introBrowserProxy_: IntroMojoBrowserProxy =
      IntroMojoBrowserProxyImpl.getInstance();
  private variation_: Variation =
      loadTimeData.getInteger('signInPromoVariation') as Variation;
  private darkModeListener_: (e: MediaQueryListEvent) => void;
  private matchMedia_: MediaQueryList;
  private listenerIds_: number[] = [];

  constructor() {
    super();
    this.matchMedia_ =
        this.browserProxy_.matchMedia('(prefers-color-scheme: dark)');
    this.isDarkMode_ = this.matchMedia_.matches;
    this.darkModeListener_ = (e) => {
      this.isDarkMode_ = e.matches;
    };
    this.benefitCards_ = [
      {
        title: this.i18n('devicesCardTitle'),
        description: this.i18n('devicesCardDescription'),
        iconId: 'devices',
      },
      {
        title: this.i18n('securityCardTitle'),
        description: this.i18n('securityCardDescription'),
        iconId: 'security',
      },
      {
        title: this.i18n('backupCardTitle'),
        description: this.i18n('backupCardDescription'),
        iconId: 'cloud-upload',
      },
    ];
  }

  override connectedCallback() {
    super.connectedCallback();

    this.browserProxy_.initializeMainView();

    if (this.isDeviceManaged_) {
      this.addWebUiListener(
          'managed-device-disclaimer-updated',
          this.onManagedDeviceDisclaimerUpdated_.bind(this));
    }

    this.addWebUiListener('reset-intro-buttons', this.resetButtons_.bind(this));
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

  private resetButtons_() {
    this.anyButtonClicked_ = false;
  }

  private onManagedDeviceDisclaimerUpdated_(disclaimer: string) {
    this.managedDeviceDisclaimer_ = disclaimer;
  }

  /**
   * Disable buttons if the device is managed until the management
   * disclaimer is loaded or if a button was clicked.
   */
  protected shouldDisableButtons_(): boolean {
    return (this.isDeviceManaged_ &&
            this.managedDeviceDisclaimer_.length === 0) ||
        this.anyButtonClicked_;
  }

  protected onAcceptSignInButtonClick_() {
    this.anyButtonClicked_ = true;
    this.browserProxy_.continueWithAccount();
  }

  protected onDeclineSignInButtonClick_() {
    this.anyButtonClicked_ = true;
    this.browserProxy_.continueWithoutAccount();
  }

  /**
   * To keep the layout stable during animations, for managed devices it is
   * invisible while we're fetching the text to display.
   */
  protected getDisclaimerVisibilityClass_(): string {
    return this.managedDeviceDisclaimer_.length === 0 ? 'temporarily-hidden' :
                                                        'fast-fade-in';
  }

  protected isDefaultVariation_(): boolean {
    return this.variation_ === Variation.DEFAULT;
  }

  protected isTopRightCornerVariation_(): boolean {
    return this.variation_ === Variation.DONT_SIGN_IN_IN_TOP_RIGHT_CORNER;
  }

  protected getAnimationUrl_(position: 'left'|'right'|'bottom'): string {
    // If animations are disabled entirely (e.g. via revamp disabled or
    // testing), we load static JSON files for the light theme that start from
    // frame 180 (resting state) instead of frame 0. We don't need separate
    // files for the dark theme because the dark JSON files starting from frame
    // 0 are acceptable from UX point of view.
    const staticSuffix =
        !this.isDarkMode_ && this.shouldDisableAnimations_ ? '_static' : '';
    const theme = this.isDarkMode_ ? 'dark' : 'light';
    return `chrome://intro/animations/signin_benefits_${theme}_${position}${
        staticSuffix}.json`;
  }

  private toggleAnimations_(active: boolean) {
    if (this.shouldDisableAnimations_) {
      return;
    }

    this.$.leftAnimation.setPlay(active);
    this.$.rightAnimation.setPlay(active);
    this.$.bottomAnimation.setPlay(active);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'sign-in-promo-refresh': SignInPromoRefreshElement;
  }
}

customElements.define(SignInPromoRefreshElement.is, SignInPromoRefreshElement);
