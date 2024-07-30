// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import './strings.m.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {IntroBrowserProxy} from './browser_proxy.js';
import {IntroBrowserProxyImpl} from './browser_proxy.js';
import {getCss} from './sign_in_promo.css.js';
import {getHtml} from './sign_in_promo.html.js';

export interface SignInPromoElement {
  $: {
    acceptSignInButton: CrButtonElement,
    buttonRow: HTMLElement,
    contentArea: HTMLElement,
    declineSignInButton: CrButtonElement,
    disclaimerText: HTMLElement,
    managedDeviceDisclaimer: HTMLElement,
    safeZone: HTMLElement,
  };
}

export interface BenefitCard {
  title: string;
  description: string;
  iconId: string;
}

const SignInPromoElementBase =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export class SignInPromoElement extends SignInPromoElementBase {
  static get is() {
    return 'sign-in-promo';
  }

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
    };
  }

  private browserProxy_: IntroBrowserProxy =
      IntroBrowserProxyImpl.getInstance();
  protected benefitCards_: BenefitCard[];
  private divisionLineResizeObserver_: ResizeObserver|null = null;
  protected managedDeviceDisclaimer_: string = '';
  protected isDeviceManaged_: boolean =
      loadTimeData.getBoolean('isDeviceManaged');
  private anyButtonClicked_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();

    this.browserProxy_.initializeMainView();
    this.toggleDivisionLine_();

    if (this.isDeviceManaged_) {
      this.addWebUiListener(
          'managed-device-disclaimer-updated',
          this.handleManagedDeviceDisclaimerUpdate_.bind(this));
    }

    this.addWebUiListener('reset-intro-buttons', this.resetButtons_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.divisionLineResizeObserver_!.disconnect();
  }

  override firstUpdated() {
    this.addEventListener(
        'view-enter-start', this.onViewEnterStart_.bind(this));
  }

  private onViewEnterStart_() {
    this.setTranslationHeightToAlignLogoAndAnimation_();
  }

  private toggleDivisionLine_() {
    const safeZone = this.$.safeZone;

    this.divisionLineResizeObserver_ = new ResizeObserver(() => {
      this.$.buttonRow.classList.toggle(
          'division-line', safeZone.scrollHeight > safeZone.clientHeight);
    });
    this.divisionLineResizeObserver_.observe(safeZone);
  }

  private resetButtons_() {
    this.anyButtonClicked_ = false;
  }

  private handleManagedDeviceDisclaimerUpdate_(disclaimer: string) {
    this.managedDeviceDisclaimer_ = disclaimer;
  }

  /**
   * Disable buttons if the device is managed until the management
   * disclaimer is loaded or if a button was clicked.
   */
  protected areButtonsDisabled_(): boolean {
    return (this.isDeviceManaged_ &&
            this.managedDeviceDisclaimer_.length === 0) ||
        this.anyButtonClicked_;
  }

  // At the start of the signInPromo animation, the product logo should be at
  // the same position as the splash view logo animation. To be able
  // to do that, we had to translate the safeZone vertically up by the value
  // calculated in the function below, after doing top:50%.
  private setTranslationHeightToAlignLogoAndAnimation_() {
    const contentAreaHeight = this.$.contentArea.clientHeight;
    const safeZoneHeight = this.$.safeZone.clientHeight;
    const productLogoMarginTop = parseInt(
        getComputedStyle(this).getPropertyValue('--product-logo-margin-top'));
    const productLogoSize = parseInt(
        getComputedStyle(this).getPropertyValue('--product-logo-size'));

    const contentAreaAndSafeZoneHeightDifference =
        contentAreaHeight < safeZoneHeight ?
        safeZoneHeight - contentAreaHeight :
        0;
    const translationHeight = contentAreaAndSafeZoneHeightDifference / 2 +
        productLogoMarginTop + productLogoSize / 2;

    this.style.setProperty(
        '--safe-zone-animation-translation-height', translationHeight + 'px');
  }

  protected onContinueWithAccountClick_() {
    this.anyButtonClicked_ = true;
    this.browserProxy_.continueWithAccount();
  }

  protected onContinueWithoutAccountClick_() {
    this.anyButtonClicked_ = true;
    this.browserProxy_.continueWithoutAccount();
  }

  // To keep the layout stable during animations, for managed devices it is
  // invisible while we're fetching the text to display.
  protected getDisclaimerVisibilityClass_() {
    return this.managedDeviceDisclaimer_.length === 0 ? 'temporarily-hidden' :
                                                        'fast-fade-in';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'sign-in-promo': SignInPromoElement;
  }
}

customElements.define(SignInPromoElement.is, SignInPromoElement);
