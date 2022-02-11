// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-review-clear-on-exit' is the fragment in a privacy review card
 * that contains the 'clear cookies on exit' setting and its description.
 */
import '../../controls/settings_toggle_button.js';
import '../../prefs/prefs.js';
import './privacy_review_description_item.js';
import './privacy_review_fragment_shared_css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getTemplate} from './privacy_review_clear_on_exit_fragment.html.js';

export class PrivacyReviewClearOnExitFragmentElement extends PolymerElement {
  static get is() {
    return 'privacy-review-clear-on-exit-fragment';
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
}

customElements.define(
    PrivacyReviewClearOnExitFragmentElement.is,
    PrivacyReviewClearOnExitFragmentElement);
