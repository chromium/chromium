// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview `secure-dns-input` is a single-line text field that is used
 * with the secure DNS setting to configure custom servers. It is based on
 * `home-url-input`.
 */
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';

import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {PrivacyPageBrowserProxy, PrivacyPageBrowserProxyImpl} from './privacy_page_browser_proxy.js';
import {getTemplate} from './secure_dns_input.html.js';

export interface SecureDnsInputElement {
  $: {
    input: CrInputElement,
  };
}

export class SecureDnsInputElement extends PolymerElement {
  static get is() {
    return 'secure-dns-input';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /*
       * The value of the input field.
       */
      value: String,

      /*
       * Whether |errorText| should be displayed beneath the input field.
       */
      showError_: Boolean,

      /**
       * The error text to display beneath the input field when |showError_| is
       * true.
       */
      errorText_: String,
    };
  }

  value: string;
  private showError_: boolean;
  private errorText_: string;
  private browserProxy_: PrivacyPageBrowserProxy =
      PrivacyPageBrowserProxyImpl.getInstance();

  /**
   * This function ensures that while the user is entering input, especially
   * after pressing Enter, the input is not prematurely marked as invalid.
   */
  private onInput_() {
    this.showError_ = false;
  }

  /**
   * When the custom input field loses focus, validate the current value and
   * trigger an event with the result. If the value is valid, also attempt a
   * test query. Show an error message if the tested value is still the most
   * recent value, is non-empty, and was either invalid or failed the test
   * query.
   */
  async validate() {
    this.showError_ = false;
    const valueToValidate = this.value;
    const valid = await this.browserProxy_.isValidConfig(valueToValidate);
    const successfulProbe =
        valid && await this.browserProxy_.probeConfig(valueToValidate);
    // If there was an invalid template or no template can successfully
    // answer a probe query, show an error as long as the input field value
    // hasn't changed and is non-empty.
    if (valueToValidate === this.value && this.value !== '' &&
        !successfulProbe) {
      this.errorText_ = loadTimeData.getString(
          valid ? 'secureDnsCustomConnectionError' :
                  'secureDnsCustomFormatError');
      this.showError_ = true;
    }
    this.dispatchEvent(new CustomEvent('value-update', {
      bubbles: true,
      composed: true,
      detail: {isValid: valid, text: valueToValidate}
    }));
  }

  /**
   * Focus the custom dns input field.
   */
  override focus() {
    this.$.input.focus();
  }

  /**
   * @return whether an error is being shown.
   */
  isInvalid(): boolean {
    return !!this.showError_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'secure-dns-input': SecureDnsInputElement;
  }
}

customElements.define(SecureDnsInputElement.is, SecureDnsInputElement);
