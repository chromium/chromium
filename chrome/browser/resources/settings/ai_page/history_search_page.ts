// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../controls/settings_toggle_button.js';
import '../settings_columned_section.css.js';
import '../settings_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './history_search_page.html.js';

// These values must stay in sync with
// optimization_guide::prefs::FeatureOptInState in
// components/optimization_guide/core/optimization_guide_prefs.h.
export enum FeatureOptInState {
  NOT_INITIALIZED = 0,
  ENABLED = 1,
  DISABLED = 2,
}

export class SettingsHistorySearchPageElement extends PolymerElement {
  static get is() {
    return 'settings-history-search-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      featureOptInStateEnum_: {
        type: Object,
        value: FeatureOptInState,
      },

      numericUncheckedValues_: {
        type: Array,
        value: () =>
            [FeatureOptInState.DISABLED, FeatureOptInState.NOT_INITIALIZED],
      },

      prefs: Object,
    };
  }

  private numericUncheckedValues_: FeatureOptInState[];
  prefs: {[key: string]: any};
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-history-search-page': SettingsHistorySearchPageElement;
  }
}

customElements.define(
    SettingsHistorySearchPageElement.is, SettingsHistorySearchPageElement);
