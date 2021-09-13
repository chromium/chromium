// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-review-promo' is an element representing a promo for the
 * privacy review feature.
 */
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../route.js';
import {Router} from '../router.js';

export class PrivacyReviewPromoElement extends PolymerElement {
  static get is() {
    return 'settings-privacy-review-promo';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  private onPrivacyReviewClick_() {
    Router.getInstance().navigateTo(routes.PRIVACY_REVIEW);
  }
}

customElements.define(PrivacyReviewPromoElement.is, PrivacyReviewPromoElement);
