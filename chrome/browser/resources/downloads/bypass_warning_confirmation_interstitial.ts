// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'bypass-warning-confirmation-interstitial' is the interstitial
 * that allows bypassing a download warning (keeping a file flagged as
 * dangerous). A 'success' indicates the warning interstitial was confirmed and
 * the dangerous file was downloaded.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './bypass_warning_confirmation_interstitial.html.js';


export interface DownloadsDangerousDownloadInterstitialElement {
  $: {
    dialog: HTMLDialogElement,
  };
}

export class DownloadsDangerousDownloadInterstitialElement extends
    PolymerElement {
  static get is() {
    return 'downloads-dangerous-download-interstitial';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      trustSiteLine: String,
    };
  }

  trustSiteLine: string;

  override connectedCallback() {
    super.connectedCallback();
    this.$.dialog.showModal();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'downloads-dangerous-download-interstitial':
        DownloadsDangerousDownloadInterstitialElement;
  }
}

customElements.define(
    DownloadsDangerousDownloadInterstitialElement.is,
    DownloadsDangerousDownloadInterstitialElement);
