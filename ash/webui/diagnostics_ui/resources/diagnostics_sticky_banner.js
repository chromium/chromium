// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './diagnostics_shared_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class DiagnosticsStickyBannerElement extends PolymerElement {
  static get is() {
    return 'diagnostics-sticky-banner';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {string} */
      bannerMessage: {
        type: String,
        value: '',
        notify: true,
      },

      /** @protected {string} */
      scrollingClass_: {
        type: String,
        value: '',
      },

      /** @private */
      scrollTimerId_: {
        type: Number,
        value: -1,
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

    /**
     * Event callback for 'scroll'.
     * @private {?Function}
     */
    this.scrollClassHandler_ = () => {
      this.onScroll_();
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    window.addEventListener(
        'show-caution-banner', this.showCautionBannerHandler_);
    window.addEventListener(
        'dismiss-caution-banner', this.dismissCautionBannerHandler_);
    window.addEventListener('scroll', this.scrollClassHandler_);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener(
        'show-caution-banner', this.showCautionBannerHandler_);
    window.removeEventListener(
        'dismiss-caution-banner', this.dismissCautionBannerHandler_);
    window.removeEventListener('scroll', this.scrollClassHandler_);
  }

  /**
   * Event handler for 'scroll' to ensure shadow and elevation of banner is
   * correct while scrolling. Timer is used to clear class after 300ms.
   * @private
   */
  onScroll_() {
    if (!this.bannerMessage) {
      return;
    }

    // Reset timer since we've received another 'scroll' event.
    if (this.scrollTimerId_ !== -1) {
      this.scrollingClass_ = 'elevation-2';
      clearTimeout(this.scrollTimerId_);
    }

    // Remove box shadow from banner since the user has stopped scrolling
    // for at least 300ms.
    this.scrollTimerId_ =
        window.setTimeout(() => this.scrollingClass_ = '', 300);
  }
}

customElements.define(
    DiagnosticsStickyBannerElement.is, DiagnosticsStickyBannerElement);
