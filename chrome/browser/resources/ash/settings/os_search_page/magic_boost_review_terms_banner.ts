// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'magic-boost-review-terms-banner' is an element to display
 * an option for users to review the terms.
 *
 */
import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MagicBoostNoticeBrowserProxy} from '../os_search_page/magic_boost_browser_proxy.js';

import {getTemplate} from './magic_boost_review_terms_banner.html.js';

export class MagicBoostReviewTermsBanner extends PolymerElement {
  private magicBoostNoticeProxy_: MagicBoostNoticeBrowserProxy;

  constructor() {
    super();
    this.magicBoostNoticeProxy_ = MagicBoostNoticeBrowserProxy.getInstance();
  }
  static get is() {
    return 'magic-boost-review-terms-banner';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      description: {
        type: String,
      },

      buttonLabel: {
        type: String,
      },
    };
  }
  description: string;
  buttonLabel: string;

  private onReviewButtonClick_(): void {
    this.magicBoostNoticeProxy_.showNotice();
  }
}

customElements.define(
    MagicBoostReviewTermsBanner.is, MagicBoostReviewTermsBanner);

declare global {
  interface HTMLElementTagNameMap {
    'magic-boost-review-terms-banner': MagicBoostReviewTermsBanner;
  }
}
