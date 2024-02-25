// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'eol-offer-section' contains information and a link for an
 * offer related to the device's end of life.
 */

import '../os_settings_icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AboutPageBrowserProxyImpl} from './about_page_browser_proxy.js';
import {getTemplate} from './eol_offer_section.html.js';

class EolOfferSection extends PolymerElement {
  static get is() {
    return 'eol-offer-section' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      shouldShowOfferText: {
        type: Boolean,
        value: false,
      },
    };
  }

  shouldShowOfferText: boolean;

  private onIncentiveButtonClick_(): void {
    AboutPageBrowserProxyImpl.getInstance().endOfLifeIncentiveButtonClicked();
  }

  private getEolTitleText_(): string {
    return loadTimeData.getString(
        this.shouldShowOfferText ? 'eolIncentiveOfferTitle' :
                                   'eolIncentiveNoOfferTitle');
  }

  private getEolMessageText_(): string {
    return loadTimeData.getString(
        this.shouldShowOfferText ? 'eolIncentiveOfferMessage' :
                                   'eolIncentiveNoOfferMessage');
  }

  private getEolButtonText_(): string {
    return loadTimeData.getString(
        this.shouldShowOfferText ? 'eolIncentiveButtonOfferText' : 'learnMore');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EolOfferSection.is]: EolOfferSection;
  }
}

customElements.define(EolOfferSection.is, EolOfferSection);
