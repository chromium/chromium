// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-privacy-hub-page' contains privacy hub configurations.
 */

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class SettingsPrivacyHubPage extends PolymerElement {
  static get is() {
    return 'settings-privacy-hub-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /** @override */
  ready() {
    super.ready();
    assert(loadTimeData.getBoolean('showPrivacyHub'));
  }
}

customElements.define(SettingsPrivacyHubPage.is, SettingsPrivacyHubPage);
