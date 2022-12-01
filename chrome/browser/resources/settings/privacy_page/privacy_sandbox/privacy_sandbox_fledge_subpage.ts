// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../prefs/prefs.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefsMixin} from '../../prefs/prefs_mixin.js';

import {getTemplate} from './privacy_sandbox_fledge_subpage.html.js';

const SettingsPrivacySandboxFledgeSubpageElementBase =
    PrefsMixin(PolymerElement);

export class SettingsPrivacySandboxFledgeSubpageElement extends
    SettingsPrivacySandboxFledgeSubpageElementBase {
  static get is() {
    return 'settings-privacy-sandbox-fledge-subpage';
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
    'settings-privacy-sandbox-fledge-subpage':
        SettingsPrivacySandboxFledgeSubpageElement;
  }
}

customElements.define(
    SettingsPrivacySandboxFledgeSubpageElement.is,
    SettingsPrivacySandboxFledgeSubpageElement);
