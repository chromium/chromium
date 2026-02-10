// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '/strings.m.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {IntroBrowserProxy} from './browser_proxy.js';
import {IntroBrowserProxyImpl} from './browser_proxy.js';
import {getCss} from './sign_in_promo_refresh.css.js';
import {getHtml} from './sign_in_promo_refresh.html.js';

export interface SignInPromoRefreshElement {
  $: {
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
    };
  }

  private browserProxy_: IntroBrowserProxy =
      IntroBrowserProxyImpl.getInstance();
  protected accessor benefitCards_: BenefitCard[];
  protected accessor managedDeviceDisclaimer_: string = '';
  protected accessor isDeviceManaged_: boolean =
      loadTimeData.getBoolean('isDeviceManaged');
  protected accessor usePrimaryAndTonalButtonsForPromos_: boolean =
      loadTimeData.getBoolean('usePrimaryAndTonalButtonsForPromos');
  private accessor anyButtonClicked_: boolean = false;

  constructor() {
    super();
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
}

declare global {
  interface HTMLElementTagNameMap {
    'sign-in-promo-refresh': SignInPromoRefreshElement;
  }
}

customElements.define(SignInPromoRefreshElement.is, SignInPromoRefreshElement);
