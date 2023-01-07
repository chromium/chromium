// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
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

/**
 * Indicate the variation of the consent card.
 *   * Default, StringChange, Dialog, and NativeDialog has one step.
 *   * Inline has two steps.
 *
 * This is the same enum in commerce_feature_list.h,
 * commerce::DiscountConsentNtpVariation.
 *
 * TODO(meiliang@): Use a mojo enum instead.
 */
export enum DiscountConsentVariation {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  DEFAULT = 0,
  STRING_CHANGE = 1,
  INLINE = 2,
  DIALOG = 3,
  NATIVE_DIALOG = 4
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
      merchants: Array,
      currentStep: {type: Number, value: 0},
      steps_: {
        type: Array,
        computed: 'computeSteps_(showCloseButton_, stepOneContent_)',
      },
      colorConsentContainer_: {
        type: Boolean,
        computed: 'computeColorConsentContainer_(currentStep)',
        reflectToAttribute: true,
      },
      showCloseButton_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean(
            'modulesCartDiscountInlineCardShowCloseButton'),
      },
      stepOneContent_:
          {type: String, computed: 'computeStepOneContent_(merchants)'},
      showDiscountConsentDialog_: {type: Boolean, value: false},
    };
  }

  // This is used to build the consent content string. This should not be empty
  // and it contains a maximum of 3 MerchantCart. The first 3 MerchantCart in
  // the Cart module.
  merchants: MerchantCart[];
  currentStep: number;
  // Whether the 'x' button is shown.
  private showCloseButton_: boolean;
  private steps_: Step[];
  // This is a Finch parameter that decides whether we should change container
  // background color.
  private colorConsentContainer_: boolean;
  private stepOneContent_: string;
  private showDiscountConsentDialog_: boolean;


  private getTotalStep_(): number {
    // Inline-variation is 2, see ntp_feature::DiscountConsentNtpVariation.
    if (loadTimeData.getInteger('modulesCartDiscountConsentVariation') ===
        DiscountConsentVariation.INLINE) {
      return 2;
    }
    return 1;
  }

  private getStepTwoContent_(): string {
    return loadTimeData.getString('modulesCartConsentStepTwoContent');
  }

  private computeColorConsentContainer_(currentStep: number) {
    return loadTimeData.getBoolean('modulesCartConsentStepTwoDifferentColor') &&
        currentStep === 1;
  }

  private computeSteps_(
      showCloseButton: boolean, stepOneContent: string): Step[] {
    const steps = [];
    steps.push({
      id: 'step1',
      content: stepOneContent,
      hasOneButton: true,
      actionButton: {
        text: loadTimeData.getString('modulesCartConsentStepOneButton'),
        onClickHandler: () => {
          chrome.metricsPrivate.recordUserAction(
              'NewTabPage.Carts.ShowInterestInDiscountConsent');
          this.dispatchEvent(
              new CustomEvent('discount-consent-continued', {composed: true}));
          if (loadTimeData.getInteger('modulesCartDiscountConsentVariation') ===
              DiscountConsentVariation.NATIVE_DIALOG) {
            return;
          }
          if (this.currentStep + 1 < this.getTotalStep_()) {
            this.currentStep++;
          } else {
            this.showDiscountConsentDialog_ = true;
          }
        },
      },
    });

    if (this.getTotalStep_() === 1) {
      return steps;
    }

    const step2: Step = {
      id: 'step2',
      content: this.getStepTwoContent_(),
      actionButton: {
        text: loadTimeData.getString('modulesCartDiscountConsentAccept'),
        onClickHandler: () => {
          this.dispatchEvent(
              new CustomEvent('discount-consent-accepted', {composed: true}));
        },
      },
    };
    if (showCloseButton) {
      step2.hasOneButton = true;
    } else {
      step2.hasTwoButtons = true;
      step2.cancelButton = {
        text: loadTimeData.getString('modulesCartDiscountConsentReject'),
        onClickHandler: () => {
          this.dispatchEvent(
              new CustomEvent('discount-consent-rejected', {composed: true}));
        },
      };
    }
    steps.push(step2);
    return steps;
  }

  private computeStepOneContent_(merchants: MerchantCart[]): string {
    const stepOneUseStaticContent =
        loadTimeData.getBoolean('modulesCartStepOneUseStaticContent');
    if (!stepOneUseStaticContent) {
      // TODO(crbug.com/1298116): We should also handle the case where the
      // string gets too long here.
      if (merchants.length === 1) {
        return loadTimeData.getStringF(
            'modulesCartConsentStepOneOneMerchantContent',
            merchants[0].merchant);
      } else if (merchants.length === 2) {
        return loadTimeData.getStringF(
            'modulesCartConsentStepOneTwoMerchantsContent',
            merchants[0].merchant, merchants[1].merchant);
      } else if (merchants.length >= 3) {
        return loadTimeData.getStringF(
            'modulesCartConsentStepOneThreeMerchantsContent',
            merchants[0].merchant, merchants[1].merchant);
      }
    }
    return loadTimeData.getString('modulesCartStepOneStaticContent');
  }

  private getFaviconUrl_(url: string): string {
    const faviconUrl = new URL('chrome://favicon2/');
    faviconUrl.searchParams.set('size', '20');
    faviconUrl.searchParams.set('scaleFactor', '1x');
    faviconUrl.searchParams.set('showFallbackMonogram', '');
    faviconUrl.searchParams.set('pageUrl', url);
    return faviconUrl.href;
  }

  private onCloseClick_() {
    if (this.currentStep === 0) {
      this.dispatchEvent(
          new CustomEvent('discount-consent-dismissed', {composed: true}));
    } else {
      this.dispatchEvent(
          new CustomEvent('discount-consent-rejected', {composed: true}));
    }
  }

  private onDiscountConsentDialogClose_() {
    this.showDiscountConsentDialog_ = false;
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'discount-consent-card': DiscountConsentCard;
  }
}

customElements.define(DiscountConsentCard.is, DiscountConsentCard);
