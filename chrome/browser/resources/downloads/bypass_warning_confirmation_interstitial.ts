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

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './bypass_warning_confirmation_interstitial.html.js';


export interface DownloadsDangerousDownloadInterstitialElement {
  $: {
    dialog: HTMLDialogElement,
    continueAnywayButton: CrButtonElement,
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
      hideSurveyAndDownloadButton_: {
        type: Boolean,
        value: true,
      },

      trustSiteLine: String,
    };
  }

  trustSiteLine: string;

  private boundKeydown_: ((e: KeyboardEvent) => void)|null = null;
  private hideSurveyAndDownloadButton_: boolean;

  override connectedCallback() {
    super.connectedCallback();
    this.$.dialog.showModal();
    this.disableEscapeKey_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.removeKeydownListener_();
  }

  private disableEscapeKey_() {
    this.boundKeydown_ = this.boundKeydown_ || this.onKeydown_.bind(this);
    this.addEventListener('keydown', this.boundKeydown_);
    // Sometimes <body> is key event's target and in that case the event
    // will bypass dialog. We should consume those events too in order to
    // modally. This prevents cancelling the interstitial via keyboard events.
    document.body.addEventListener('keydown', this.boundKeydown_);
  }

  private onBackToSafetyClick_() {
    this.$.dialog.close();
    assert(!this.$.dialog.open);
    this.dispatchEvent(
        new CustomEvent('cancel', {bubbles: true, composed: true}));
  }

  private onContinueAnywayClick_() {
    const continueAnywayButton = this.$.continueAnywayButton;
    assert(!!continueAnywayButton);
    continueAnywayButton.setAttribute('disabled', 'true');
    this.hideSurveyAndDownloadButton_ = false;
  }

  private onDownloadClick_() {
    this.$.dialog.close();
    assert(!this.$.dialog.open);
    this.dispatchEvent(
        new CustomEvent('close', {bubbles: true, composed: true}));
  }

  private onKeydown_(e: KeyboardEvent) {
    if (e.key === 'Escape') {
      e.preventDefault();
    }
  }

  private removeKeydownListener_() {
    if (!this.boundKeydown_) {
      return;
    }

    this.removeEventListener('keydown', this.boundKeydown_);
    document.body.removeEventListener('keydown', this.boundKeydown_);
    this.boundKeydown_ = null;
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
