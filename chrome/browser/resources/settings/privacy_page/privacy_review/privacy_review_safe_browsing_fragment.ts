// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-review-safe-browsing-fragment' is the fragment in a privacy
 * review card that contains the safe browsing settings and their descriptions.
 */
import '../../prefs/prefs.js';
import './privacy_review_description_item.js';
import './privacy_review_fragment_shared_css.js';
import '../../controls/settings_radio_group.js';
import '../../privacy_page/collapse_radio_button.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SafeBrowsingSetting} from '../../privacy_page/security_page.js';

export class PrivacyReviewSafeBrowsingFragmentElement extends PolymerElement {
  static get is() {
    return 'privacy-review-safe-browsing-fragment';
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

      /**
       * Valid safe browsing states.
       */
      safeBrowsingSettingEnum_: {
        type: Object,
        value: SafeBrowsingSetting,
      },
    };
  }
}

customElements.define(
    PrivacyReviewSafeBrowsingFragmentElement.is,
    PrivacyReviewSafeBrowsingFragmentElement);