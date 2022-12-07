// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../controls/settings_toggle_button.js';
import '../../prefs/prefs.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {PrefsMixin} from '../../prefs/prefs_mixin.js';

import {getTemplate} from './privacy_sandbox_topics_subpage.html.js';

export interface SettingsPrivacySandboxTopicsSubpageElement {
  $: {
    topicsToggle: SettingsToggleButtonElement,
  };
}

const SettingsPrivacySandboxTopicsSubpageElementBase =
    PrefsMixin(PolymerElement);

export class SettingsPrivacySandboxTopicsSubpageElement extends
    SettingsPrivacySandboxTopicsSubpageElementBase {
  static get is() {
    return 'settings-privacy-sandbox-topics-subpage';
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

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-sandbox-topics-subpage':
        SettingsPrivacySandboxTopicsSubpageElement;
  }
}

customElements.define(
    SettingsPrivacySandboxTopicsSubpageElement.is,
    SettingsPrivacySandboxTopicsSubpageElement);
