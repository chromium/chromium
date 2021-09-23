// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class DiagnosticsStickyBannerElement extends PolymerElement {
  static get is() {
    return 'diagnostics-sticky-banner';
  }

  static get template() {
    return html`{__html_template__}`
  }

  static get properties() {
    return {
      /** @type {string} */
      bannerMessage: {
        type: String,
        value: '',
        notify: true,
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /**
     * Event callback for 'show-caution-banner' which is triggered from routine-
     * section. Event will contain message to display on message property of
     * event found on path `event.detail.message`.
     * @private {?Function}
     */
    this.showCautionBannerHandler_ = (/** @type {!CustomEvent} */ e) => {
      assert(e.detail.message);

      this.bannerMessage = e.detail.message;
    };

    /**
     * Event callback for 'dismiss-caution-banner' which is triggered from
     * routine-section.
     * @private {?Function}
     */
    this.dismissCautionBannerHandler_ = () => {
      this.bannerMessage = '';
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    window.addEventListener(
        'show-caution-banner', this.showCautionBannerHandler_);
    window.addEventListener(
        'dismiss-caution-banner', this.dismissCautionBannerHandler_);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener(
        'show-caution-banner', this.showCautionBannerHandler_);
    window.removeEventListener(
        'dismiss-caution-banner', this.dismissCautionBannerHandler_);
  }
};

customElements.define(
    DiagnosticsStickyBannerElement.is, DiagnosticsStickyBannerElement);
