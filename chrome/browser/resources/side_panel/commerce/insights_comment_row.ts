// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './insights_comment_row.css.js';
import {getHtml} from './insights_comment_row.html.js';
import {PriceInsightsBrowserProxyImpl} from './price_insights_browser_proxy.js';
import type {PriceInsightsBrowserProxy} from './price_insights_browser_proxy.js';

export class InsightsCommentRowElement extends CrLitElement {
  static get is() {
    return 'insights-comment-row';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      shouldShowFeedback_: {type: Boolean},
    };
  }

  protected accessor shouldShowFeedback_: boolean =
      loadTimeData.getBoolean('shouldShowFeedback');
  private priceInsightsProxy_: PriceInsightsBrowserProxy =
      PriceInsightsBrowserProxyImpl.getInstance();

  protected onFeedbackClick_() {
    this.priceInsightsProxy_.showFeedback();
    chrome.metricsPrivate.recordUserAction(
        'Commerce.PriceInsights.InlineFeedbackLinkClicked');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'insights-comment-row': InsightsCommentRowElement;
  }
}

customElements.define(InsightsCommentRowElement.is, InsightsCommentRowElement);
