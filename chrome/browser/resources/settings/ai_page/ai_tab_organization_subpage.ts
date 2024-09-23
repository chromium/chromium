// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './ai_tab_organization_subpage.html.js';
import {FeatureOptInState} from './constants.js';

const SettingsAiTabOrganizationSubpageElementBase = PrefsMixin(PolymerElement);

export class SettingsAiTabOrganizationSubpageElement extends
    SettingsAiTabOrganizationSubpageElementBase {
  static get is() {
    return 'settings-ai-tab-organization-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      featureOptInStateEnum_: {
        type: Object,
        value: FeatureOptInState,
      },

      numericUncheckedValues_: {
        type: Array,
        value: () =>
            [FeatureOptInState.DISABLED, FeatureOptInState.NOT_INITIALIZED],
      },
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-tab-organization-subpage':
        SettingsAiTabOrganizationSubpageElement;
  }
}

customElements.define(
    SettingsAiTabOrganizationSubpageElement.is,
    SettingsAiTabOrganizationSubpageElement);
