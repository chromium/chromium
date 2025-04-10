// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './incognito_tracking_protections_page.html.js';

export class IncognitoTrackingProtectionsPageElement extends PolymerElement {
  static get is() {
    return 'incognito-tracking-protections-page';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'incognito-tracking-protections-page': IncognitoTrackingProtectionsPageElement;
  }
}

customElements.define(
    IncognitoTrackingProtectionsPageElement.is, IncognitoTrackingProtectionsPageElement);
