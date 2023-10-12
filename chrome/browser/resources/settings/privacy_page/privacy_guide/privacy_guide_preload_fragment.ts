// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-guide-preload-fragment' is the fragment in a privacy
 * guide card that contains the preload settings and their descriptions.
 */
import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import './privacy_guide_description_item.js';
import './privacy_guide_fragment_shared.css.js';
import '/shared/settings/controls/settings_radio_group.js';
import '../../privacy_page/collapse_radio_button.js';

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NetworkPredictionOptions} from '../../performance_page/constants.js';

import {getTemplate} from './privacy_guide_preload_fragment.html.js';

const PrivacyGuidePreloadFragmentBase = PrefsMixin(PolymerElement);

export class PrivacyGuidePreloadFragmentElement extends
    PrivacyGuidePreloadFragmentBase {
  static get is() {
    return 'privacy-guide-preload-fragment';
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

      /** Valid network prediction options state. */
      networkPredictionOptionsEnum_: {
        type: Object,
        value: NetworkPredictionOptions,
      },
    };
  }

  override focus() {
    // The fragment element is focused when it becomes visible. Move the focus
    // to the fragment header, so that the newly shown content of the fragment
    // is downwards from the focus position. This allows users of screen readers
    // to continue navigating the screen reader position downwards through the
    // newly visible content.
    const focusElement =
        this.shadowRoot!.querySelector<HTMLElement>('[focus-element]');
    assert(focusElement);
    focusElement.focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-guide-preload-fragment': PrivacyGuidePreloadFragmentElement;
  }
}

customElements.define(
    PrivacyGuidePreloadFragmentElement.is, PrivacyGuidePreloadFragmentElement);
