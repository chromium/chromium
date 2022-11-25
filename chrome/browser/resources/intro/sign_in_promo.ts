// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './icons.html.js';
import './strings.m.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {IntroBrowserProxy, IntroBrowserProxyImpl} from './browser_proxy.js';
import {getTemplate} from './sign_in_promo.html.js';

export interface SignInPromoElement {
  $: {
    acceptSignInButton: CrButtonElement,
    buttonContainer: HTMLElement,
    contentArea: HTMLElement,
    declineSignInButton: CrButtonElement,
    safeZone: HTMLElement,
  };
}

export interface BenefitCard {
  title: string;
  description: string;
  iconName: string;
}

const SignInPromoElementBase = I18nMixin(PolymerElement);

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
        iconName: 'intro:devices',
      },
      {
        title: this.i18n('securityCardTitle'),
        description: this.i18n('securityCardDescription'),
        iconName: 'cr:security',
      },
      {
        title: this.i18n('backupCardTitle'),
        description: this.i18n('backupCardDescription'),
        iconName: 'intro:cloud-upload',
      },
    ];
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      benefitCards_: {
        type: Array,
      },
    };
  }

  private browserProxy_: IntroBrowserProxy =
      IntroBrowserProxyImpl.getInstance();
  private benefitCards_: BenefitCard[];
  private divisionLineResizeObserver_: ResizeObserver|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.toggleDivisionLine_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.divisionLineResizeObserver_!.disconnect();
  }

  override ready() {
    super.ready();
    this.addEventListener('view-enter-start', this.onViewEnterStart_);
  }

  private onViewEnterStart_() {
    this.setTranslationHeightToAlignLogoAndAnimation();
  }

  private toggleDivisionLine_() {
    const safeZone = this.$.safeZone;

    this.divisionLineResizeObserver_ = new ResizeObserver(() => {
      this.$.buttonContainer.classList.toggle(
          'division-line', safeZone.scrollHeight > safeZone.clientHeight);
    });
    this.divisionLineResizeObserver_.observe(safeZone);
  }

  // At the start of the signInPromo animation, the product logo should be at
  // the same position as the splash view logo animation. To be able
  // to do that, we had to translate the safeZone vertically up by the value
  // calculated in the function below, after doing top:50%.
  private setTranslationHeightToAlignLogoAndAnimation() {
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

  private onContinueWithAccountClick_() {
    this.browserProxy_.continueWithAccount();
  }

  private onContinueWithoutAccountClick_() {
    this.browserProxy_.continueWithoutAccount();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'sign-in-promo': SignInPromoElement;
  }
}

customElements.define(SignInPromoElement.is, SignInPromoElement);
