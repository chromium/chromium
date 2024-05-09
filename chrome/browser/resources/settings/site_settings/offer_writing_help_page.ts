// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../controls/settings_toggle_button.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './offer_writing_help_page.html.js';

export const COMPOSE_PROACTIVE_NUDGE_PREF = 'compose.proactive_nudge_enabled';

const SettingsOfferWritingHelpPageElementBase = PrefsMixin(PolymerElement);

export class SettingsOfferWritingHelpPageElement extends
    SettingsOfferWritingHelpPageElementBase {
  static get is() {
    return 'settings-offer-writing-help-page';
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
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-offer-writing-help-page': SettingsOfferWritingHelpPageElement;
  }
}

customElements.define(
    SettingsOfferWritingHelpPageElement.is,
    SettingsOfferWritingHelpPageElement);
