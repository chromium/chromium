// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-guide-clear-on-exit' is the fragment in a privacy guide card
 * that contains the 'clear cookies on exit' setting and its description.
 */
import '../../controls/settings_toggle_button.js';
import '../../prefs/prefs.js';
import './privacy_guide_description_item.js';
import './privacy_guide_fragment_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';

import {getTemplate} from './privacy_guide_clear_on_exit_fragment.html.js';

export class PrivacyGuideClearOnExitFragmentElement extends PolymerElement {
  static get is() {
    return 'privacy-guide-clear-on-exit-fragment';
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

      enablePrivacyGuide2_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('privacyGuide2Enabled'),
      },
    };
  }

  override focus() {
    this.shadowRoot!.querySelector<HTMLElement>('[focus-element]')!.focus();
  }
}

customElements.define(
    PrivacyGuideClearOnExitFragmentElement.is,
    PrivacyGuideClearOnExitFragmentElement);
