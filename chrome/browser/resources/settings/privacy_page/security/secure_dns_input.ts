// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview `secure-dns-input` is a single-line text field that is used
 * with the secure DNS setting to configure custom servers. It is based on
 * `home-url-input`.
 */
import 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';

import type {SecurityPageBrowserProxy} from '/shared/settings/security_page/security_page_browser_proxy.js';
import {SecurityPageBrowserProxyImpl} from '/shared/settings/security_page/security_page_browser_proxy.js';
import type {CrTextareaElement} from 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../../i18n_setup.js';

import {getCss} from './secure_dns_input.css.js';
import {getHtml} from './secure_dns_input.html.js';

export interface SecureDnsInputElement {
  $: {
    input: CrTextareaElement,
  };
}

export class SecureDnsInputElement extends CrLitElement {
  static get is() {
    return 'secure-dns-input';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /*
       * The value of the input field.
       */
      value: {type: String},

      /**
       * The error text to display beneath the input field when |showError_| is
       * true.
       */
      errorText_: {type: String},
    };
  }

  accessor value: string = '';
  protected accessor errorText_: string = '';

  private browserProxy_: SecurityPageBrowserProxy =
      SecurityPageBrowserProxyImpl.getInstance();

  protected onValueChanged_(e: CustomEvent<{value: string}>) {
    this.value = e.detail.value;
  }

  protected onKeypress_(e: KeyboardEvent) {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      this.validate();
    }
  }

  protected onInputBlur_() {
    this.validate();
  }

  protected onInputChange_() {
    this.validate();
  }

  /**
   * This function ensures that while the user is entering input, especially
   * after pressing Enter, the input is not prematurely marked as invalid.
   */
  protected onInput_() {
    this.errorText_ = '';
  }

  /**
   * When the custom input field loses focus, validate the current value and
   * trigger an event with the result. If the value is valid, also attempt a
   * test query. Show an error message if the tested value is still the most
   * recent value, is non-empty, and was either invalid or failed the test
   * query.
   */
  async validate() {
    this.errorText_ = '';
    const valueToValidate = this.value;
    const valid = await this.browserProxy_.isValidConfig(valueToValidate);
    const successfulProbe =
        valid && (await this.browserProxy_.probeConfig(valueToValidate));
    // If there was an invalid template or no template can successfully
    // answer a probe query, show an error as long as the input field value
    // hasn't changed and is non-empty.
    if (valueToValidate === this.value && this.value !== '' &&
        !successfulProbe) {
      this.errorText_ = loadTimeData.getString(
          valid ? 'secureDnsCustomConnectionError' :
                  'secureDnsCustomFormatError');
    }
    this.fire('value-update', {isValid: valid, text: valueToValidate});
  }

  /**
   * Focus the custom dns input field.
   */
  override focus() {
    this.$.input.focusInput();
  }

  /**
   * @return whether an error is being shown.
   */
  protected isInvalid_(): boolean {
    return this.errorText_.length > 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'secure-dns-input': SecureDnsInputElement;
  }
}

customElements.define(SecureDnsInputElement.is, SecureDnsInputElement);
