// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MerchantCart} from '../../chrome_cart.mojom-webui.js';
import {I18nMixin, loadTimeData} from '../../i18n_setup.js';

import {getTemplate} from './discount_consent_card.html.js';

/**
 * This interface contains information that is required to build a text button
 * in each Step in the DiscountConsentCard.
 */
interface ButtonInfo {
  /* The text shows in side the button.*/
  text: string;
  /* The on-click handler. */
  onClickHandler: () => void;
}

/**
 * Contains information for each step in the DiscountConsentCard. These
 * information are needed to build the iron-pages DOM.
 */
interface Step {
  /* Unique id for each step, this is used as the id attribution for each
   * step.*/
  id: string;
  /* Content for each step. */
  content: string;
  /* Indicates whether the current step has one or two buttons. */
  hasOneButton?: boolean;
  /* Indicates whether the current step has one or two buttons. */
  hasTwoButtons?: boolean;
  /* Required information to build the action button. */
  actionButton?: ButtonInfo;
  /* Required information to build the cancel button. */
  cancelButton?: ButtonInfo;
}

// This is a configurable multi-step card. Each step is represented by the Step
// interface.
export class DiscountConsentCard extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'discount-consent-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      merchants: {type: Array, observer: 'onMerchantsChanged_'},
      currentStep: {type: Number, value: 0},
      steps_: Array,
      colorConsentContainer_: {
        type: Boolean,
        computed: 'computeColorConsentContainer_(currentStep)',
        reflectToAttribute: true
      }
    };
  }

  // This is used to build the consent content string. This should not be empty
  // and it contains a maximum of 3 MerchantCart. The first 3 MerchantCart in
  // the Cart module.
  merchants: MerchantCart[];
  currentStep: number;
  private steps_: Array<Step> = [];
  // This is a Finch parameter that decides whether we should change container
  // background color.
  private colorConsentContainer_: boolean;

  constructor() {
    super();

    this.steps_.push({
      id: 'step1',
      content: this.getStepOneContent_(),
      hasOneButton: true,
      actionButton: {
        // TODO(crbug.com/1298116): Load string from resource.
        text: 'continue',
        onClickHandler: () => {
          if (this.currentStep < this.getTotalStep_()) {
            this.currentStep++;
          } else {
            // TODO(crbug.com/1298116): Show DiscountConsentDialog.
          }
          // TODO(crbug.com/1298116): Record user click on this button.
        },
      }
    });

    // TODO(crbug.com/1298116): Gate second step with a finch flag.
    this.steps_.push({
      id: 'step2',
      content: this.getStepTwoContent_(),
      hasTwoButtons: true,
      actionButton: {
        text: loadTimeData.getString('modulesCartDiscountConsentAccept'),
        onClickHandler: () => {
          this.dispatchEvent(
              new CustomEvent('discount-consent-accepted', {composed: true}));
        },
      },
      cancelButton: {
        text: loadTimeData.getString('modulesCartDiscountConsentReject'),
        onClickHandler: () => {
          this.dispatchEvent(
              new CustomEvent('discount-consent-rejected', {composed: true}));
        }
      }
    });
  }

  private getTotalStep_(): number {
    // TODO(crbug.com/1298116): Return number based on finch flags. If
    // inline-variation return 2, otherwise return 1.
    return 2;
  }

  private getStepOneContent_(): string {
    // TODO(crbug.com/1298116): Return strings based on finch flags.
    return loadTimeData.getString('modulesCartDiscountConsentContent');
  }

  private getStepTwoContent_(): string {
    // TODO(crbug.com/1298116): Return strings based on finch flags.
    return loadTimeData.getString('modulesCartDiscountConsentContent');
  }

  private computeColorConsentContainer_(currentStep: number) {
    return loadTimeData.getBoolean('modulesCartConsentStepTwoDifferentColor') &&
        currentStep === 1;
  }

  private onMerchantsChanged_() {
    if (this.currentStep === 0) {
      // TODO(crbug.com/1298116): Build strings with merchant names and string
      // template from resource. We should also handle the case where the string
      // gets too long here.
      this.steps_[0].content = this.merchants[0].merchant + ', ' +
          this.merchants[1].merchant + ' and more';
    }
  }

  private getFaviconUrl_(url: string): string {
    const faviconUrl = new URL('chrome://favicon2/');
    faviconUrl.searchParams.set('size', '24');
    faviconUrl.searchParams.set('scale_factor', '1x');
    faviconUrl.searchParams.set('show_fallback_monogram', '');
    faviconUrl.searchParams.set('page_url', url);
    return faviconUrl.href;
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'discount-consent-card': DiscountConsentCard;
  }
}

customElements.define(DiscountConsentCard.is, DiscountConsentCard);
