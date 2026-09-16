// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-guide-promo' is an element representing a promo for the
 * privacy guide feature.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {MetricsBrowserProxy} from '../../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, PrivacyGuideInteractions} from '../../metrics_browser_proxy.js';
import {routes} from '../../route.js';
import {Router} from '../../router.js';

import {getCss} from './privacy_guide_promo.css.js';
import {getHtml} from './privacy_guide_promo.html.js';

export class PrivacyGuidePromoElement extends CrLitElement {
  static get is() {
    return 'settings-privacy-guide-promo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  protected onPrivacyGuideStartClick_() {
    this.metricsBrowserProxy_.recordAction('Settings.PrivacyGuide.StartPromo');
    this.metricsBrowserProxy_.recordPrivacyGuideEntryExitHistogram(
        PrivacyGuideInteractions.PROMO_ENTRY);
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
  }

  protected onNoThanksButtonClick_() {
    PrefService.getInstance().setPrefValue('privacy_guide.viewed', true);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-guide-promo': PrivacyGuidePromoElement;
  }
}

customElements.define(PrivacyGuidePromoElement.is, PrivacyGuidePromoElement);
