// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-guide-promo' is an element representing a promo for the
 * privacy guide feature.
 */
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {MetricsBrowserProxy} from '../../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, PrivacyGuideInteractions} from '../../metrics_browser_proxy.js';
import {routes} from '../../route.js';
import {Router} from '../../router.js';

import {getTemplate} from './privacy_guide_promo.html.js';

const PrivacyGuidePromoElementBase = PrefsMixin(PolymerElement);

export class PrivacyGuidePromoElement extends PrivacyGuidePromoElementBase {
  static get is() {
    return 'settings-privacy-guide-promo';
  }

  static get template() {
    return getTemplate();
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

  private onPrivacyGuideStartClick_() {
    this.metricsBrowserProxy_.recordAction('Settings.PrivacyGuide.StartPromo');
    this.metricsBrowserProxy_.recordPrivacyGuideEntryExitHistogram(
        PrivacyGuideInteractions.PROMO_ENTRY);
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
  }

  private onNoThanksButtonClick_() {
    this.setPrefValue('privacy_guide.viewed', true);
  }
}

customElements.define(PrivacyGuidePromoElement.is, PrivacyGuidePromoElement);
