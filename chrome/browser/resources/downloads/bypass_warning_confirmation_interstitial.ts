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

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {getTemplate} from './bypass_warning_confirmation_interstitial.html.js';
import type {PageHandlerInterface} from './downloads.mojom-webui.js';
import {DangerousDownloadInterstitialSurveyOptions as surveyOptions} from './downloads.mojom-webui.js';

export interface DownloadsDangerousDownloadInterstitialElement {
  $: {
    dialog: HTMLDialogElement,
    continueAnywayButton: CrButtonElement,
    backToSafetyButton: CrButtonElement,
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
      bypassPromptItemId: String,

      hideSurveyAndDownloadButton_: {
        type: Boolean,
        value: true,
      },

      selectedRadioOption_: String,

      trustSiteLine: String,

      trustSiteLineAccessibleText: String,
    };
  }

  bypassPromptItemId: string;
  trustSiteLine: string;
  trustSiteLineAccessibleText: string;

  private boundKeydown_: ((e: KeyboardEvent) => void)|null = null;
  private hideSurveyAndDownloadButton_: boolean;
  private selectedRadioOption_: string;

  private mojoHandler_: PageHandlerInterface|null = null;

  override ready() {
    super.ready();
    this.mojoHandler_ = BrowserProxy.getInstance().handler;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.$.dialog.showModal();
    this.$.dialog.focus();
    this.disableEscapeKey_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.removeKeydownListener_();
  }

  getSurveyResponse(): surveyOptions {
    const surveyResponse = this.$.dialog.returnValue;
    switch (surveyResponse) {
      case 'CreatedFile':
        return surveyOptions.kCreatedFile;
      case 'TrustSite':
        return surveyOptions.kTrustSite;
      case 'AcceptRisk':
        return surveyOptions.kAcceptRisk;
      default:
        return surveyOptions.kNoResponse;
    }
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

    const backToSafetyButton = this.$.backToSafetyButton;
    assert(!!backToSafetyButton);
    backToSafetyButton.focus();
    this.hideSurveyAndDownloadButton_ = false;

    assert(this.bypassPromptItemId !== '');
    assert(!!this.mojoHandler_);
    this.mojoHandler_.recordOpenSurveyOnDangerousInterstitial(
        this.bypassPromptItemId);
  }

  private onDownloadClick_() {
    getAnnouncerInstance().announce(
        loadTimeData.getString('screenreaderSavedDangerous'));

    this.$.dialog.close(this.selectedRadioOption_);
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

  private onSelectedRadioOptionChanged_(e: CustomEvent<{value: string}>) {
    this.selectedRadioOption_ = e.detail.value;
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
