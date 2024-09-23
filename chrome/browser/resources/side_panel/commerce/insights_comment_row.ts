// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import type {BrowserProxy} from '//resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from '//resources/cr_components/commerce/browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './insights_comment_row.html.js';

export class InsightsCommentRow extends PolymerElement {
  static get is() {
    return 'insights-comment-row';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      shouldShowFeedback_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('shouldShowFeedback'),
      },
    };
  }

  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();

  private showFeedback_() {
    this.shoppingApi_.showFeedbackForPriceInsights();
    chrome.metricsPrivate.recordUserAction(
        'Commerce.PriceInsights.InlineFeedbackLinkClicked');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'insights-comment-row': InsightsCommentRow;
  }
}

customElements.define(InsightsCommentRow.is, InsightsCommentRow);
