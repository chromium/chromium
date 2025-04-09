// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'pay-over-time-issuer-list-entry' is an Pay Over Time issuer
 * row to be shown on the settings page.
 */

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../i18n_setup.js';
import '../settings_shared.css.js';
import './passwords_shared.css.js';

import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './pay_over_time_issuer_list_entry.html.js';

export class SettingsPayOverTimeIssuerListEntryElement extends PolymerElement {
  static get is() {
    return 'settings-pay-over-time-issuer-list-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      payOverTimeIssuer: Object,
    };
  }

  declare payOverTimeIssuer: chrome.autofillPrivate.PayOverTimeIssuerEntry;

  /**
   * When the provided `imageSrc` points toward an issuer's default logo art,
   * this function returns a string that will scale the image based on the
   * user's screen resolution, otherwise it will return the unmodified
   * `imageSrc`.
   */
  private getIssuerImage_(imageSrc: string): string {
    return imageSrc.startsWith('chrome://theme') ?
        this.getScaledSrcSet_(imageSrc) :
        imageSrc;
  }

  /**
   * This function returns a string that can be used in a srcset to scale
   * the provided `url` based on the user's screen resolution.
   */
  private getScaledSrcSet_(url: string): string {
    return `${url} 1x, ${url}@2x 2x`;
  }

  private onRemoteEditClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('managePaymentMethodsUrl'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-pay-over-time-issuer-list-entry':
        SettingsPayOverTimeIssuerListEntryElement;
  }
}

customElements.define(
    SettingsPayOverTimeIssuerListEntryElement.is,
    SettingsPayOverTimeIssuerListEntryElement);
