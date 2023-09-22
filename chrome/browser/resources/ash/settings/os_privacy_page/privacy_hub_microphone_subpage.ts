// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-hub-microphone-subpage' contains a detailed overview about
 * the state of the system microphone access.
 */

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_hub_microphone_subpage.html.js';

export class SettingsPrivacyHubMicrophoneSubpage extends PolymerElement {
  static get is() {
    return 'settings-privacy-hub-microphone-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPrivacyHubMicrophoneSubpage.is]:
        SettingsPrivacyHubMicrophoneSubpage;
  }
}

customElements.define(
    SettingsPrivacyHubMicrophoneSubpage.is,
    SettingsPrivacyHubMicrophoneSubpage);
