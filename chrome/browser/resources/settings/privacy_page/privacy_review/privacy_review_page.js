// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-review-page' is the settings page that helps users review
 * various privacy settings.
 */
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
export class SettingsPrivacyReviewPageElement extends PolymerElement {
  static get is() {
    return 'settings-privacy-review-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(
    SettingsPrivacyReviewPageElement.is, SettingsPrivacyReviewPageElement);
