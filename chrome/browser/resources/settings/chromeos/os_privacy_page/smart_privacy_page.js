// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-smart-privacy-page' contains smart privacy settings.
 */

import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '../../controls/extension_controlled_indicator.js';
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared_css.js';
import '../../settings_vars_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefsBehavior} from '../prefs_behavior.js';

/** @polymer */
class SettingsSmartPrivacyPage extends PolymerElement {
  static get is() {
    return 'settings-smart-privacy-page';
  }

  static get template() {
    return html`{__html_template__}`;
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

customElements.define(SettingsSmartPrivacyPage.is, SettingsSmartPrivacyPage);
