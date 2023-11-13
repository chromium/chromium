// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-hub-camera-subpage' contains a detailed overview about the
 * state of the system camera access.
 */

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_hub_camera_subpage.html.js';

export class SettingsPrivacyHubCameraSubpage extends PolymerElement {
  static get is() {
    return 'settings-privacy-hub-camera-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPrivacyHubCameraSubpage.is]: SettingsPrivacyHubCameraSubpage;
  }
}

customElements.define(
    SettingsPrivacyHubCameraSubpage.is, SettingsPrivacyHubCameraSubpage);
