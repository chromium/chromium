// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-review-two-state-settings-fragment' is the fragment in a privacy
 * review card that contains an embedded two-state setting and a two-column
 * description of the setting.
 */
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import '../../settings_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
export class PrivacyReviewTwoStateSettingFragmentElement extends
    PolymerElement {
  static get is() {
    return 'privacy-review-two-state-setting-fragment';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      featureHeader: {
        type: String,
        value: '',
      },

      privacyHeader: {
        type: String,
        value: '',
      },
    };
  }
}

customElements.define(
    PrivacyReviewTwoStateSettingFragmentElement.is,
    PrivacyReviewTwoStateSettingFragmentElement);
