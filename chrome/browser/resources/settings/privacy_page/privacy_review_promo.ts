// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-review-promo' is an element representing a promo for the
 * privacy review feature.
 */
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MetricsBrowserProxy, MetricsBrowserProxyImpl, PrivacyGuideInteractions} from '../metrics_browser_proxy.js';
import {PrefsMixin} from '../prefs/prefs_mixin.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

const PrivacyReviewPromoElementBase = PrefsMixin(PolymerElement);

export class PrivacyReviewPromoElement extends PrivacyReviewPromoElementBase {
  static get is() {
    return 'settings-privacy-review-promo';
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

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  private onPrivacyReviewStartClick_() {
    this.metricsBrowserProxy_.recordAction('Settings.PrivacyGuide.StartPromo');
    this.metricsBrowserProxy_.recordPrivacyGuideEntryExitHistogram(
        PrivacyGuideInteractions.PROMO_ENTRY);
    Router.getInstance().navigateTo(routes.PRIVACY_REVIEW);
  }

  private onNoThanksButtonClick_() {
    this.setPrefValue('privacy_guide.viewed', true);
  }
}

customElements.define(PrivacyReviewPromoElement.is, PrivacyReviewPromoElement);
