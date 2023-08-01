// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-guide-search-suggestions-fragment' is the fragment in a privacy
 * guide card that contains the search suggestions setting with a two-column
 * description.
 */
import '/shared/settings/controls/settings_toggle_button.js';
import './privacy_guide_description_item.js';
import './privacy_guide_fragment_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_guide_search_suggestions_fragment.html.js';

export class PrivacyGuideSearchSuggestionsFragmentElement extends
    PolymerElement {
  static get is() {
    return 'privacy-guide-search-suggestions-fragment';
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

  override focus() {
    // The fragment element is focused when it becomes visible. Move the focus
    // to the fragment header, so that the newly shown content of the fragment
    // is downwards from the focus position. This allows users of screen readers
    // to continue navigating the screen reader position downwards through the
    // newly visible content.
    this.shadowRoot!.querySelector<HTMLElement>('[focus-element]')!.focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-guide-search-suggestions-fragment':
        PrivacyGuideSearchSuggestionsFragmentElement;
  }
}

customElements.define(
    PrivacyGuideSearchSuggestionsFragmentElement.is,
    PrivacyGuideSearchSuggestionsFragmentElement);
