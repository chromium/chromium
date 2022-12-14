// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage for managing Hotspot.
 */

import './internet_shared.css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class SettingsHotspotSubpageElement extends PolymerElement {
  static get is() {
    return 'settings-hotspot-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(
    SettingsHotspotSubpageElement.is, SettingsHotspotSubpageElement);
