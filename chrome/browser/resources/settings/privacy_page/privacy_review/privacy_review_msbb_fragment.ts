// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-review-msbb-fragment' is the fragment in a privacy review card
 * that contains the MSBB setting with a two-column description.
 */
import '../../controls/settings_toggle_button.js';
import '../../prefs/prefs.js';
import './privacy_review_description_item.js';
import './privacy_review_fragment_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MetricsBrowserProxy, MetricsBrowserProxyImpl, PrivacyGuideSettingsStates} from '../../metrics_browser_proxy.js';
import {PrefsMixin} from '../../prefs/prefs_mixin.js';

const PrivacyReviewMsbbFragmentBase = PrefsMixin(PolymerElement);

export class PrivacyReviewMsbbFragmentElement extends
    PrivacyReviewMsbbFragmentBase {
  static get is() {
    return 'privacy-review-msbb-fragment';
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
  private startStateMsbbOn_: boolean;

  ready() {
    super.ready();
    this.addEventListener('view-enter-start', this.onViewEnterStart_);
    this.addEventListener('view-exit-finish', this.onViewExitFinish_);
  }

  private onViewEnterStart_() {
    this.startStateMsbbOn_ =
        this.getPref('url_keyed_anonymized_data_collection.enabled').value;
  }

  private onViewExitFinish_() {
    const endStateMsbbOn =
        this.getPref('url_keyed_anonymized_data_collection.enabled').value;

    let state: PrivacyGuideSettingsStates|null = null;
    if (this.startStateMsbbOn_) {
      state = endStateMsbbOn ? PrivacyGuideSettingsStates.MSBB_ON_TO_ON :
                               PrivacyGuideSettingsStates.MSBB_ON_TO_OFF;
    } else {
      state = endStateMsbbOn ? PrivacyGuideSettingsStates.MSBB_OFF_TO_ON :
                               PrivacyGuideSettingsStates.MSBB_OFF_TO_OFF;
    }
    this.metricsBrowserProxy_.recordPrivacyGuideSettingsStatesHistogram(state!);
  }
}

customElements.define(
    PrivacyReviewMsbbFragmentElement.is, PrivacyReviewMsbbFragmentElement);
