// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-review-welcome-fragment' is the fragment in a privacy review
 * card that contains the welcome screen and its description.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import '../../controls/settings_checkbox.js';
import './privacy_review_fragment_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export interface PrivacyReviewWelcomeFragmentElement {
  $: {
    startButton: HTMLElement,
  };
}

export class PrivacyReviewWelcomeFragmentElement extends PolymerElement {
  static get is() {
    return 'privacy-review-welcome-fragment';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },
    };
  }

  private onStartButtonClick_(e: Event) {
    e.stopPropagation();
    this.shadowRoot!.querySelector('settings-checkbox')!.sendPrefChange();
    this.dispatchEvent(
        new CustomEvent('start-button-click', {bubbles: true, composed: true}));
  }
}

customElements.define(
    PrivacyReviewWelcomeFragmentElement.is,
    PrivacyReviewWelcomeFragmentElement);
